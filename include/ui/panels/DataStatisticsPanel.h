#ifndef DATASTATISTICSPANEL_H
#define DATASTATISTICSPANEL_H

#include <QFrame>

class QCustomPlot;
class QLabel;
class QWheelEvent;

class DataStatisticsPanel : public QFrame
{
    Q_OBJECT

public:
    explicit DataStatisticsPanel(QWidget *parent = nullptr);

    void appendSpindleSample(double speed, double torque);

private:
    enum class SelectedMetric { Speed, Torque };

    void setupPlot();
    void setSelectedMetric(SelectedMetric metric);
    void applyViewRange();
    void onPlotWheel(QWheelEvent *event);
    void setPaused(bool paused);

    QCustomPlot *m_plot{nullptr};
    QLabel *m_pauseHint{nullptr};
    double m_latestTime{0.0};
    bool m_followLive{true};
    bool m_rangeDragging{false};
    bool m_paused{false};
};

#endif // DATASTATISTICSPANEL_H
