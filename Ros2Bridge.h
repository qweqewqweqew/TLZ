#ifndef ROS2BRIDGE_H
#define ROS2BRIDGE_H

#include <QImage>
#include <QObject>
#include <QString>

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
                            const QString &shmName,
                            quint64 offset,
                            quint64 dataSize,
                            quint32 width,
                            quint32 height,
                            quint32 pixelFormat);
    // 从共享内存读出的一帧：range 为 Mono16 拉伸后的 Grayscale8，
    // intensity 为 Mono8 原始像素；任一未提供时对应 QImage 可能为空。
    void scanFrameReady(const QImage &range,
                        const QImage &intensity,
                        quint64 frameId,
                        quint64 timestampNs,
                        quint32 width,
                        quint32 height,
                        quint32 pixelFormat);
    void algorithmResultReceived(bool success,
                                 quint64 frameId,
                                 quint64 taskId,
                                 const QString &message);
    void backendStateReceived(const QString &state);

private:
    void handleAlgorithmResult(bool success,
                               std::uint64_t frameId,
                               std::uint64_t taskId,
                               const std::string &message);
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
