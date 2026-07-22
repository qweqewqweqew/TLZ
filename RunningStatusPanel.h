#ifndef RUNNINGSTATUSPANEL_H
#define RUNNINGSTATUSPANEL_H

#include <QFrame>
#include <QHash>
#include <QElapsedTimer>

class QColor;
class QLabel;
struct PlcFeedbackVM;
class TelemetryPlotWidget;

class RunningStatusPanel : public QFrame
{
    Q_OBJECT

public:
    explicit RunningStatusPanel(QWidget *parent = nullptr);

    void appendSample(double time, double speed, double torque);
    void setPlcFeedback(const PlcFeedbackVM &feedback);

private:
    enum class MetricState { NoData, Normal, Warning, Critical };

    QWidget *createSpindleGrid(QWidget *parent);
    QWidget *createPositionGrid(const QString &title,
                                const QString &keyPrefix,
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

private:
    QHash<QString, QLabel *> m_metricLabels;
    TelemetryPlotWidget *m_speedPlot{nullptr};
    TelemetryPlotWidget *m_torquePlot{nullptr};
    QElapsedTimer m_plcElapsedTimer;
};

#endif // RUNNINGSTATUSPANEL_H
