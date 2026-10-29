#include "bridge/Ros2Bridge.h"

#include "app/Logger.h"
#include "bridge/ShmImageReader.h"

#include <mz_interfaces/msg/camera_status.hpp>
#include <mz_interfaces/msg/milling_command.hpp>
#include <mz_interfaces/msg/milling_command_result.hpp>
#include <mz_interfaces/msg/milling_path.hpp>
#include <mz_interfaces/msg/milling_paths.hpp>
#include <mz_interfaces/msg/milling_progress.hpp>
#include <mz_interfaces/msg/plc_feedback.hpp>
#include <mz_interfaces/msg/scan_geometry.hpp>
#include <mz_interfaces/msg/scan_result.hpp>
#include <mz_interfaces/msg/sick_camera_parameter_result.hpp>
#include <mz_interfaces/msg/sick_camera_parameters.hpp>
#include <rclcpp/executors/single_threaded_executor.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/string.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QMetaObject>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>

namespace {

constexpr char kNodeName[] = "mz_tlz";
constexpr char kCameraStatusTopic[] = "camera/status";
constexpr char kSickParametersTopic[] = "sick_camera/parameters";
constexpr char kSickSetParametersTopic[] = "sick_camera/set_parameters";
constexpr char kSickParameterResultTopic[] = "sick_camera/parameter_result";
constexpr char kScanRangeTopic[] = "scan_range";
constexpr char kScanIntensityTopic[] = "scan_intensity";
constexpr char kScanGeometryTopic[] = "scan_geometry";
constexpr char kBackendStateTopic[] = "backend_state";
constexpr char kTeachRequestTopic[] = "teach_request";
constexpr char kMillingPathsTopic[] = "milling/paths";
constexpr char kMillingProgressTopic[] = "milling/progress";
constexpr char kMillingCommandTopic[] = "milling/command";
constexpr char kMillingCommandResultTopic[] = "milling/command_result";
constexpr char kPlcFeedbackTopic[] = "plc/feedback";
constexpr char kCoordinateOffsetTopic[] = "plc/coordinate_offset";
constexpr char kDispatchPathsToPlcTopic[] = "plc/dispatch_paths_to_plc";
constexpr std::uint64_t kInvalidFrame = ~std::uint64_t(0);
constexpr std::uint32_t kCv8Uc1 = 0;
constexpr std::uint32_t kCv16Uc1 = 2;
constexpr std::uint32_t kCv32Fc1 = 5;
constexpr float kDefaultMissingValue = -1.0e6f;

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
                                  std::uint32_t stride,
                                  float missingValue)
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
            if (!std::isfinite(value) || value == missingValue) continue;
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
            if (!std::isfinite(value) || value == missingValue) {
                dst[x] = 0;
                continue;
            }
            dst[x] = static_cast<uchar>(std::clamp((value - lo) * scale, 0.0f, 255.0f));
        }
    }
    return img;
}

