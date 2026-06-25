#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class EventLogPanel;
class ImageDisplayPanel;
class RunDataPanel;
class RunningStatusPanel;
class QTimer;
class TelemetryWebSocketServer;
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
    void startTelemetryServer();
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
    QTimer *m_waveformTimer{nullptr};
    TelemetryWebSocketServer *m_telemetryServer{nullptr};
    double m_currentT{0};
    bool m_simulationRunning{false};
};

#endif // MAINWINDOW_H
