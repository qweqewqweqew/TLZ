#include "Ros2Bridge.h"

#include "ShmImageReader.h"

#include <mz_interfaces/msg/algorithm_result.hpp>
#include <mz_interfaces/msg/scan_result.hpp>
#include <rclcpp/executors/single_threaded_executor.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>

namespace {

constexpr char kNodeName[] = "mz_tlz";
constexpr char kScanCompleteTopic[] = "scan_complete";
constexpr char kAlgorithmResultTopic[] = "algorithm_result";
constexpr char kBackendStateTopic[] = "backend_state";
constexpr char kTeachRequestTopic[] = "teach_request";

void emitScanSegment(Ros2Bridge *bridge,
                     const mz_interfaces::msg::ScanResult &message,
                     std::uint8_t imageType,
                     std::uint64_t offset,
                     std::uint64_t size)
{
    emit bridge->scanResultReceived(imageType,
                                    message.frame_id,
                                    QString::fromStdString(message.shm_name),
                                    offset,
                                    size,
                                    message.width,
                                    message.height,
                                    message.pixel_format);

    emit bridge->infoMessage(QString("收到相机帧: type=%1 frame=%2 shm=%3 offset=%4 size=%5 %6x%7 fmt=%8")
                                 .arg(imageType)
                                 .arg(message.frame_id)
                                 .arg(QString::fromStdString(message.shm_name))
                                 .arg(offset)
                                 .arg(size)
                                 .arg(message.width)
                                 .arg(message.height)
                                 .arg(message.pixel_format));
}

void emitScanResult(Ros2Bridge *bridge, const mz_interfaces::msg::ScanResult &message)
{
    if (message.range_size > 0) {
        emitScanSegment(bridge, message, 0, 0, message.range_size);
    }
    if (message.intensity_size > 0) {
        emitScanSegment(bridge, message, 1, message.range_size, message.intensity_size);
    }
}

// ---------- UI 侧的显示适配：把 SHM 原始字节转成 QImage ----------
// 这些函数只服务于本 GUI 的显示需求，跟 ShmImageReader 无关。
// 算法端/存储端拿到的是同一份 RawSlice，怎么解读是他们各自的事。

// Mono16 距离图 → Grayscale8：按有效像素 (v != 0) 的 min/max 线性拉伸。
QImage stretchMono16ToGrayscale8(const RawSlice &s, int width, int height)
{
    if (s.empty() || width <= 0 || height <= 0) return QImage();
    const std::size_t pixels = std::size_t(width) * std::size_t(height);
    if (s.size < pixels * 2) return QImage();

    const auto *src = reinterpret_cast<const std::uint16_t *>(s.data);

    std::uint16_t lo = std::numeric_limits<std::uint16_t>::max();
    std::uint16_t hi = 0;
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::uint16_t v = src[i];
        if (v == 0) continue;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }

    QImage img(width, height, QImage::Format_Grayscale8);
    if (lo >= hi) {
        img.fill(0);
        return img;
    }
    const std::uint32_t span = std::uint32_t(hi) - std::uint32_t(lo);
    for (int y = 0; y < height; ++y) {
        uchar *dst = img.scanLine(y);
        const std::uint16_t *row = src + std::size_t(y) * width;
        for (int x = 0; x < width; ++x) {
            const std::uint16_t v = row[x];
            dst[x] = (v == 0) ? 0
                : static_cast<uchar>((std::uint32_t(v) - lo) * 255U / span);
        }
    }
    return img;
}

// Mono8 强度图 → Grayscale8：按行 memcpy，一比一。
QImage wrapMono8ToGrayscale8(const RawSlice &s, int width, int height)
{
    if (s.empty() || width <= 0 || height <= 0) return QImage();
    const std::size_t pixels = std::size_t(width) * std::size_t(height);
    if (s.size < pixels) return QImage();

    QImage img(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        std::memcpy(img.scanLine(y), s.data + std::size_t(y) * width, width);
    }
    return img;
}

} // namespace

struct Ros2BridgeEntities final
{
    rclcpp::Subscription<mz_interfaces::msg::ScanResult>::SharedPtr scanResult;
    rclcpp::Subscription<mz_interfaces::msg::AlgorithmResult>::SharedPtr algorithmResult;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr backendState;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr teachRequest;
};

Ros2Bridge::Ros2Bridge(QObject *parent)
    : QObject(parent)
{
}

Ros2Bridge::~Ros2Bridge()
{
    stop();
}

