#ifndef ROS2BRIDGE_H
#define ROS2BRIDGE_H

#include "bridge/MillingCommand.h"
#include "bridge/MillingPathVM.h"
#include "bridge/PlcFeedbackVM.h"
#include "bridge/SickCameraParametersVM.h"
#include "vision/PointCloudFrame.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rclcpp {
class Node;
namespace executors {
class SingleThreadedExecutor;
} // namespace executors
} // namespace rclcpp

struct Ros2BridgeEntities;
class ShmImageReader;

class Ros2Bridge : public QObject
{
    Q_OBJECT

public:
    explicit Ros2Bridge(QObject *parent = nullptr);
    ~Ros2Bridge() override;

    bool start();
    void stop();
    bool isRunning() const;
    void refreshCameraStatusDiagnostics();

    void publishTeachRequest();
    // 写入坐标补偿（mm）到 plc/coordinate_offset，JSON: {"x":..,"y":..,"z":..}
    void publishCoordinateOffset(double xMm, double yMm, double zMm);
    // 是否下发路径到 PLC（true=写 PLC，false=只发 milling/paths）
    void publishDispatchPathsToPlc(bool enabled);
    // 写入 SICK 采集参数，实际结果由 sickCameraParameterResultReceived 返回。
    void publishSickCameraParameters(const SickCameraParametersVM &parameters);
    // 写入修磨启停。节点未运行时返回 false，不改本地状态。
    bool publishMillingCommand(MillingCommand command, quint64 taskId);

signals:
    void infoMessage(const QString &message);
    void errorMessage(const QString &message);
    void scanResultReceived(quint8 imageType,
                            quint64 frameId,
                            quint64 taskId,
                            const QString &shmName,
                            quint64 offset,
                            quint64 dataSize,
                            quint32 width,
                            quint32 height,
                            quint32 pixelFormat);
    // 从共享内存读出并拥有的一帧。Float32 Range 保留原始毫米值，
    // preview/intensity 继续供现有 2D 视图使用。
    void scanFrameReady(const PointCloudFrame &frame);
    void backendStateReceived(const QString &state);
    void cameraStatusReceived(bool connected,
                              const QString &cameraId,
                              const QString &message);
    void cameraStatusDiagnosticsChanged(bool bridgeRunning, int publisherCount);
    void sickCameraParametersReceived(const SickCameraParametersVM &parameters);
    void sickCameraParameterResultReceived(bool success,
                                           const QString &message,
                                           const SickCameraParametersVM &parameters);

    // 打磨路径全量下发（后端 → 前端）。
    void millingPathsReceived(quint64 taskId,
                              int pathTotal,
                              int maxParticleHeight,
                              const QString &particleBoardGrade,
                              const QString &drawimagePath,
                              const QVector<MillingPathVM> &paths,
                              const QVector<ParticleRegionVM> &particles);

    // 打磨进度（后端 → 前端）。progress 按 0~100 显示。
    void millingProgressReceived(quint64 taskId,
                                 float progress,
                                 bool finished,
                                 bool success,
                                 const QString &message);

    // PLC 反馈（后端 → 前端）：状态字、路径计数、故障码、当前位置/速度/主轴参数。
    void plcFeedbackReceived(const PlcFeedbackVM &feedback);

    // 后端是否接受 milling/command。accepted 为 false 时 message 是拒绝原因。
    void millingCommandResultReceived(quint8 command,
                                      quint64 taskId,
                                      bool accepted,
                                      const QString &message);

private:
    void handleBackendState(const std::string &state);
    void queueMillingProgress(quint64 taskId,
                              float progress,
                              bool finished,
                              bool success,
                              const QString &message);
    void deliverLatestMillingProgress();

    struct MillingProgressSnapshot
    {
        quint64 taskId{0};
        float progress{0.0f};
        bool finished{false};
        bool success{false};
        QString message;
        bool valid{false};
    };

private:
    std::shared_ptr<rclcpp::Node> m_node;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::shared_ptr<Ros2BridgeEntities> m_entities;
    std::thread m_spinThread;
    bool m_running{false};

    std::mutex m_readerMutex;
    std::unique_ptr<ShmImageReader> m_shmReader;

    std::mutex m_progressMutex;
    std::vector<MillingProgressSnapshot> m_pendingMillingProgress;
    bool m_progressDeliveryScheduled{false};
};

#endif // ROS2BRIDGE_H
