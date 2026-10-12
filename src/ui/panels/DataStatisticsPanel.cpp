#include "ui/panels/DataStatisticsPanel.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "qcustomplot.h"

#include <QDateTime>
#include <QFont>
#include <QLabel>
#include <QMouseEvent>
#include <QPen>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {

constexpr double kVisibleSeconds = 30.0;
constexpr double kHistorySeconds = 600.0;
constexpr double kWheelStepSeconds = 15.0;
constexpr double kSpeedMaximum = 3500.0;
constexpr double kTorqueMaximum = 35.0;
constexpr double kNormalizedMaximum = 100.0;

void styleAxis(QCPAxis *axis, const QColor &color)
{
    axis->setBasePen(QPen(Theme::kBorder));
    axis->setTickPen(QPen(Theme::kBorder));
    axis->setSubTickPen(QPen(Theme::kDivider));
    axis->setTickLabelColor(color);
    axis->setLabelColor(color);
    axis->setTickLabelFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    axis->setLabelFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
}

} // namespace

DataStatisticsPanel::DataStatisticsPanel(QWidget *parent)
    : QFrame(parent)
{
    m_pauseHint = makeLabel(QStringLiteral("已暂停"), QStringLiteral("sectionHint"));
    m_pauseHint->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::kStateWarning));
    m_pauseHint->setVisible(false);
    auto *panel = createPanel(QStringLiteral("动态特性曲线"), m_pauseHint, this);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    setMinimumHeight(270);
    setMaximumHeight(340);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    m_plot = new QCustomPlot(panel);
    m_plot->setToolTip(QStringLiteral("右键暂停 / 继续"));
    setupPlot();
    panelLayout->addWidget(m_plot, 1);
}

