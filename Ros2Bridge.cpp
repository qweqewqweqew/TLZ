#include "Ros2Bridge.h"

#include "ShmImageReader.h"

#include <mz_interfaces/msg/camera_status.hpp>
#include <mz_interfaces/msg/milling_path.hpp>
#include <mz_interfaces/msg/milling_paths.hpp>
#include <mz_interfaces/msg/milling_progress.hpp>
#include <mz_interfaces/msg/plc_feedback.hpp>
#include <mz_interfaces/msg/plc_path_command_params.hpp>
#include <mz_interfaces/msg/scan_result.hpp>
#include <rclcpp/executors/single_threaded_executor.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include <QMetaType>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>

namespace {

constexpr char kNodeName[] = "mz_tlz";
constexpr char kCameraStatusTopic[] = "camera/status";
constexpr char kScanRangeTopic[] = "scan_range";
constexpr char kScanIntensityTopic[] = "scan_intensity";
constexpr char kBackendStateTopic[] = "backend_state";
constexpr char kTeachRequestTopic[] = "teach_request";
constexpr char kMillingPathsTopic[] = "milling/paths";
constexpr char kMillingProgressTopic[] = "milling/progress";
constexpr char kPlcFeedbackTopic[] = "plc/feedback";
constexpr char kPlcPathCommandParamsTopic[] = "plc_path_command_params";
constexpr std::uint64_t kInvalidFrame = ~std::uint64_t(0);
constexpr std::uint32_t kCv8Uc1 = 0;
constexpr std::uint32_t kCv16Uc1 = 2;
constexpr std::uint32_t kCv32Fc1 = 5;

void emitScanSegment(Ros2Bridge *bridge,
                     const mz_interfaces::msg::ScanResult &message,
                     std::uint8_t imageType,
                     std::uint64_t offset,
                     std::uint64_t size)
{
    emit bridge->scanResultReceived(imageType,
                                    message.frame_id,
                                    message.task_id,
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

// ---------- UI 侧的显示适配：把 SHM 原始字节转成 QImage ----------
// 这些函数只服务于本 GUI 的显示需求，跟 ShmImageReader 无关。
// 算法端/存储端拿到的是同一份 RawSlice，怎么解读是他们各自的事。

// Mono16 距离图 → Grayscale8：按有效像素 (v != 0) 的 min/max 线性拉伸。
bool hasValidImageLayout(const RawSlice &slice,
                         int width,
                         int height,
                         std::uint32_t stride,
                         std::size_t minimumRowBytes)
{
    if (slice.empty() || width <= 0 || height <= 0 || stride < minimumRowBytes) {
        return false;
    }
    return static_cast<std::size_t>(height) <= slice.size / stride;
}

QImage stretchMono16ToGrayscale8(const RawSlice &slice,
                                 int width,
                                 int height,
                                 std::uint32_t stride)
{
    const std::size_t rowBytes = static_cast<std::size_t>(width) * sizeof(std::uint16_t);
    if (!hasValidImageLayout(slice, width, height, stride, rowBytes)) return QImage();

    std::uint16_t lo = std::numeric_limits<std::uint16_t>::max();
    std::uint16_t hi = 0;
    for (int y = 0; y < height; ++y) {
        const auto *row = reinterpret_cast<const std::uint16_t *>(
            slice.data + static_cast<std::size_t>(y) * stride);
        for (int x = 0; x < width; ++x) {
            const std::uint16_t value = row[x];
            if (value == 0) continue;
            if (value < lo) lo = value;
            if (value > hi) hi = value;
        }
    }

    QImage img(width, height, QImage::Format_Grayscale8);
    if (lo >= hi) {
        img.fill(0);
        return img;
    }
    const std::uint32_t span = std::uint32_t(hi) - std::uint32_t(lo);
    for (int y = 0; y < height; ++y) {
        uchar *dst = img.scanLine(y);
        const auto *row = reinterpret_cast<const std::uint16_t *>(
            slice.data + static_cast<std::size_t>(y) * stride);
        for (int x = 0; x < width; ++x) {
            const std::uint16_t value = row[x];
            dst[x] = (value == 0) ? 0
                : static_cast<uchar>((std::uint32_t(value) - lo) * 255U / span);
        }
    }
    return img;
}

QImage stretchFloat32ToGrayscale8(const RawSlice &slice,
                                  int width,
                                  int height,
                                  std::uint32_t stride)
{
    const std::size_t rowBytes = static_cast<std::size_t>(width) * sizeof(float);
    if (!hasValidImageLayout(slice, width, height, stride, rowBytes)) return QImage();

    float lo = std::numeric_limits<float>::infinity();
    float hi = -std::numeric_limits<float>::infinity();
    for (int y = 0; y < height; ++y) {
        const auto *row = reinterpret_cast<const float *>(
            slice.data + static_cast<std::size_t>(y) * stride);
        for (int x = 0; x < width; ++x) {
            const float value = row[x];
            if (!std::isfinite(value) || value <= 0.0f) continue;
            lo = std::min(lo, value);
            hi = std::max(hi, value);
        }
    }

    QImage img(width, height, QImage::Format_Grayscale8);
    if (!(lo < hi)) {
        img.fill(0);
        return img;
    }

    const float scale = 255.0f / (hi - lo);
    for (int y = 0; y < height; ++y) {
        uchar *dst = img.scanLine(y);
        const auto *row = reinterpret_cast<const float *>(
            slice.data + static_cast<std::size_t>(y) * stride);
        for (int x = 0; x < width; ++x) {
            const float value = row[x];
            if (!std::isfinite(value) || value <= 0.0f) {
                dst[x] = 0;
                continue;
            }
            dst[x] = static_cast<uchar>(std::clamp((value - lo) * scale, 0.0f, 255.0f));
        }
    }
    return img;
}

QImage wrapMono8ToGrayscale8(const RawSlice &slice,
                              int width,
                              int height,
                              std::uint32_t stride)
{
    if (!hasValidImageLayout(slice, width, height, stride, static_cast<std::size_t>(width))) {
        return QImage();
    }

    QImage img(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        std::memcpy(img.scanLine(y),
                    slice.data + static_cast<std::size_t>(y) * stride,
                    static_cast<std::size_t>(width));
    }
    return img;
}

} // namespace

struct Ros2BridgeEntities final
{
    rclcpp::Subscription<mz_interfaces::msg::CameraStatus>::SharedPtr cameraStatus;
    rclcpp::Subscription<mz_interfaces::msg::ScanResult>::SharedPtr scanRange;
    rclcpp::Subscription<mz_interfaces::msg::ScanResult>::SharedPtr scanIntensity;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr backendState;
    rclcpp::Subscription<mz_interfaces::msg::MillingPaths>::SharedPtr millingPaths;
    rclcpp::Subscription<mz_interfaces::msg::MillingProgress>::SharedPtr millingProgress;
    rclcpp::Subscription<mz_interfaces::msg::PlcFeedback>::SharedPtr plcFeedback;
    rclcpp::Subscription<mz_interfaces::msg::PlcPathCommandParams>::SharedPtr plcPathCommandParams;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr teachRequest;

    bool pendingHasRange{false};
    bool pendingHasIntensity{false};
    std::uint64_t pendingFrameId{0};
    std::uint64_t pendingTaskId{0};
    std::uint64_t pendingTimestampNs{0};
    std::uint32_t pendingWidth{0};
    std::uint32_t pendingHeight{0};
    std::uint32_t pendingPixelFormat{0};
    QImage pendingRange;
    QImage pendingIntensity;
};

Ros2Bridge::Ros2Bridge(QObject *parent)
    : QObject(parent)
{
    // 让 QVector<MillingPathVM> 能通过 Qt::QueuedConnection 跨线程传递（ROS 回调线程 → GUI 线程）
    qRegisterMetaType<MillingPathVM>("MillingPathVM");
    qRegisterMetaType<QVector<MillingPathVM>>("QVector<MillingPathVM>");
    qRegisterMetaType<PlcFeedbackVM>("PlcFeedbackVM");
    qRegisterMetaType<PlcPathCommandParamsVM>("PlcPathCommandParamsVM");
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
        const auto statusQos = rclcpp::QoS(rclcpp::KeepLast(1))
                                   .reliable()
                                   .transient_local();

        auto clearPendingScanFrame = [this]() {
            if (!m_entities) return;
            m_entities->pendingHasRange = false;
            m_entities->pendingHasIntensity = false;
            m_entities->pendingFrameId = 0;
            m_entities->pendingTaskId = 0;
            m_entities->pendingTimestampNs = 0;
            m_entities->pendingWidth = 0;
            m_entities->pendingHeight = 0;
            m_entities->pendingPixelFormat = 0;
            m_entities->pendingRange = QImage();
            m_entities->pendingIntensity = QImage();
        };

        auto emitPendingScanFrame = [this, clearPendingScanFrame]() {
            if (!m_entities || (!m_entities->pendingHasRange && !m_entities->pendingHasIntensity)) {
                return;
            }
            emit scanFrameReady(m_entities->pendingRange,
                                m_entities->pendingIntensity,
                                m_entities->pendingFrameId,
                                m_entities->pendingTaskId,
                                m_entities->pendingTimestampNs,
                                m_entities->pendingWidth,
                                m_entities->pendingHeight,
                                m_entities->pendingPixelFormat);
            clearPendingScanFrame();
        };

        auto handleScanResult =
            [this, emitPendingScanFrame, clearPendingScanFrame](
                const mz_interfaces::msg::ScanResult::SharedPtr msg,
                std::uint8_t imageType) {
                if (!msg) return;

                const std::uint64_t offset = msg->offset;
                const std::uint64_t dataSize = msg->data_size;

                emitScanSegment(this, *msg, imageType, offset, dataSize);

                if (msg->frame_id == kInvalidFrame) {
                    if (imageType == 0) {
                        emit errorMessage(
                            QString("相机 Range 采集失败: frame_id=-1 shm=%1")
                                .arg(QString::fromStdString(msg->shm_name)));
                        clearPendingScanFrame();
                    } else {
                        emit infoMessage("相机未提供 Intensity 图像");
                        emitPendingScanFrame();
                    }
                    return;
                }
                if (dataSize == 0) {
                    return;
                }

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

                err.clear();
                const RawSlice slice = m_shmReader->slice(offset, dataSize, &err);
                if (!err.empty()) {
                    emit errorMessage(QString("读取共享内存切片失败: %1")
                                          .arg(QString::fromStdString(err)));
                }
                if (slice.empty()) {
                    return;
                }

                QImage image;
                if (imageType == 0) {
                    if (msg->pixel_format == kCv32Fc1) {
                        image = stretchFloat32ToGrayscale8(
                            slice, int(msg->width), int(msg->height), msg->stride);
                    } else if (msg->pixel_format == kCv16Uc1) {
                        image = stretchMono16ToGrayscale8(
                            slice, int(msg->width), int(msg->height), msg->stride);
                    } else {
                        emit errorMessage(QString("Unsupported Range image format: %1")
                                              .arg(msg->pixel_format));
                        return;
                    }
                } else {
                    if (msg->pixel_format != kCv8Uc1) {
                        emit errorMessage(QString("Unsupported Intensity image format: %1")
                                              .arg(msg->pixel_format));
                        return;
                    }
                    image = wrapMono8ToGrayscale8(
                        slice, int(msg->width), int(msg->height), msg->stride);
                }
                if (image.isNull()) {
                    emit errorMessage(QString("Invalid image layout: frame=%1 %2x%3 stride=%4 size=%5 fmt=%6")
                                          .arg(msg->frame_id)
                                          .arg(msg->width)
                                          .arg(msg->height)
                                          .arg(msg->stride)
                                          .arg(msg->data_size)
                                          .arg(msg->pixel_format));
                    return;
                }

                if ((m_entities->pendingHasRange || m_entities->pendingHasIntensity) &&
                    m_entities->pendingFrameId != msg->frame_id) {
                    emitPendingScanFrame();
                }

                m_entities->pendingFrameId = msg->frame_id;
                if (imageType == 0) {
                    m_entities->pendingHasRange = true;
                    m_entities->pendingRange = image;
                    m_entities->pendingTaskId = msg->task_id;
                    m_entities->pendingTimestampNs = msg->timestamp_ns;
                    m_entities->pendingWidth = msg->width;
                    m_entities->pendingHeight = msg->height;
                    m_entities->pendingPixelFormat = msg->pixel_format;
                } else {
                    m_entities->pendingHasIntensity = true;
                    m_entities->pendingIntensity = image;
                    if (!m_entities->pendingHasRange) {
                        m_entities->pendingTaskId = msg->task_id;
                        m_entities->pendingTimestampNs = msg->timestamp_ns;
                        m_entities->pendingWidth = msg->width;
                        m_entities->pendingHeight = msg->height;
                        m_entities->pendingPixelFormat = msg->pixel_format;
                    }
                }

                if (m_entities->pendingHasRange && m_entities->pendingHasIntensity) {
                    emitPendingScanFrame();
                }
            };

        m_entities->scanRange = m_node->create_subscription<mz_interfaces::msg::ScanResult>(
            kScanRangeTopic,
            qos,
            [handleScanResult](const mz_interfaces::msg::ScanResult::SharedPtr msg) {
                handleScanResult(msg, 0);
            });

        m_entities->scanIntensity = m_node->create_subscription<mz_interfaces::msg::ScanResult>(
            kScanIntensityTopic,
            qos,
            [handleScanResult](const mz_interfaces::msg::ScanResult::SharedPtr msg) {
                handleScanResult(msg, 1);
            });

        m_entities->backendState = m_node->create_subscription<std_msgs::msg::String>(
            kBackendStateTopic,
            qos,
            [this](const std_msgs::msg::String::SharedPtr msg) {
                if (msg) {
                    handleBackendState(msg->data);
                }
            });

        m_entities->cameraStatus =
            m_node->create_subscription<mz_interfaces::msg::CameraStatus>(
                kCameraStatusTopic,
                statusQos,
                [this](const mz_interfaces::msg::CameraStatus::SharedPtr msg) {
                    if (!msg) return;
                    emit cameraStatusReceived(
                        msg->connected,
                        QString::fromStdString(msg->camera_id),
                        QString::fromStdString(msg->message));
                });

        m_entities->millingPaths = m_node->create_subscription<mz_interfaces::msg::MillingPaths>(
            kMillingPathsTopic,
            qos,
            [this](const mz_interfaces::msg::MillingPaths::SharedPtr msg) {
                if (!msg) return;

                QVector<MillingPathVM> paths;
                paths.reserve(int(msg->paths.size()));
                for (const auto &p : msg->paths) {
                    MillingPathVM vm;
                    vm.uStart = p.u_start;
                    vm.vStart = p.v_start;
                    vm.uEnd   = p.u_end;
                    vm.vEnd   = p.v_end;
                    vm.rSpeed = p.r_speed;
                    vm.xSpeed = p.x_speed;
                    vm.ySpeed = p.y_speed;
                    vm.doCut  = p.do_cut;
                    paths.push_back(vm);
                }

                emit millingPathsReceived(
                    msg->task_id,
                    int(msg->path_total),
                    int(msg->max_particle_height),
                    msg->calibration_applied,
                    paths);

                emit infoMessage(QString("收到打磨路径: task=%1 total=%2 maxH=%3 calib=%4 size=%5")
                                     .arg(msg->task_id)
                                     .arg(msg->path_total)
                                     .arg(msg->max_particle_height)
                                     .arg(msg->calibration_applied ? "true" : "false")
                                     .arg(paths.size()));
            });

        m_entities->millingProgress = m_node->create_subscription<mz_interfaces::msg::MillingProgress>(
            kMillingProgressTopic,
            qos,
            [this](const mz_interfaces::msg::MillingProgress::SharedPtr msg) {
                if (!msg) return;

                const QString message = QString::fromStdString(msg->message);
                emit millingProgressReceived(
                    msg->task_id,
                    msg->progress,
                    msg->finished,
                    msg->success,
                    message);
            });

        m_entities->plcFeedback = m_node->create_subscription<mz_interfaces::msg::PlcFeedback>(
            kPlcFeedbackTopic,
            qos,
            [this](const mz_interfaces::msg::PlcFeedback::SharedPtr msg) {
                if (!msg) return;

                PlcFeedbackVM vm;
                vm.statusWord = msg->status_word;
                vm.completedPath = msg->completed_path;
                vm.currentPath = msg->current_path;
                vm.faultCode = msg->fault_code;
                vm.pathParams.xPos = msg->path_params.x_pos;
                vm.pathParams.yPos = msg->path_params.y_pos;
                vm.pathParams.zPos = msg->path_params.z_pos;
                vm.pathParams.xSpeed = msg->path_params.x_speed;
                vm.pathParams.ySpeed = msg->path_params.y_speed;
                vm.pathParams.zSpeed = msg->path_params.z_speed;
                vm.pathParams.spindleSpeed = msg->path_params.spindle_speed;
                vm.pathParams.spindleTorque = msg->path_params.spindle_torque;

                emit plcFeedbackReceived(vm);
            });

        m_entities->plcPathCommandParams =
            m_node->create_subscription<mz_interfaces::msg::PlcPathCommandParams>(
                kPlcPathCommandParamsTopic,
                qos,
                [this](const mz_interfaces::msg::PlcPathCommandParams::SharedPtr msg) {
                    if (!msg) return;

                    PlcPathCommandParamsVM vm;
                    vm.x = msg->x;
                    vm.y = msg->y;
                    vm.z = msg->z;
                    vm.feedSpeed = msg->feed_speed;
                    vm.feedAmount = msg->feed_amount;
                    vm.spindleSpeed = msg->spindle_speed;
                    vm.plungeCount = msg->plunge_count;
                    vm.plungeAmount = msg->plunge_amount;
                    vm.feedDirection = msg->feed_direction;

                    emit plcPathCommandReceived(vm);
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

        emit infoMessage("ROS2 通信节点已启动，监听 camera/status / scan_range / scan_intensity / backend_state / milling/paths / milling/progress / plc/feedback / plc_path_command_params");
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
