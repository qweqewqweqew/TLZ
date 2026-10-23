#ifndef NODECOMMSETTINGSPAGE_H
#define NODECOMMSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

#include <QDateTime>

class ElaPushButton;
class QDialog;
class QFrame;
class QLabel;
class QToolButton;
class QTableWidget;
class QWidget;

class NodeCommSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit NodeCommSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;

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
    void updateNodeControls();
    void checkNodeReadiness();
    void refreshDatabaseSchema();
    void refreshHikCameraInfo();
    void updateCameraNodeDiagnostics();

    QLabel *m_nodeState{nullptr};
    QLabel *m_readinessSummary{nullptr};
    QLabel *m_cameraReadiness{nullptr};
    QLabel *m_backendReadiness{nullptr};
    QLabel *m_algorithmReadiness{nullptr};
    QFrame *m_cameraCard{nullptr};
    QFrame *m_backendCard{nullptr};
    QFrame *m_algorithmCard{nullptr};
    ElaPushButton *m_startButton{nullptr};
    ElaPushButton *m_stopButton{nullptr};
    ElaPushButton *m_checkButton{nullptr};
    QWidget *m_databaseSummary{nullptr};
    QLabel *m_databaseStatusValue{nullptr};
    QLabel *m_imageStorageValue{nullptr};
    QTableWidget *m_databaseSchemaTable{nullptr};
    QDialog *m_databaseSchemaDialog{nullptr};
    ElaPushButton *m_databaseSchemaButton{nullptr};
    ElaPushButton *m_openImageFolderButton{nullptr};
    QLabel *m_cameraNodeStatus{nullptr};
    QLabel *m_hikCameraStatus{nullptr};
    QLabel *m_plcStatus{nullptr};
    QToolButton *m_cameraNodeToggle{nullptr};
    QToolButton *m_hikCameraToggle{nullptr};
    QToolButton *m_plcToggle{nullptr};
    QWidget *m_cameraNodeDetails{nullptr};
    QWidget *m_hikCameraDetails{nullptr};
    QWidget *m_plcDetails{nullptr};
    QLabel *m_cameraPublisherCount{nullptr};
    QLabel *m_cameraLastMessage{nullptr};
    QLabel *m_cameraDiagnosis{nullptr};
    QLabel *m_hikAddress{nullptr};
    QLabel *m_hikUsername{nullptr};
    QLabel *m_hikEndpoint{nullptr};
    QLabel *m_hikStream{nullptr};
    QLabel *m_hikConfigState{nullptr};
    ElaPushButton *m_hikConnectButton{nullptr};
    ElaPushButton *m_hikDisconnectButton{nullptr};
    bool m_rosBridgeRunning{false};
    int m_cameraStatusPublisherCount{-1};
    bool m_cameraStatusReceived{false};
    bool m_cameraNodeConnected{false};
    QString m_cameraStatusMessage;
    QDateTime m_cameraLastMessageAt;
    bool m_hikConnected{false};
    bool m_hikConnecting{false};
    bool m_running{false};
    bool m_busy{false};
    bool m_allReady{false};
};

#endif // NODECOMMSETTINGSPAGE_H
