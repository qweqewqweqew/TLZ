#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include "camera/HikCameraParameters.h"
#include "bridge/SickCameraParametersVM.h"

#include <QWidget>
#include <QtGui/qwindowdefs.h>

struct PointCloudFrame;
class EventLogPanel;
class CalibrationPlaceholderPage;
class NodeCommSettingsPage;
class NodeLifecycleMonitor;
class QLabel;
class QListWidget;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QStackedWidget;
class Ros2Bridge;
class SettingsSubPage;
class VisionSettingsPage;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(Ros2Bridge *bridge,
                          NodeLifecycleMonitor *nodes,
                          EventLogPanel *eventLog,
                          QWidget *parent = nullptr);

    // 离开设置或切换子页前调用。保存失败或用户取消时返回 false。
    bool confirmLeave();

public slots:
    // 节点运行态回传给「节点与通信」页的启停控件。
    void setNodesRunning(bool running);
    void setNodesBusy(bool busy);
    void setCameraNodeStatus(bool connected, const QString &detail);
    void setCameraTopicDiagnostics(bool bridgeRunning, int publisherCount);
    void setHikCameraStatus(bool connected, const QString &detail);
    void setHikCameraParameters(const HikCameraParameters &parameters);
    void setSickCameraParameters(const SickCameraParametersVM &parameters);
    void setSickCameraParameterResult(bool success,
                                      const QString &message,
                                      const SickCameraParametersVM &parameters);
    void setPlcDeviceStatus(const QString &status, const QString &detail = QString());
    void setScanFrame(const PointCloudFrame &frame);

signals:
    void historyRequested();
    void logRequested(const QString &level, const QString &message);
    void startNodesRequested();
    void stopNodesRequested();
    void hikCameraConnectRequested();
    void hikCameraPreviewRequested(WId targetWindow);
    void hikCameraDisconnectRequested();
    void hikCameraRefreshParametersRequested();
    void hikCameraParametersUpdateRequested(const HikCameraParameters &parameters);
    void sickCameraParametersUpdateRequested(const SickCameraParametersVM &parameters);
    void refreshCameraDiagnosticsRequested();

private:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void updateNavCardSizes();
    void updateNavIcons();
    void updateNavSelection();
    void showPage(int row);
    void updateActions();
    void refreshAccess();
    void updateAccessBanner();
    void setStatus(const QString &text, bool problem);
    bool saveCurrent();
    bool canWrite(const SettingsSubPage *page) const;
    QString pageTitle(int row) const;
    SettingsSubPage *currentPage() const;

    QListWidget *m_nav{nullptr};
    QStackedWidget *m_stack{nullptr};
    NodeCommSettingsPage *m_nodeCommPage{nullptr};
    VisionSettingsPage *m_visionPage{nullptr};
    CalibrationPlaceholderPage *m_calibrationPage{nullptr};
    QLabel *m_pageTitle{nullptr};
    QLabel *m_accessBanner{nullptr};
    QLabel *m_status{nullptr};
    QPushButton *m_saveButton{nullptr};
    QPushButton *m_cancelButton{nullptr};
    QPushButton *m_defaultButton{nullptr};
    int m_currentRow{0};
    bool m_updatingNav{false};
};

#endif // SETTINGSPAGE_H