void DataStatisticsPanel::appendSpindleSample(double speed, double torque)
{
    if (!m_plot) {
        return;
    }

    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    m_latestTime = now;
    const double normalizedSpeed = qBound(0.0, speed, kSpeedMaximum)
                                   / kSpeedMaximum * kNormalizedMaximum;
    const double normalizedTorque = qBound(0.0, torque, kTorqueMaximum)
                                    / kTorqueMaximum * kNormalizedMaximum;
    m_plot->graph(0)->addData(now, normalizedSpeed);
    m_plot->graph(1)->addData(now, normalizedTorque);
    m_plot->graph(0)->data()->removeBefore(now - kHistorySeconds);
    m_plot->graph(1)->data()->removeBefore(now - kHistorySeconds);
    if (m_paused) {
        return;
    }
    applyViewRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void DataStatisticsPanel::setupPlot()
{
    const QColor speedColor(Theme::kRealtime);
    const QColor torqueColor(Theme::kAccent);
    const QColor mutedColor(Theme::kTextMuted);

    m_plot->setBackground(QColor(Theme::kCanvasBg));
    m_plot->axisRect()->setBackground(QColor(Theme::kCanvasBg));

    auto dateTicker = QSharedPointer<QCPAxisTickerDateTime>(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat(QStringLiteral("HH:mm:ss"));
    m_plot->xAxis->setTicker(dateTicker);
    m_plot->xAxis->setLabel(QString());

    m_plot->yAxis->setRange(0.0, kNormalizedMaximum);
    m_plot->yAxis2->setVisible(false);

    styleAxis(m_plot->xAxis, mutedColor);
    m_plot->xAxis->grid()->setPen(QPen(QColor(Theme::kDivider), 1, Qt::DotLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor(Theme::kDivider), 1, Qt::DotLine));

    auto *speedGraph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    speedGraph->setName(QStringLiteral("主轴转速"));
    speedGraph->setLineStyle(QCPGraph::lsLine);
    speedGraph->setBrush(Qt::NoBrush);
    speedGraph->setPen(QPen(speedColor, 1.75));
    speedGraph->setAntialiased(true);

    auto *torqueGraph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    torqueGraph->setName(QStringLiteral("主轴扭矩"));
    torqueGraph->setLineStyle(QCPGraph::lsLine);
    torqueGraph->setBrush(Qt::NoBrush);
    torqueGraph->setPen(QPen(torqueColor, 1.75));
    torqueGraph->setAntialiased(true);

    m_plot->legend->setVisible(true);
    m_plot->legend->setBrush(QColor(Theme::kCanvasBg));
    m_plot->legend->setBorderPen(Qt::NoPen);
    m_plot->legend->setTextColor(QColor(Theme::kTextSecondary));
    m_plot->legend->setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignLeft);
    m_plot->setSelectionTolerance(12);
    m_plot->setContextMenuPolicy(Qt::NoContextMenu);
    m_plot->setInteractions(QCP::iRangeDrag);
    m_plot->axisRect()->setRangeDrag(Qt::Horizontal);
    m_plot->axisRect()->setRangeZoom(Qt::Orientations());

    if (auto *item = m_plot->legend->itemWithPlottable(speedGraph)) {
        item->setTextColor(speedColor);
    }
    if (auto *item = m_plot->legend->itemWithPlottable(torqueGraph)) {
        item->setTextColor(torqueColor);
    }

    connect(m_plot, &QCustomPlot::plottableClick, this,
            [this, speedGraph, torqueGraph](QCPAbstractPlottable *plottable, int, QMouseEvent *) {
                if (plottable == speedGraph) {
                    setSelectedMetric(SelectedMetric::Speed);
                } else if (plottable == torqueGraph) {
                    setSelectedMetric(SelectedMetric::Torque);
                }
            });
    connect(m_plot, &QCustomPlot::legendClick, this,
            [this, speedGraph, torqueGraph](QCPLegend *, QCPAbstractLegendItem *item, QMouseEvent *) {
                auto *plottableItem = qobject_cast<QCPPlottableLegendItem *>(item);
                if (!plottableItem) {
                    return;
                }
                if (plottableItem->plottable() == speedGraph) {
                    setSelectedMetric(SelectedMetric::Speed);
                } else if (plottableItem->plottable() == torqueGraph) {
                    setSelectedMetric(SelectedMetric::Torque);
                }
            });

    connect(m_plot, &QCustomPlot::mouseWheel, this,
            [this](QWheelEvent *event) { onPlotWheel(event); });
    connect(m_plot, &QCustomPlot::mousePress, this, [this](QMouseEvent *event) {
        if (event->button() == Qt::RightButton) {
            setPaused(!m_paused);
        }
    });
    connect(m_plot, &QCustomPlot::mouseMove, this, [this](QMouseEvent *event) {
        if (event->buttons() & Qt::LeftButton) {
            m_rangeDragging = true;
            if (!m_paused) {
                m_followLive = false;
            }
        }
    });
    connect(m_plot, &QCustomPlot::mouseRelease, this, [this](QMouseEvent *event) {
        m_rangeDragging = false;
        if (event->button() != Qt::LeftButton || !m_plot || m_latestTime <= 0.0 || m_paused) {
            return;
        }
        m_followLive = m_plot->xAxis->range().upper >= m_latestTime - 0.5;
        applyViewRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });

    setSelectedMetric(SelectedMetric::Torque);

    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    m_plot->xAxis->setRange(now - kVisibleSeconds, now);
}

void DataStatisticsPanel::setSelectedMetric(SelectedMetric metric)
{
    if (!m_plot || m_plot->graphCount() < 2) {
        return;
    }

    const bool speedSelected = metric == SelectedMetric::Speed;
    const QColor selectedColor(speedSelected ? Theme::kRealtime : Theme::kAccent);

    auto ticker = QSharedPointer<QCPAxisTickerText>(new QCPAxisTickerText);
    ticker->setSubTickCount(4);
    const QVector<double> tickPositions{0.0, 25.0, 50.0, 75.0, 100.0};
    if (speedSelected) {
        ticker->addTicks(tickPositions,
                         QVector<QString>{QStringLiteral("0"), QStringLiteral("875"),
                                          QStringLiteral("1750"), QStringLiteral("2625"),
                                          QStringLiteral("3500")});
        m_plot->yAxis->setLabel(QStringLiteral("RPM"));
    } else {
        ticker->addTicks(tickPositions,
                         QVector<QString>{QStringLiteral("0"), QStringLiteral("7.5"),
                                          QStringLiteral("15"), QStringLiteral("22.5"),
                                          QStringLiteral("30")});
        m_plot->yAxis->setLabel(QStringLiteral("N·m"));
    }

    m_plot->yAxis->setTicker(ticker);
    styleAxis(m_plot->yAxis, selectedColor);

    QColor speedPen(Theme::kRealtime);
    QColor torquePen(Theme::kAccent);
    if (!speedSelected) {
        speedPen.setAlpha(102); // ~40% when unselected
    }
    if (speedSelected) {
        torquePen.setAlpha(102);
    }
    m_plot->graph(0)->setBrush(Qt::NoBrush);
    m_plot->graph(1)->setBrush(Qt::NoBrush);
    m_plot->graph(0)->setLineStyle(QCPGraph::lsLine);
    m_plot->graph(1)->setLineStyle(QCPGraph::lsLine);
    m_plot->graph(0)->setPen(QPen(speedPen, speedSelected ? 2.0 : 1.5));
    m_plot->graph(1)->setPen(QPen(torquePen, speedSelected ? 1.5 : 2.0));
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void DataStatisticsPanel::applyViewRange()
{
    if (!m_plot || m_latestTime <= 0.0 || m_rangeDragging) {
        return;
    }
    if (m_followLive && !m_paused) {
        m_plot->xAxis->setRange(m_latestTime - kVisibleSeconds, m_latestTime);
        return;
    }

    const QCPRange range = m_plot->xAxis->range();
    const double oldest = m_latestTime - kHistorySeconds;
    double upper = range.upper;
    double lower = range.lower;
    if (upper - lower < 1.0) {
        upper = m_latestTime;
        lower = m_latestTime - kVisibleSeconds;
    }
    if (!m_paused && upper > m_latestTime) {
        upper = m_latestTime;
        lower = upper - kVisibleSeconds;
        m_followLive = true;
    } else if (lower < oldest) {
        lower = oldest;
        upper = lower + kVisibleSeconds;
    } else {
        upper = lower + kVisibleSeconds;
        if (!m_paused && upper > m_latestTime) {
            upper = m_latestTime;
            lower = upper - kVisibleSeconds;
            m_followLive = true;
        } else if (upper > m_latestTime) {
            upper = m_latestTime;
            lower = upper - kVisibleSeconds;
        }
    }
    m_plot->xAxis->setRange(lower, upper);
}

void DataStatisticsPanel::onPlotWheel(QWheelEvent *event)
{
    if (!m_plot || m_latestTime <= 0.0 || event->angleDelta().y() == 0) {
        return;
    }

    const double steps = double(event->angleDelta().y()) / 120.0;
    const QCPRange range = m_plot->xAxis->range();
    const double shift = -steps * kWheelStepSeconds;
    double lower = range.lower + shift;
    double upper = range.upper + shift;
    const double oldest = m_latestTime - kHistorySeconds;

    if (!m_paused && upper >= m_latestTime) {
        m_followLive = true;
        applyViewRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    if (!m_paused) {
        m_followLive = false;
    }
    if (upper > m_latestTime) {
        upper = m_latestTime;
        lower = upper - kVisibleSeconds;
    }
    if (lower < oldest) {
        lower = oldest;
        upper = oldest + kVisibleSeconds;
    }
    m_plot->xAxis->setRange(lower, upper);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void DataStatisticsPanel::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    if (m_pauseHint) {
        m_pauseHint->setVisible(paused);
    }
    if (!m_plot) {
        return;
    }
    if (!paused) {
        m_followLive = true;
        applyViewRange();
    } else {
        m_followLive = false;
    }
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}
