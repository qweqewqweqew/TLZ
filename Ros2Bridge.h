#ifndef ROS2BRIDGE_H
#define ROS2BRIDGE_H

#include "MillingPathVM.h"
#include "PlcFeedbackVM.h"
#include "PlcPathCommandParamsVM.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

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

    void publishTeachRequest();

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
    // 从共享内存读出的一帧：range 为 Float32/Mono16 拉伸后的 Grayscale8，
    // intensity 为 Mono8 原始像素；任一未提供时对应 QImage 可能为空。
    // taskId 为后端任务号（一次任务多次扫描，帧按 taskId 归拢）。
    void scanFrameReady(const QImage &range,
                        const QImage &intensity,
                        quint64 frameId,
                        quint64 taskId,
                        quint64 timestampNs,
                        quint32 width,
                        quint32 height,
                        quint32 pixelFormat);
    void backendStateReceived(const QString &state);

    // 打磨路径全量下发（后端 → 前端），calibrationApplied=false 时前端显示未标定角标。
    void millingPathsReceived(quint64 taskId,
                              int pathTotal,
                              int maxParticleHeight,
                              bool calibrationApplied,
                              const QVector<MillingPathVM> &paths);

    // 打磨进度（后端 → 前端）。progress 按 0~100 显示。
    void millingProgressReceived(quint64 taskId,
                                 float progress,
                                 bool finished,
                                 bool success,
                                 const QString &message);

    // PLC 反馈（后端 → 前端）：状态字、路径计数、故障码、当前位置/速度/主轴参数。
    void plcFeedbackReceived(const PlcFeedbackVM &feedback);

    void plcPathCommandReceived(const PlcPathCommandParamsVM &command);

private:
    void handleBackendState(const std::string &state);

private:
    std::shared_ptr<rclcpp::Node> m_node;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::shared_ptr<Ros2BridgeEntities> m_entities;
    std::thread m_spinThread;
    bool m_running{false};

    std::mutex m_readerMutex;
    std::unique_ptr<ShmImageReader> m_shmReader;
};

#endif // ROS2BRIDGE_H
