#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class EventLogPanel;
class HistoryDialog;
class ImageDisplayPanel;
class Ros2Bridge;
class RunDataPanel;
class RunningStatusPanel;
class TaskFlowPanel;
class TeachingDialog;
class QTimer;
class TitleBar;

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

public slots:
    void appendTelemetrySample(double speed, double torque);

private:
    void changeEvent(QEvent *event) override;

    void buildMainView();
    void connectTitleBar();
    void startRos2Bridge();
    void appendEventLog(const QString &level, const QString &message);
    void toggleMaximized();
    void setSimulationRunning(bool running);

private slots:
    void onWaveformUpdate();

private:
    Ui::MainWindow *ui;
    TitleBar *m_titleBar{nullptr};
    RunningStatusPanel *m_statusPanel{nullptr};
    ImageDisplayPanel *m_imagePanel{nullptr};
    EventLogPanel *m_eventLogPanel{nullptr};
    RunDataPanel *m_runDataPanel{nullptr};
    TaskFlowPanel *m_taskFlowPanel{nullptr};
    QTimer *m_waveformTimer{nullptr};
    Ros2Bridge *m_ros2Bridge{nullptr};
    TeachingDialog *m_teachingDialog{nullptr};
    HistoryDialog *m_historyDialog{nullptr};
    double m_currentT{0};
    bool m_simulationRunning{false};
    bool m_debugFakeActive{false};
};

#endif // MAINWINDOW_H
