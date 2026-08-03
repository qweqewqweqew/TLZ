#ifndef RUNNINGSTATUSPANEL_H
#define RUNNINGSTATUSPANEL_H

#include <QFrame>
#include <QHash>
#include <QElapsedTimer>

class QColor;
class QLabel;
class QTimer;
struct PlcFeedbackVM;
class TelemetryPlotWidget;

class RunningStatusPanel : public QFrame
{
    Q_OBJECT

public:
    explicit RunningStatusPanel(QWidget *parent = nullptr);

    void appendSample(double time, double speed, double torque);
    void setPlcFeedback(const PlcFeedbackVM &feedback);
    void setCameraStatus(bool connected,
                         const QString &cameraId,
                         const QString &message);

private:
    enum class MetricState { NoData, Normal, Warning, Critical };
    enum class ConnectionState { Unknown, Online, Offline };

    QWidget *createSpindleGrid(QWidget *parent);
    QWidget *createPositionGrid(const QString &title,
                                const QString &keyPrefix,
                                QWidget *parent);
    QWidget *createDeviceStatusSection(QWidget *parent);
    QWidget *createStatusItem(const QString &name,
                              QLabel *&dotLabel,
                              QLabel *&statusLabel,
                              QWidget *parent);
    QWidget *createMetricCard(const QString &key,
                              const QString &title,
                              const QString &value,
                              const QString &unit,
                              const QColor &accent,
                              QWidget *parent);
    QWidget *createAxisCard(const QString &axis,
                            const QString &keyPrefix,
                            const QColor &accent,
                            QWidget *parent);
    void setMetricValue(const QString &key,
                        const QString &value,
                        MetricState state = MetricState::Normal);
    void applyCameraStatus(ConnectionState state, const QString &detail);
    void applyStatusItem(QLabel *dotLabel,
                         QLabel *statusLabel,
                         const QString &text,
                         const QString &color,
                         const QString &tooltip);

private:
    QHash<QString, QLabel *> m_metricLabels;
    TelemetryPlotWidget *m_speedPlot{nullptr};
    TelemetryPlotWidget *m_torquePlot{nullptr};
    QLabel *m_cameraStatusDot{nullptr};
    QLabel *m_cameraStatusText{nullptr};
    QLabel *m_backendStatusDot{nullptr};
    QLabel *m_backendStatusText{nullptr};
    QLabel *m_plcStatusDot{nullptr};
    QLabel *m_plcStatusText{nullptr};
    QTimer *m_cameraStatusTimer{nullptr};
    QElapsedTimer m_plcElapsedTimer;
};

#endif // RUNNINGSTATUSPANEL_H
