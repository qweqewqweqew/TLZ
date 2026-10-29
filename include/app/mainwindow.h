#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "bridge/MillingPathVM.h"
#include <QHash>
#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QVector>

class DeviceStatusPanel;
class DataStatisticsPanel;
class EventLogPanel;
class HistoryDialog;
class HikCameraClient;
class HikPreviewDialog;
class ImageDisplayPanel;
class KpiStripPanel;
class MachineStatusPanel;
class NavigationBar;
class NotificationDrawer;
class ParticleDetailPanel;
class PathQueuePanel;
class ReportPage;
class Ros2Bridge;
class SettingsPage;
class QStackedWidget;
class QTimer;
class TitleBar;
class NodeLifecycleMonitor;
class QProcess;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setDatabaseReady(bool ready);

public slots:
    void appendTelemetrySample(double speed, double torque);

private:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

    void buildMainView();
    void connectTitleBar();
    void showUserMenu(const QPoint &anchor);
    void refreshUserIdentity();
    void showHistoryPage();
    // 左侧导航路由；设置页有未保存修改时会拒绝切换并把导航高亮复位。
    void showPage(int page);
    void updateDrawerDockArea();
    void refreshCalibrationStatus();
    void startRos2Bridge();
    // Ctrl+T / 工具栏「修磨演示」：加载 TestImage 并播放假任务；再次调用退出。
    void toggleDemoMode();
    void updateKpis();
    void startNodes();
    void stopNodes();
    void runNodeLauncherScript(bool start);
    void stopNodesOnExit();
    QString resolveLauncherConfigPath() const;
    QString resolveLauncherScript(const QString &key) const;
    QString resolveStartAllScript() const;
    QString resolveStopAllScript() const;
    void appendEventLog(const QString &level, const QString &message);
    void persistTaskRecord(quint64 taskId);
    void persistScanFrame(quint64 taskId,
                          quint64 frameId,
                          quint64 timestampNs,
                          int width,
                          int height,
                          int rangePixelFormat,
                          int intensityPixelFormat,
                          bool hasRange,
                          bool hasIntensity);
    void flushPendingDatabaseWrites();
    void toggleMaximized();
    void setSimulationRunning(bool running);
    void handleMillingProgress(quint64 taskId,
                               float progress,
                               bool finished,
                               bool success,
                               const QString &message);
    void applyMillingProgress(quint64 taskId,
                              float progress,
                              bool finished,
                              bool success,
                              const QString &message);

    struct PendingMillingProgress
    {
        quint64 taskId{0};
        float progress{0.0f};
        bool finished{false};
        bool success{false};
        QString message;
        bool valid{false};
    };

private slots:
    void onWaveformUpdate();
    void onSimulationProgressUpdate();
    void onDemoPoseUpdate();
    void resetDemoPoseAnimation();
    // 演示模式仍走本地动画。否则把启停发给后端 milling/command。
    void requestMillingStart();
    void requestMillingPause();
    void requestMillingStop();
    void startDemoMilling();
    void pauseDemoMilling();
    void stopDemoMilling();
    void applyMillingCommandResult(quint8 command,
                                   quint64 taskId,
                                   bool accepted,
                                   const QString &message);

private:
    struct PendingScanFrame
    {
        quint64 taskId{0};
        quint64 frameId{0};
        quint64 timestampNs{0};
        int width{0};
        int height{0};
        int rangePixelFormat{0};
        int intensityPixelFormat{0};
        bool hasRange{false};
        bool hasIntensity{false};
    };

    Ui::MainWindow *ui;
    TitleBar *m_titleBar{nullptr};
    NavigationBar *m_navBar{nullptr};
    NotificationDrawer *m_drawer{nullptr};
    QStackedWidget *m_contentStack{nullptr};
    QWidget *m_monitorPage{nullptr};
    ReportPage *m_reportPage{nullptr};
    SettingsPage *m_settingsPage{nullptr};
    KpiStripPanel *m_kpiPanel{nullptr};
    DeviceStatusPanel *m_deviceStatusPanel{nullptr};
    ImageDisplayPanel *m_imagePanel{nullptr};
    ParticleDetailPanel *m_particlePanel{nullptr};
    MachineStatusPanel *m_machinePanel{nullptr};
    PathQueuePanel *m_queuePanel{nullptr};
    HikCameraClient *m_hikCameraClient{nullptr};
    HikPreviewDialog *m_hikPreviewDialog{nullptr};
    DataStatisticsPanel *m_dataStatisticsPanel{nullptr};
    QVector<MillingPathVM> m_activePaths;
    int m_activePathTotal{0};
    int m_activeCompletedPath{0};
    QString m_activeBoardGrade;
    QTimer *m_waveformTimer{nullptr};
    QTimer *m_simulationProgressTimer{nullptr};
    Ros2Bridge *m_ros2Bridge{nullptr};
    NodeLifecycleMonitor *m_nodeLifecycleMonitor{nullptr};
    QProcess *m_startAllProcess{nullptr};
    HistoryDialog *m_historyDialog{nullptr};
    // 最近一次 MillingPaths 给的后端任务号，作为扫描帧落库时 task_id 的回退值
    quint64 m_currentBackendTaskId{0};
    quint64 m_activeBackendTaskId{0};
    quint64 m_lastLoggedIgnoredProgressTaskId{0};
    float m_lastMillingProgress{-1.0f};
    bool m_activeTaskFinished{false};
    quint8 m_lastMillingCommand{0};
    quint64 m_lastMillingCommandTaskId{0};
    QHash<quint64, PendingMillingProgress> m_pendingMillingProgress;
    QSet<quint64> m_pendingDatabaseTaskIds;
    QVector<PendingScanFrame> m_pendingDatabaseFrames;
    double m_currentT{0};
    float m_simulationProgress{0.0f};
    bool m_simulationRunning{false};
    bool m_debugFakeActive{false};
    QVector<MillingPathVM> m_debugPaths;
    int m_demoPathIndex{0};
    double m_demoPathT{0.0}; // 0..1 along current segment
    QTimer *m_demoPoseTimer{nullptr};
    double m_debugXScaleMm{0.5};
    double m_debugYScaleMm{0.5};
    double m_debugXOffsetMm{0.0};
    double m_debugYOffsetMm{0.0};
    bool m_databaseReady{false};
    bool m_stopScriptLaunched{false};
    bool m_hikPreviewConnected{false};
    bool m_shuttingDown{false};
    QString m_lastIdentity;
};

#endif // MAINWINDOW_H
