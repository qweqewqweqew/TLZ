#ifndef DEVICESTATUSPANEL_H
#define DEVICESTATUSPANEL_H

#include <QFrame>

class QLabel;
class QTimer;

// 底部状态栏：左侧显示当前时间，中间显示版权信息，右侧显示设备状态。
class DeviceStatusPanel : public QFrame
{
    Q_OBJECT

public:
    explicit DeviceStatusPanel(QWidget *parent = nullptr);

    // 相机连接状态（由主窗口在收到相机帧时调用）。
    void setCameraStatus(bool connected,
                         const QString &cameraId,
                         const QString &message);

    // 后端连接状态：收到后端发布的 PLC 反馈时调用。
    void setBackendOnline(const QString &tooltip);

    // PLC 状态：依据状态字 + 故障码呈现。
    void setPlcStatus(quint16 statusWord, quint16 faultCode);
    void clearBackendAndPlcStatus();

private:
    enum class ConnectionState { Unknown, Online, Offline };

    QWidget *createStatusItem(const QString &name,
                              QLabel *&dotLabel,
                              QWidget *parent);
    void applyStatusItem(QLabel *dotLabel,
                         const QString &color,
                         const QString &tooltip);
    void applyCameraStatus(ConnectionState state, const QString &detail);
    void updateCurrentTime();

    QLabel *m_clockLabel{nullptr};
    QLabel *m_cameraStatusDot{nullptr};
    QLabel *m_backendStatusDot{nullptr};
    QLabel *m_plcStatusDot{nullptr};
    QTimer *m_clockTimer{nullptr};
    QTimer *m_cameraStatusTimer{nullptr};
};

#endif // DEVICESTATUSPANEL_H