QByteArray copyFloat32Rows(const RawSlice &slice,
                           int width,
                           int height,
                           std::uint32_t stride)
{
    const std::size_t rowBytes = static_cast<std::size_t>(width) * sizeof(float);
    if (!hasValidImageLayout(slice, width, height, stride, rowBytes)) {
        return {};
    }

    const quint64 totalBytes = quint64(rowBytes) * quint64(height);
    if (totalBytes > quint64(std::numeric_limits<int>::max())) {
        return {};
    }

    QByteArray result;
    result.resize(int(totalBytes));
    for (int y = 0; y < height; ++y) {
        std::memcpy(result.data() + static_cast<std::size_t>(y) * rowBytes,
                    slice.data + static_cast<std::size_t>(y) * stride,
                    rowBytes);
    }
    return result;
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
    rclcpp::TimerBase::SharedPtr cameraStatusDiagnosticsTimer;
    rclcpp::Subscription<mz_interfaces::msg::ScanResult>::SharedPtr scanRange;
    rclcpp::Subscription<mz_interfaces::msg::ScanResult>::SharedPtr scanIntensity;
    rclcpp::Subscription<mz_interfaces::msg::ScanGeometry>::SharedPtr scanGeometry;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr backendState;
    rclcpp::Subscription<mz_interfaces::msg::MillingPaths>::SharedPtr millingPaths;
    rclcpp::Subscription<mz_interfaces::msg::MillingProgress>::SharedPtr millingProgress;
    rclcpp::Subscription<mz_interfaces::msg::PlcFeedback>::SharedPtr plcFeedback;
    rclcpp::Subscription<mz_interfaces::msg::SickCameraParameters>::SharedPtr sickParameters;
    rclcpp::Subscription<mz_interfaces::msg::SickCameraParameterResult>::SharedPtr sickParameterResult;
    rclcpp::Publisher<mz_interfaces::msg::SickCameraParameters>::SharedPtr sickSetParameters;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr teachRequest;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr coordinateOffset;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr dispatchPathsToPlc;
    rclcpp::Publisher<mz_interfaces::msg::MillingCommand>::SharedPtr millingCommand;
    rclcpp::Subscription<mz_interfaces::msg::MillingCommandResult>::SharedPtr millingCommandResult;

    bool pendingHasRange{false};
    bool pendingHasIntensity{false};
    std::uint64_t pendingFrameId{0};
    std::uint64_t pendingTaskId{0};
    std::uint64_t pendingTimestampNs{0};
    std::uint32_t pendingWidth{0};
    std::uint32_t pendingHeight{0};
    int pendingRangePixelFormat{-1};
    int pendingIntensityPixelFormat{-1};
    QImage pendingRange;
    QImage pendingIntensity;
    QByteArray pendingRangeFloat32;
    bool pendingHasGeometry{false};
    std::uint64_t pendingGeometryFrameId{0};
    bool pendingHasMetricGeometry{false};
    double pendingXOffsetMm{0.0};
    double pendingYOffsetMm{0.0};
    double pendingXScaleMm{1.0};
    double pendingYScaleMm{1.0};
    float pendingMissingValue{-1.0e6f};
    double pendingEncoderResolutionMm{0.0};
    QVector<qint32> pendingEncoderValues;
};

