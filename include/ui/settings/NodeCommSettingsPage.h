#ifndef NODECOMMSETTINGSPAGE_H
#define NODECOMMSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

#include "camera/HikCameraConfig.h"

#include <QDateTime>
#include <QString>

class ElaPushButton;
class QButtonGroup;
class QCheckBox;
class QDialog;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QWidget;

// 「设备与通信」设置页。
// 顶部核心服务状态条 + 左窄右宽主从面板
// （左侧设备节点列表，右侧海康相机配置 + 数据存储策略）。
// 配色跟随 Theme，切换深色/浅色时重刷。
class NodeCommSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit NodeCommSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;
    bool isEditable() const override;
    bool isDirty() const override;
    SettingsSaveOutcome save() override;
    void revert() override;
    void restoreDefaults() override;
    QString pendingChangeSummary() const override;

public slots:
    // 由 MainWindow 回传：节点进程是否在跑 / 启停脚本是否执行中，用于刷新按钮与状态文字。
    void setNodesRunning(bool running);
    void setNodesBusy(bool busy);
    void setCameraNodeStatus(bool connected, const QString &detail);
    void setCameraTopicDiagnostics(bool bridgeRunning, int publisherCount);
    void setHikCameraStatus(bool connected, const QString &detail);
    void setPlcDeviceStatus(const QString &status, const QString &detail = QString());
    void refreshNodeReadiness();

signals:
    void startNodesRequested();
    void stopNodesRequested();
    void hikCameraConnectRequested();
    void hikCameraDisconnectRequested();

protected:
    void reloadFromSource() override;

private:
    void rebuildUi();
    void applyTheme();
    void updateServiceStatusBar();
    void updateNodeList();
    void updateCameraNodeDiagnostics();
    void refreshDiskUsage();
    void refreshHikCameraInfo();

    // 顶部状态条
    QLabel *m_serviceSummary{nullptr};
    QPushButton *m_startAllButton{nullptr};
    QPushButton *m_redetectButton{nullptr};

    // 左侧节点列表
    QListWidget *m_nodeList{nullptr};
    QLabel *m_cameraNodeBadge{nullptr};
    QLabel *m_topicNodeBadge{nullptr};
    QLabel *m_plcBadge{nullptr};
    QLabel *m_cncBadge{nullptr};
    QLabel *m_storageBadge{nullptr};

    // 右侧相机配置
    QFrame *m_cameraCard{nullptr};
    QLineEdit *m_cameraIp{nullptr};
    QLineEdit *m_cameraTopic{nullptr};
    QButtonGroup *m_triggerModeGroup{nullptr};
    QPushButton *m_softTriggerButton{nullptr};
    QPushButton *m_hardTriggerButton{nullptr};
    QSpinBox *m_timeoutSpin{nullptr};
    QCheckBox *m_autoReconnect{nullptr};
    QPushButton *m_cameraTestButton{nullptr};
    QPushButton *m_cameraPingButton{nullptr};
    QPushButton *m_cameraApplyButton{nullptr};
    QLabel *m_cameraStateHint{nullptr};

    // 右侧数据存储
    QFrame *m_storageCard{nullptr};
    QLabel *m_dbStatusValue{nullptr};
    QPushButton *m_dbViewButton{nullptr};
    QLineEdit *m_imageRoot{nullptr};
    QPushButton *m_browseButton{nullptr};
    QButtonGroup *m_saveModeGroup{nullptr};
    QPushButton *m_saveNgOnlyButton{nullptr};
    QPushButton *m_saveAllButton{nullptr};
    QPushButton *m_saveNoneButton{nullptr};
    QProgressBar *m_diskBar{nullptr};
    QCheckBox *m_autoClean{nullptr};
    QSpinBox *m_cleanMinGbSpin{nullptr};
    QSpinBox *m_cleanMaxDaysSpin{nullptr};
    QDialog *m_databaseSchemaDialog{nullptr};
    QTableWidget *m_databaseSchemaTable{nullptr};

    // 运行态
    bool m_running{false};
    bool m_busy{false};
    bool m_cameraNodeConnected{false};
    bool m_rosBridgeRunning{false};
    bool m_hikConnected{false};
    bool m_dirty{false};
    bool m_loading{false};
    QDateTime m_cameraLastMessageAt;
    QString m_cameraStatusMessage;
    QString m_plcStatusText;
    QString m_plcStatusDetail;
    int m_cameraPublisherCount{-1};
    bool m_cameraStatusReceived{false};
    HikCameraConfig m_config;
};

#endif // NODECOMMSETTINGSPAGE_H
