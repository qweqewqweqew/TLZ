#ifndef RUNNINGSTATUSPANEL_H
#define RUNNINGSTATUSPANEL_H

#include <QFrame>
#include <QHash>

class QLabel;
struct PlcFeedbackVM;
class ElaProgressRing;
class VerticalGauge;

class RunningStatusPanel : public QFrame
{
    Q_OBJECT

public:
    explicit RunningStatusPanel(QWidget *parent = nullptr);

    void appendSample(double time, double speed, double torque);
    void setPlcFeedback(const PlcFeedbackVM &feedback);
    void setTaskProgress(float progress);
    void clearTaskProgress();
    void clearPlcFeedback();

private:
    enum class AxisMotion {
        Unknown,
        Stopped,
        Positive,
        Negative,
    };

    QWidget *createOverviewRow(QWidget *parent);
    QWidget *createGaugeRow(QWidget *parent);
    QWidget *createTaskProgressBlock(QWidget *parent);
    QWidget *createPositionGrid(const QString &title,
                                const QString &keyPrefix,
                                QWidget *parent);
    QWidget *createPositionBlock(const QString &axis,
                                 const QString &keyPrefix,
                                 QWidget *parent);
    QWidget *createPlcStatusBlock(QWidget *parent);
    void setPositionValue(const QString &key, const QString &value);
    void updatePlcStatus(quint16 statusWord, quint16 faultCode);
    void clearPlcStatus();
    AxisMotion nextAxisMotion(AxisMotion last, double speed) const;
    void applyAxisSpeed(VerticalGauge *gauge, AxisMotion &last, double speed);

    QHash<QString, QLabel *> m_metricLabels;
    VerticalGauge *m_spindleSpeedGauge{nullptr};
    VerticalGauge *m_spindleTorqueGauge{nullptr};
    VerticalGauge *m_xSpeedGauge{nullptr};
    VerticalGauge *m_ySpeedGauge{nullptr};
    VerticalGauge *m_zSpeedGauge{nullptr};
    AxisMotion m_xMotion{AxisMotion::Unknown};
    AxisMotion m_yMotion{AxisMotion::Unknown};
    AxisMotion m_zMotion{AxisMotion::Unknown};
    QFrame *m_plcStatusFrame{nullptr};
    QLabel *m_plcStatusText{nullptr};
    ElaProgressRing *m_taskProgressRing{nullptr};
};

#endif // RUNNINGSTATUSPANEL_H