bool Ros2Bridge::start()
{
    if (m_running) {
        return true;
    }

    if (!rclcpp::ok()) {
        emit errorMessage("ROS2 未初始化，无法启动通信节点");
        return false;
    }

    try {
        m_node = std::make_shared<rclcpp::Node>(kNodeName);
        m_entities = std::make_shared<Ros2BridgeEntities>();
        const auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();

        m_entities->scanResult = m_node->create_subscription<mz_interfaces::msg::ScanResult>(
            kScanCompleteTopic,
            qos,
            [this](const mz_interfaces::msg::ScanResult::SharedPtr msg) {
                if (!msg) return;

                // 1. 先按原有逻辑广播每段的元数据（用于事件日志）
                emitScanResult(this, *msg);

                // 2. 若 frame_id == uint64(-1) 说明相机端标记失败，跳过读取
                constexpr std::uint64_t kInvalidFrame = ~std::uint64_t(0);
                if (msg->frame_id == kInvalidFrame) {
                    emit errorMessage(
                        QString("相机上报采集失败: frame_id=-1 shm=%1")
                            .arg(QString::fromStdString(msg->shm_name)));
                    return;
                }
                if (msg->range_size == 0 && msg->intensity_size == 0) {
                    return;
                }

                // 3. 打开/复用共享内存视图，切出两段字节
                std::lock_guard<std::mutex> lock(m_readerMutex);
                if (!m_shmReader) {
                    m_shmReader = std::make_unique<ShmImageReader>();
                }

                std::string err;
                if (!m_shmReader->open(msg->shm_name, &err)) {
                    emit errorMessage(QString("打开共享内存失败: %1")
                                          .arg(QString::fromStdString(err)));
                    return;
                }

                RawSlice rangeSlice;
                RawSlice intensitySlice;
                err.clear();
                m_shmReader->sliceScanFrame(msg->range_size,
                                            msg->intensity_size,
                                            &rangeSlice,
                                            &intensitySlice,
                                            &err);
                if (!err.empty()) {
                    emit errorMessage(QString("切分共享内存失败: %1")
                                          .arg(QString::fromStdString(err)));
                }

                // 4. UI 侧适配：把原始字节解码成用于显示的 QImage。
                //    RawSlice 指向 mapping view 内部，只在本作用域内使用，
                //    stretchMono16ToGrayscale8 / wrapMono8ToGrayscale8 内部
                //    会把像素拷贝进 QImage，之后就与 SHM 无关了。
                const QImage rangeImg = stretchMono16ToGrayscale8(
                    rangeSlice, int(msg->width), int(msg->height));
                const QImage intensityImg = wrapMono8ToGrayscale8(
                    intensitySlice, int(msg->width), int(msg->height));

                if (!rangeImg.isNull() || !intensityImg.isNull()) {
                    emit scanFrameReady(rangeImg,
                                        intensityImg,
                                        msg->frame_id,
                                        msg->timestamp_ns,
                                        msg->width,
                                        msg->height,
                                        msg->pixel_format);
                }
            });

        m_entities->algorithmResult = m_node->create_subscription<mz_interfaces::msg::AlgorithmResult>(
            kAlgorithmResultTopic,
            qos,
            [this](const mz_interfaces::msg::AlgorithmResult::SharedPtr msg) {
                if (msg) {
                    handleAlgorithmResult(msg->success, msg->frame_id, msg->task_id, msg->message);
                }
            });

        m_entities->backendState = m_node->create_subscription<std_msgs::msg::String>(
            kBackendStateTopic,
            qos,
            [this](const std_msgs::msg::String::SharedPtr msg) {
                if (msg) {
                    handleBackendState(msg->data);
                }
            });

        m_entities->teachRequest = m_node->create_publisher<std_msgs::msg::String>(
            kTeachRequestTopic, qos);

        m_executor = std::make_unique<rclcpp::executors::SingleThreadedExecutor>();
        m_executor->add_node(m_node);

        m_running = true;
        m_spinThread = std::thread([this]() {
            try {
                m_executor->spin();
            } catch (const std::exception &ex) {
                emit errorMessage(QString("ROS2 executor 异常: %1").arg(QString::fromStdString(ex.what())));
            }
        });

        emit infoMessage("ROS2 通信节点已启动，监听 scan_complete / algorithm_result / backend_state");
        return true;
    } catch (const std::exception &ex) {
        emit errorMessage(QString("ROS2 通信节点启动失败: %1").arg(QString::fromStdString(ex.what())));
        stop();
        return false;
    }
}

void Ros2Bridge::stop()
{
    if (!m_running && !m_executor && !m_node) {
        return;
    }

    m_running = false;

    if (m_executor) {
        m_executor->cancel();
    }

    if (m_spinThread.joinable()) {
        m_spinThread.join();
    }

    if (m_executor && m_node) {
        try {
            m_executor->remove_node(m_node);
        } catch (const std::exception &) {
        }
    }

    m_executor.reset();
    m_node.reset();
    m_entities.reset();

    {
        std::lock_guard<std::mutex> lock(m_readerMutex);
        m_shmReader.reset();
    }

    emit infoMessage("ROS2 通信节点已停止");
}

bool Ros2Bridge::isRunning() const
{
    return m_running;
}

void Ros2Bridge::handleAlgorithmResult(bool success,
                                       std::uint64_t frameId,
                                       std::uint64_t taskId,
                                       const std::string &message)
{
    const QString text = QString::fromStdString(message);
    emit algorithmResultReceived(success, frameId, taskId, text);
    emit infoMessage(QString("收到后端结果: frame=%1 task=%2 success=%3 message=%4")
                         .arg(frameId)
                         .arg(taskId)
                         .arg(success ? "true" : "false")
                         .arg(text));
}

void Ros2Bridge::handleBackendState(const std::string &state)
{
    const QString text = QString::fromStdString(state);
    emit backendStateReceived(text);
    emit infoMessage(QString("收到后端状态: %1").arg(text));
}

void Ros2Bridge::publishTeachRequest()
{
    if (!m_running || !m_entities || !m_entities->teachRequest) {
        emit errorMessage("ROS2 节点未运行，无法发送示教取坐标请求");
        return;
    }
    std_msgs::msg::String msg;
    msg.data = "get_position";
    m_entities->teachRequest->publish(msg);
    emit infoMessage("已发送示教取坐标请求 (teach_request: get_position)");
}
