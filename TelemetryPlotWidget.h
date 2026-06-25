#ifndef TELEMETRYPLOTWIDGET_H
#define TELEMETRYPLOTWIDGET_H

#include <QColor>
#include <QDateTime>
#include <QWidget>

class QCustomPlot;
class QCPItemStraightLine;
class QCPItemText;

struct TelemetryPlotConfig
{
    QString title;
    QString unit;
    QColor color;
    double yMin{0};
    double yMax{0};
    double alarmValue{0};
    QColor alarmColor;
};

class TelemetryPlotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TelemetryPlotWidget(const TelemetryPlotConfig &config, QWidget *parent = nullptr);

    void appendSample(double time, double value, double visibleSeconds);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupPlot();
    void showCursorAt(double x, bool frozen);
    void hideCursor();
    bool isVisibleTime(double x) const;
    QDateTime timeForX(double x) const;

private:
    TelemetryPlotConfig m_config;
    QCustomPlot *m_plot{nullptr};
    QCPItemStraightLine *m_cursorLine{nullptr};
    QCPItemText *m_timeLabel{nullptr};
    double m_currentTime{0};
    double m_visibleSeconds{30};
    bool m_cursorFrozen{false};
};

#endif // TELEMETRYPLOTWIDGET_H
