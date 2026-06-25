#include "TelemetryPlotWidget.h"

#include "UiHelpers.h"
#include "qcustomplot.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPen>
#include <QVBoxLayout>

TelemetryPlotWidget::TelemetryPlotWidget(const TelemetryPlotConfig &config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *label = makeLabel(QString("%1 (%2)").arg(m_config.title, m_config.unit));
    label->setStyleSheet(QString("color:%1;font-size:12px;font-weight:600;").arg(m_config.color.name()));
    layout->addWidget(label);

    m_plot = new QCustomPlot(this);
    m_plot->setMinimumHeight(176);
    m_plot->setMaximumHeight(210);
    setupPlot();
    layout->addWidget(m_plot);
}

void TelemetryPlotWidget::appendSample(double time, double value, double visibleSeconds)
{
    m_currentTime = time;
    m_visibleSeconds = visibleSeconds;

    m_plot->graph(0)->addData(time, value);
    m_plot->graph(0)->data()->removeBefore(time - visibleSeconds);
    m_plot->xAxis->setRange(time, visibleSeconds, Qt::AlignRight);
    m_plot->replot();
}

bool TelemetryPlotWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_plot && event->type() == QEvent::Leave && !m_cursorFrozen) {
        hideCursor();
        return QWidget::eventFilter(watched, event);
    }

    return QWidget::eventFilter(watched, event);
}

void TelemetryPlotWidget::setupPlot()
{
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(m_config.color, 1));
    m_plot->graph(0)->setLineStyle(QCPGraph::lsLine);
    m_plot->graph(0)->setAntialiased(true);
    m_plot->yAxis->setRange(m_config.yMin, m_config.yMax);
    m_plot->xAxis->setRange(0, m_visibleSeconds);

    auto *limit = new QCPItemStraightLine(m_plot);
    limit->point1->setCoords(0, m_config.alarmValue);
    limit->point2->setCoords(1, m_config.alarmValue);
    limit->setPen(QPen(m_config.alarmColor, 1, Qt::DashLine));

    m_plot->setBackground(QColor("#202B37"));
    m_plot->axisRect()->setBackground(QColor("#202B37"));
    m_plot->xAxis->setBasePen(QPen(QColor("#334353")));
    m_plot->xAxis->setTickPen(Qt::NoPen);
    m_plot->xAxis->setSubTickPen(Qt::NoPen);
    m_plot->xAxis->setTickLabels(false);
    m_plot->yAxis->setBasePen(QPen(m_config.color));
    m_plot->yAxis->setTickPen(QPen(m_config.color));
    m_plot->yAxis->setSubTickPen(QPen(QColor("#2A3745")));
    m_plot->yAxis->setTickLabelColor(m_config.color);
    m_plot->yAxis->setLabelColor(m_config.color);
    m_plot->xAxis->grid()->setPen(QPen(QColor("#2A3745"), 1, Qt::DotLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor("#2A3745"), 1, Qt::DotLine));
    m_plot->yAxis->setLabel(m_config.unit);

    m_cursorLine = new QCPItemStraightLine(m_plot);
    m_cursorLine->point1->setCoords(0, m_config.yMin);
    m_cursorLine->point2->setCoords(0, m_config.yMax);
    m_cursorLine->setPen(QPen(m_config.color, 1, Qt::DotLine));
    m_cursorLine->setVisible(false);

    m_timeLabel = new QCPItemText(m_plot);
    m_timeLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_timeLabel->position->setType(QCPItemPosition::ptPlotCoords);
    m_timeLabel->position->setCoords(0, m_config.yMax);
    m_timeLabel->setColor(QColor("#E4EAF0"));
    m_timeLabel->setFont(QFont("Microsoft YaHei", 10));
    m_timeLabel->setPadding(QMargins(6, 3, 6, 3));
    m_timeLabel->setBrush(QColor(32, 43, 55, 220));
    m_timeLabel->setPen(QPen(QColor("#334353")));
    m_timeLabel->setVisible(false);

    m_plot->setMouseTracking(true);
    m_plot->installEventFilter(this);
    connect(m_plot, &QCustomPlot::mouseMove, this, [this](QMouseEvent *event) {
        if (m_cursorFrozen) {
            return;
        }
        const double x = m_plot->xAxis->pixelToCoord(event->pos().x());
        if (!isVisibleTime(x)) {
            hideCursor();
            return;
        }
        showCursorAt(x, false);
    });
    connect(m_plot, &QCustomPlot::mousePress, this, [this](QMouseEvent *event) {
        const double x = m_plot->xAxis->pixelToCoord(event->pos().x());
        if (event->button() == Qt::LeftButton) {
            if (!isVisibleTime(x)) {
                return;
            }
            m_cursorFrozen = true;
            showCursorAt(x, true);
        } else if (event->button() == Qt::RightButton) {
            m_cursorFrozen = false;
            if (!isVisibleTime(x)) {
                hideCursor();
                return;
            }
            showCursorAt(x, false);
        }
    });
}

void TelemetryPlotWidget::showCursorAt(double x, bool frozen)
{
    m_cursorLine->point1->setCoords(x, m_config.yMin);
    m_cursorLine->point2->setCoords(x, m_config.yMax);
    m_cursorLine->setPen(QPen(m_config.color, frozen ? 2 : 1, frozen ? Qt::SolidLine : Qt::DotLine));
    m_cursorLine->setVisible(true);
    m_timeLabel->position->setCoords(x, m_config.yMax);
    m_timeLabel->setText(timeForX(x).toString("HH:mm:ss"));
    m_timeLabel->setVisible(true);
    m_plot->replot();
}

void TelemetryPlotWidget::hideCursor()
{
    m_cursorLine->setVisible(false);
    m_timeLabel->setVisible(false);
    m_plot->replot();
}

bool TelemetryPlotWidget::isVisibleTime(double x) const
{
    return x >= m_currentTime - m_visibleSeconds && x <= m_currentTime;
}

QDateTime TelemetryPlotWidget::timeForX(double x) const
{
    return QDateTime::currentDateTime().addSecs(static_cast<qint64>(x - m_currentTime));
}
