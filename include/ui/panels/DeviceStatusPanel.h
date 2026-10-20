#ifndef DEVICESTATUSPANEL_H
#define DEVICESTATUSPANEL_H

#include <QFrame>

class QLabel;
class QTimer;

// 底部状态栏：左侧 3D相机 / PLC / 修磨机 / 坐标标定 四个连接状态，右侧版权与版本。
class DeviceStatusPanel : public QFrame
{
    Q_OBJECT

public:
    explicit DeviceStatusPanel(QWidget *parent = nullptr);

    // 相机连接状态（由主窗口在收到相机状态时调用）。
    void setCameraStatus(bool connected,
                         const QString &cameraId,
                         const QString &message);

    // 后端在线：收到后端转发的 PLC 反馈时调用。
    void setBackendOnline(const QString &tooltip);
    // 后端状态文本（backend_state topic），显示在「修磨机」项上。
    void setBackendState(const QString &state);

    // PLC 状态：依据状态字 + 故障码呈现。
    void setPlcStatus(quint16 statusWord, quint16 faultCode);
    void clearBackendAndPlcStatus();

    // 坐标标定：当前只有 XYZ 补偿，已保存即视为「已设置」。
    void setCalibrationStatus(bool calibrated, const QString &detail);

private:
    enum class ConnectionState { Unknown, Online, Offline };

    struct StatusItem
    {
        QLabel *dot{nullptr};
        QLabel *value{nullptr};
    };

    QWidget *createStatusItem(const QString &name, StatusItem &item, QWidget *parent);
    void applyStatusItem(StatusItem &item,
                         const QString &color,
                         const QString &text,
                         const QString &tooltip);
    void applyCameraStatus(ConnectionState state, const QString &text, const QString &detail);

    StatusItem m_camera;
    StatusItem m_plc;
    StatusItem m_machine;
    StatusItem m_calibration;
    QTimer *m_cameraStatusTimer{nullptr};
    QString m_backendStateText;
    bool m_backendOnline{false};
};

#endif // DEVICESTATUSPANEL_H