Ros2Bridge::Ros2Bridge(QObject *parent)
    : QObject(parent)
{
    // 让 QVector<MillingPathVM> 能通过 Qt::QueuedConnection 跨线程传递（ROS 回调线程 → GUI 线程）
    qRegisterMetaType<MillingPathVM>("MillingPathVM");
    qRegisterMetaType<QVector<MillingPathVM>>("QVector<MillingPathVM>");
    qRegisterMetaType<ParticleRegionVM>("ParticleRegionVM");
    qRegisterMetaType<QVector<ParticleRegionVM>>("QVector<ParticleRegionVM>");
    qRegisterMetaType<PlcFeedbackVM>("PlcFeedbackVM");
    qRegisterMetaType<PointCloudFrame>("PointCloudFrame");
    qRegisterMetaType<SickCameraParametersVM>("SickCameraParametersVM");
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
            const std::uint64_t imageFrameId = m_entities->pendingFrameId;
            m_entities->pendingHasRange = false;
            m_entities->pendingHasIntensity = false;
            m_entities->pendingFrameId = 0;
            m_entities->pendingTaskId = 0;
            m_entities->pendingTimestampNs = 0;
            m_entities->pendingWidth = 0;
            m_entities->pendingHeight = 0;
            m_entities->pendingRangePixelFormat = -1;
            m_entities->pendingIntensityPixelFormat = -1;
            m_entities->pendingRange = QImage();
            m_entities->pendingIntensity = QImage();
            m_entities->pendingRangeFloat32.clear();
            if (m_entities->pendingHasGeometry &&
                m_entities->pendingGeometryFrameId == imageFrameId) {
                m_entities->pendingHasGeometry = false;
                m_entities->pendingGeometryFrameId = 0;
                m_entities->pendingHasMetricGeometry = false;
                m_entities->pendingXOffsetMm = 0.0;
                m_entities->pendingYOffsetMm = 0.0;
                m_entities->pendingXScaleMm = 1.0;
                m_entities->pendingYScaleMm = 1.0;
                m_entities->pendingMissingValue = kDefaultMissingValue;
                m_entities->pendingEncoderResolutionMm = 0.0;
                m_entities->pendingEncoderValues.clear();
            }
        };

        auto emitPendingScanFrame = [this, clearPendingScanFrame]() {
            if (!m_entities || (!m_entities->pendingHasRange && !m_entities->pendingHasIntensity)) {
                return;
            }
            PointCloudFrame frame;
            // 深拷贝，避免跨线程 QueuedConnection 时与后续 clear/SHM 复用打架。
            frame.rangePreview = m_entities->pendingRange.isNull()
                ? QImage()
                : m_entities->pendingRange.copy();
            frame.intensity = m_entities->pendingIntensity.isNull()
                ? QImage()
                : m_entities->pendingIntensity.copy();
            frame.rangeFloat32 = m_entities->pendingRangeFloat32;
            frame.frameId = m_entities->pendingFrameId;
            frame.taskId = m_entities->pendingTaskId;
            frame.timestampNs = m_entities->pendingTimestampNs;
            frame.width = m_entities->pendingWidth;
            frame.height = m_entities->pendingHeight;
            frame.rangePixelFormat = m_entities->pendingRangePixelFormat;
            frame.intensityPixelFormat = m_entities->pendingIntensityPixelFormat;
            if (m_entities->pendingHasGeometry &&
                m_entities->pendingGeometryFrameId == m_entities->pendingFrameId) {
                frame.hasMetricGeometry = m_entities->pendingHasMetricGeometry;
                frame.xOffsetMm = m_entities->pendingXOffsetMm;
                frame.yOffsetMm = m_entities->pendingYOffsetMm;
                frame.xScaleMm = m_entities->pendingXScaleMm;
                frame.yScaleMm = m_entities->pendingYScaleMm;
                frame.missingValue = m_entities->pendingMissingValue;
                frame.encoderResolutionMm = m_entities->pendingEncoderResolutionMm;
                frame.encoderValues = m_entities->pendingEncoderValues;
            }
            // 先打日志再投递 UI：即使 UI 槽异常，也能确认配对已完成。
            LOG("[ROS][ScanPair] ready → UI: frame=%llu task=%llu %ux%u "
                "range=%d intensity=%d float32=%d geometry=%d",
                static_cast<unsigned long long>(frame.frameId),
                static_cast<unsigned long long>(frame.taskId),
                frame.width,
                frame.height,
                frame.rangePreview.isNull() ? 0 : 1,
                frame.intensity.isNull() ? 0 : 1,
                frame.hasFloatRange() ? 1 : 0,
                frame.hasMetricGeometry ? 1 : 0);
            emit infoMessage(
                QStringLiteral("扫图配对完成，准备刷新界面: frame=%1 task=%2 %3x%4 range=%5 intensity=%6")
                    .arg(frame.frameId)
                    .arg(frame.taskId)
                    .arg(frame.width)
                    .arg(frame.height)
                    .arg(frame.rangePreview.isNull() ? QStringLiteral("无") : QStringLiteral("有"))
                    .arg(frame.intensity.isNull() ? QStringLiteral("无") : QStringLiteral("有")));
            emit scanFrameReady(frame);
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

                if (msg->frame_id == 0 || msg->frame_id == kInvalidFrame) {
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
                    emit errorMessage(
                        QStringLiteral("扫图数据大小为 0，跳过: type=%1 frame=%2 shm=%3")
                            .arg(imageType)
                            .arg(msg->frame_id)
                            .arg(QString::fromStdString(msg->shm_name)));
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
                    emit errorMessage(
                        QStringLiteral("共享内存切片为空: type=%1 frame=%2 offset=%3 size=%4 shm=%5")
                            .arg(imageType)
                            .arg(msg->frame_id)
                            .arg(offset)
                            .arg(dataSize)
                            .arg(QString::fromStdString(msg->shm_name)));
                    return;
                }

                QImage image;
                if (imageType == 0) {
                    if (msg->pixel_format == kCv32Fc1) {
                        const float missingValue =
                            (m_entities->pendingHasGeometry &&
                             m_entities->pendingGeometryFrameId == msg->frame_id)
                                ? m_entities->pendingMissingValue
                                : kDefaultMissingValue;
                        image = stretchFloat32ToGrayscale8(
                            slice,
                            int(msg->width),
                            int(msg->height),
                            msg->stride,
                            missingValue);
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
                    (m_entities->pendingTaskId != msg->task_id ||
                     m_entities->pendingFrameId != msg->frame_id)) {
                    LOG("[ROS][ScanPair] incomplete frame flushed: pending_task=%llu "
                        "pending_frame=%llu has_range=%d has_intensity=%d; "
                        "incoming_task=%llu incoming_frame=%llu type=%u",
                        static_cast<unsigned long long>(m_entities->pendingTaskId),
                        static_cast<unsigned long long>(m_entities->pendingFrameId),
                        m_entities->pendingHasRange ? 1 : 0,
                        m_entities->pendingHasIntensity ? 1 : 0,
                        static_cast<unsigned long long>(msg->task_id),
                        static_cast<unsigned long long>(msg->frame_id),
                        static_cast<unsigned int>(imageType));
                    emitPendingScanFrame();
                }

                m_entities->pendingFrameId = msg->frame_id;
                if (imageType == 0) {
                    m_entities->pendingHasRange = true;
                    m_entities->pendingRange = image;
                    if (msg->pixel_format == kCv32Fc1) {
                        m_entities->pendingRangeFloat32 = copyFloat32Rows(
                            slice, int(msg->width), int(msg->height), msg->stride);
                        if (m_entities->pendingRangeFloat32.isEmpty()) {
                            emit errorMessage("复制 Float32 Range 数据失败");
                            clearPendingScanFrame();
                            return;
                        }
                    }
                    m_entities->pendingTaskId = msg->task_id;
                    m_entities->pendingTimestampNs = msg->timestamp_ns;
                    m_entities->pendingWidth = msg->width;
                    m_entities->pendingHeight = msg->height;
                    m_entities->pendingRangePixelFormat = int(msg->pixel_format);
                } else {
                    m_entities->pendingHasIntensity = true;
                    m_entities->pendingIntensity = image;
                    m_entities->pendingIntensityPixelFormat = int(msg->pixel_format);
                    if (!m_entities->pendingHasRange) {
                        m_entities->pendingTaskId = msg->task_id;
                        m_entities->pendingTimestampNs = msg->timestamp_ns;
                        m_entities->pendingWidth = msg->width;
                        m_entities->pendingHeight = msg->height;
                    }
                }

                if (m_entities->pendingHasRange && m_entities->pendingHasIntensity) {
                    emitPendingScanFrame();
                }
            };

        auto handleScanGeometry =
            [this](const mz_interfaces::msg::ScanGeometry::SharedPtr msg) {
                if (!msg || !m_entities) {
                    return;
                }

                m_entities->pendingHasGeometry = true;
                m_entities->pendingGeometryFrameId = msg->frame_id;
                m_entities->pendingHasMetricGeometry = msg->has_metric_geometry;
                m_entities->pendingXOffsetMm = msg->x_offset_mm;
                m_entities->pendingYOffsetMm = msg->y_offset_mm;
                m_entities->pendingXScaleMm = msg->x_scale_mm;
                m_entities->pendingYScaleMm = msg->y_scale_mm;
                m_entities->pendingMissingValue = msg->missing_value;
                m_entities->pendingEncoderResolutionMm = msg->encoder_resolution_mm;
                m_entities->pendingEncoderValues.clear();
                m_entities->pendingEncoderValues.reserve(int(msg->encoder_values.size()));
                for (const std::int32_t value : msg->encoder_values) {
                    m_entities->pendingEncoderValues.push_back(value);
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

        m_entities->scanGeometry = m_node->create_subscription<mz_interfaces::msg::ScanGeometry>(
            kScanGeometryTopic,
            qos,
            handleScanGeometry);

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
        m_entities->cameraStatusDiagnosticsTimer = m_node->create_wall_timer(
            std::chrono::seconds(1),
            [this]() {
                if (m_entities && m_entities->cameraStatus) {
                    emit cameraStatusDiagnosticsChanged(
                        true,
                        static_cast<int>(m_entities->cameraStatus->get_publisher_count()));
                }
            });

        const auto toSickVm = [](const mz_interfaces::msg::SickCameraParameters &message) {
            SickCameraParametersVM vm;
            vm.scanLines = message.scan_lines;
            vm.exposureTimeUs = message.exposure_time_us;
            vm.hdrMode = QString::fromStdString(message.hdr_mode);
            vm.yResolutionMm = message.y_resolution_mm;
            return vm;
        };
        m_entities->sickParameters =
            m_node->create_subscription<mz_interfaces::msg::SickCameraParameters>(
                kSickParametersTopic,
                statusQos,
                [this, toSickVm](const mz_interfaces::msg::SickCameraParameters::SharedPtr msg) {
                    if (msg) {
                        emit sickCameraParametersReceived(toSickVm(*msg));
                    }
                });
        m_entities->sickParameterResult =
            m_node->create_subscription<mz_interfaces::msg::SickCameraParameterResult>(
                kSickParameterResultTopic,
                qos,
                [this, toSickVm](const mz_interfaces::msg::SickCameraParameterResult::SharedPtr msg) {
                    if (msg) {
                        emit sickCameraParameterResultReceived(
                            msg->success,
                            QString::fromStdString(msg->message),
                            toSickVm(msg->parameters));
                    }
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

                const quint64 taskId = msg->task_id > 0 ? quint64(msg->task_id) : 0;
                int cutCount = 0;
                int moveCount = 0;
                for (const auto &path : paths) {
                    if (path.doCut) {
                        ++cutCount;
                    } else {
                        ++moveCount;
                    }
                }

                const QString particleBoardGrade =
                    QString::fromStdString(msg->particle_board_grade);
                const QString drawimagePath =
                    QString::fromStdString(msg->drawimage_path);
                QVector<ParticleRegionVM> particles;
                particles.reserve(int(msg->particles.size()));
                for (const auto &region : msg->particles) {
                    if (region.contour_u.size() < 3 || region.contour_u.size() != region.contour_v.size()) {
                        continue;
                    }
                    ParticleRegionVM particle;
                    particle.index = int(region.particle_index);
                    particle.height = region.height;
                    particle.uMin = region.u_min;
                    particle.vMin = region.v_min;
                    particle.uMax = region.u_max;
                    particle.vMax = region.v_max;
                    particle.contourU.reserve(int(region.contour_u.size()));
                    particle.contourV.reserve(int(region.contour_v.size()));
                    for (int coordinate : region.contour_u) {
                        particle.contourU.push_back(coordinate);
                    }
                    for (int coordinate : region.contour_v) {
                        particle.contourV.push_back(coordinate);
                    }
                    particles.push_back(particle);
                }

                LOG("[ROS][MillingPaths] received topic=/%s task_id=%llu path_total=%u "
                    "max_particle_height=%u grade=%s drawimage=%s paths=%d particles=%d cut=%d move=%d",
                    kMillingPathsTopic,
                    static_cast<unsigned long long>(taskId),
                    static_cast<unsigned int>(msg->path_total),
                    static_cast<unsigned int>(msg->max_particle_height),
                    qUtf8Printable(particleBoardGrade),
                    qUtf8Printable(drawimagePath),
                    paths.size(),
                    particles.size(),
                    cutCount,
                    moveCount);
                for (int i = 0; i < paths.size(); ++i) {
                    const auto &path = paths[i];
                    LOG("[ROS][MillingPaths] path[%d] uv=(%d,%d)->(%d,%d) "
                        "r_speed=%.3f x_speed=%.3f y_speed=%.3f do_cut=%s",
                        i + 1,
                        path.uStart,
                        path.vStart,
                        path.uEnd,
                        path.vEnd,
                        double(path.rSpeed),
                        double(path.xSpeed),
                        double(path.ySpeed),
                        path.doCut ? "true" : "false");
                }

                emit millingPathsReceived(
                    taskId,
                    int(msg->path_total),
                    int(msg->max_particle_height),
                    particleBoardGrade,
                    drawimagePath,
                    paths,
                    particles);

                emit infoMessage(QString("收到打磨路径: task=%1 total=%2 maxH=%3 grade=%4 image=%5 size=%6 cut=%7 move=%8")
                                     .arg(taskId)
                                     .arg(msg->path_total)
                                     .arg(msg->max_particle_height)
                                     .arg(particleBoardGrade)
                                     .arg(drawimagePath)
                                     .arg(paths.size())
                                     .arg(cutCount)
                                     .arg(moveCount));
            });

        m_entities->millingProgress = m_node->create_subscription<mz_interfaces::msg::MillingProgress>(
            kMillingProgressTopic,
            qos,
            [this](const mz_interfaces::msg::MillingProgress::SharedPtr msg) {
                if (!msg) return;

                const QString message = QString::fromStdString(msg->message);
                const quint64 taskId = msg->task_id > 0 ? quint64(msg->task_id) : 0;
                queueMillingProgress(taskId,
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
                vm.pathParams.intSpare1 = msg->path_params.int_spare1;
                vm.pathParams.intSpare2 = msg->path_params.int_spare2;
                vm.pathParams.realSpare1 = msg->path_params.real_spare1;
                vm.pathParams.realSpare2 = msg->path_params.real_spare2;

                emit plcFeedbackReceived(vm);
            });

        m_entities->millingCommandResult =
            m_node->create_subscription<mz_interfaces::msg::MillingCommandResult>(
                kMillingCommandResultTopic,
                qos,
                [this](const mz_interfaces::msg::MillingCommandResult::SharedPtr msg) {
                    if (!msg) {
                        return;
                    }
                    emit millingCommandResultReceived(
                        msg->command,
                        msg->task_id,
                        msg->accepted,
                        QString::fromStdString(msg->message));
                });

        m_entities->teachRequest = m_node->create_publisher<std_msgs::msg::String>(
            kTeachRequestTopic, qos);
        m_entities->coordinateOffset = m_node->create_publisher<std_msgs::msg::String>(
            kCoordinateOffsetTopic, qos);
        m_entities->dispatchPathsToPlc = m_node->create_publisher<std_msgs::msg::Bool>(
            kDispatchPathsToPlcTopic, qos);
        m_entities->sickSetParameters =
            m_node->create_publisher<mz_interfaces::msg::SickCameraParameters>(
                kSickSetParametersTopic,
                rclcpp::QoS(rclcpp::KeepLast(1)).reliable());
        m_entities->millingCommand = m_node->create_publisher<mz_interfaces::msg::MillingCommand>(
            kMillingCommandTopic,
            qos);

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

        emit infoMessage("ROS2 通信节点已启动，监听 camera/status / scan_range / scan_intensity / scan_geometry / backend_state / milling/paths / milling/progress / milling/command_result / plc/feedback");
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
    emit cameraStatusDiagnosticsChanged(false, 0);

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

    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        m_pendingMillingProgress.clear();
        m_progressDeliveryScheduled = false;
    }

    emit infoMessage("ROS2 通信节点已停止");
}

bool Ros2Bridge::isRunning() const
{
    return m_running;
}

void Ros2Bridge::refreshCameraStatusDiagnostics()
{
    if (!m_running || !m_entities || !m_entities->cameraStatus) {
        emit cameraStatusDiagnosticsChanged(false, 0);
        return;
    }
    emit cameraStatusDiagnosticsChanged(
        true,
        static_cast<int>(m_entities->cameraStatus->get_publisher_count()));
}

void Ros2Bridge::queueMillingProgress(quint64 taskId,
                                      float progress,
                                      bool finished,
                                      bool success,
                                      const QString &message)
{
    bool scheduleDelivery = false;
    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        auto pending = std::find_if(
            m_pendingMillingProgress.begin(),
            m_pendingMillingProgress.end(),
            [taskId](const MillingProgressSnapshot &snapshot) {
                return snapshot.taskId == taskId;
            });

        if (pending != m_pendingMillingProgress.end()) {
            // 同一任务一旦收到终态，不允许随后到达的普通进度覆盖它。
            if (pending->finished && !finished) {
                return;
            }
            if (!pending->finished && !finished) {
                progress = std::max(progress, pending->progress);
            }
            pending->progress = progress;
            pending->finished = finished;
            pending->success = success;
            pending->message = message;
            pending->valid = true;
        } else {
            m_pendingMillingProgress.push_back(
                {taskId, progress, finished, success, message, true});
        }

        if (!m_progressDeliveryScheduled) {
            m_progressDeliveryScheduled = true;
            scheduleDelivery = true;
        }
    }

    if (scheduleDelivery &&
        !QMetaObject::invokeMethod(this,
                                   [this]() { deliverLatestMillingProgress(); },
                                   Qt::QueuedConnection)) {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        m_progressDeliveryScheduled = false;
        LOG("[ROS][Progress] failed to schedule GUI delivery for task=%llu",
            static_cast<unsigned long long>(taskId));
    }
}

void Ros2Bridge::deliverLatestMillingProgress()
{
    std::vector<MillingProgressSnapshot> snapshots;
    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        if (m_pendingMillingProgress.empty()) {
            m_progressDeliveryScheduled = false;
            return;
        }
        snapshots.swap(m_pendingMillingProgress);
        m_progressDeliveryScheduled = false;
    }

    for (const MillingProgressSnapshot &snapshot : snapshots) {
        emit millingProgressReceived(snapshot.taskId,
                                     snapshot.progress,
                                     snapshot.finished,
                                     snapshot.success,
                                     snapshot.message);
    }
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

void Ros2Bridge::publishCoordinateOffset(double xMm, double yMm, double zMm)
{
    if (!m_running || !m_entities || !m_entities->coordinateOffset) {
        LOG("[设置参数] 发送坐标补偿失败: ROS2 节点未运行");
        emit errorMessage("ROS2 节点未运行，无法发送坐标补偿");
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("x"), xMm);
    obj.insert(QStringLiteral("y"), yMm);
    obj.insert(QStringLiteral("z"), zMm);

    std_msgs::msg::String msg;
    msg.data = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
    m_entities->coordinateOffset->publish(msg);

    const QString payload = QString::fromStdString(msg.data);
    LOG("[设置参数] 已发送 plc/coordinate_offset: %s", payload.toUtf8().constData());
    emit infoMessage(QStringLiteral("已发送坐标补偿 (plc/coordinate_offset): %1").arg(payload));
}

void Ros2Bridge::publishDispatchPathsToPlc(bool enabled)
{
    if (!m_running || !m_entities || !m_entities->dispatchPathsToPlc) {
        LOG("[设置参数] 发送下发路径开关失败: ROS2 节点未运行");
        emit errorMessage("ROS2 节点未运行，无法发送下发路径开关");
        return;
    }

    std_msgs::msg::Bool msg;
    msg.data = enabled;
    m_entities->dispatchPathsToPlc->publish(msg);

    const QString flag = enabled ? QStringLiteral("true") : QStringLiteral("false");
    LOG("[设置参数] 已发送 plc/dispatch_paths_to_plc: %s", flag.toUtf8().constData());
    emit infoMessage(QStringLiteral("已发送下发路径开关 (plc/dispatch_paths_to_plc): %1").arg(flag));
}

void Ros2Bridge::publishSickCameraParameters(const SickCameraParametersVM &parameters)
{
    if (!m_running || !m_entities || !m_entities->sickSetParameters) {
        emit errorMessage(QStringLiteral("ROS2 节点未运行，无法写入 SICK 采集参数"));
        return;
    }
    mz_interfaces::msg::SickCameraParameters message;
    message.scan_lines = parameters.scanLines;
    message.exposure_time_us = parameters.exposureTimeUs;
    message.hdr_mode = parameters.hdrMode.toStdString();
    message.y_resolution_mm = parameters.yResolutionMm;
    m_entities->sickSetParameters->publish(message);
    emit infoMessage(QStringLiteral("已发送 SICK 采集参数：行数=%1，曝光=%2 μs，HDR=%3，Y 分辨率=%4 mm/行")
        .arg(parameters.scanLines)
        .arg(parameters.exposureTimeUs)
        .arg(parameters.hdrMode)
        .arg(parameters.yResolutionMm));
}

bool Ros2Bridge::publishMillingCommand(MillingCommand command, quint64 taskId)
{
    if (!m_running || !m_entities || !m_entities->millingCommand) {
        emit errorMessage(QStringLiteral("ROS2 节点未运行，无法发送修磨启停"));
        return false;
    }

    mz_interfaces::msg::MillingCommand message;
    message.command = static_cast<std::uint8_t>(command);
    message.task_id = taskId;
    m_entities->millingCommand->publish(message);
    LOG("[修磨启停] 已发送 milling/command: command=%u task_id=%llu",
        static_cast<unsigned>(message.command),
        static_cast<unsigned long long>(taskId));
    emit infoMessage(QStringLiteral("已发送修磨%1 (milling/command, task_id=%2)")
                         .arg(millingCommandLabel(command))
                         .arg(taskId));
    return true;
}
