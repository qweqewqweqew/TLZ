#include "ui/panels/MachineStatusPanel.h"

#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "bridge/PlcFeedbackVM.h"
#include "bridge/PlcStatusPresentation.h"
#include "qcustomplot.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QProgressBar>
#include <QVBoxLayout>

#include <array>
#include <cmath>

namespace {

struct MetricDefinition
{
    const char *key;
    const char *title;
    const char *unit;
    double min;
    double max;
    int decimals;
};

constexpr std::array<MetricDefinition, 6> kMetricDefinitions = {{
    {"spindle", "主轴转速", "RPM", 0.0, 3500.0, 0},
    {"torque", "主轴扭矩", "N·m", 0.0, 30.0, 1},
    {"feed", "进给速度", "mm/min", 0.0, 100.0, 1},
    {"x", "X速度", "mm/min", 0.0, 100.0, 1},
    {"y", "Y速度", "mm/min", 0.0, 100.0, 1},
    {"z", "Z速度", "mm/min", 0.0, 100.0, 1},
}};

// 单个 QCustomPlot 承载六个指标。每个指标使用归一化量程，刻度文字和卡片
// 通过 QCPItem 绘制，从而保留一张图的刷新效率，同时呈现独立仪表条的层次。
class QcpGaugeBoard : public QWidget
{
public:
    explicit QcpGaugeBoard(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(210);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_plot = new QCustomPlot(this);
        m_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_plot->setBackground(QColor(Theme::kPanelBg));
        m_plot->axisRect()->setBackground(QColor(Theme::kPanelBg));
        m_plot->axisRect()->setMinimumMargins(QMargins(2, 2, 2, 2));
        m_plot->xAxis->setRange(-0.5, double(kMetricDefinitions.size()) - 0.5);
        m_plot->xAxis->setVisible(false);
        m_plot->xAxis->grid()->setVisible(false);
        m_plot->yAxis->setRange(-0.22, 1.18);
        m_plot->yAxis->setTicks(false);
        m_plot->yAxis->setTickLabels(false);
        m_plot->yAxis->setBasePen(Qt::NoPen);
        m_plot->yAxis->setTickPen(Qt::NoPen);
        m_plot->yAxis->setSubTickPen(Qt::NoPen);
        m_plot->yAxis->grid()->setVisible(false);
        m_plot->setInteractions(QCP::iNone);

        constexpr double kBarXOffset = 0.12;
        constexpr double kTileHalfWidth = 0.47;
        constexpr double kScaleXOffset = -0.24;
        constexpr double kBarTickLeft = 0.00;
        constexpr double kBarTickRight = 0.065;
        constexpr int kMajorTickCount = 5;
        constexpr int kMinorTickCount = 9;
        constexpr std::array<double, kMajorTickCount> kMajorFractions = {
            0.0, 0.25, 0.5, 0.75, 1.0};

        for (int i = 0; i < int(kMetricDefinitions.size()); ++i) {
            const MetricDefinition &definition = kMetricDefinitions[std::size_t(i)];
            const double center = double(i);
            const double barX = center + kBarXOffset;

            m_tiles[std::size_t(i)] = new QCPItemRect(m_plot);
            m_tiles[std::size_t(i)]->topLeft->setCoords(center - kTileHalfWidth, 1.16);
            m_tiles[std::size_t(i)]->bottomRight->setCoords(center + kTileHalfWidth, -0.18);
            m_tiles[std::size_t(i)]->setLayer(QStringLiteral("background"));

            m_titles[std::size_t(i)] = makeText(
                QString::fromUtf8(definition.title), center, 1.095,
                Qt::AlignHCenter | Qt::AlignVCenter, 10, true);
            m_units[std::size_t(i)] = makeText(
                QString::fromUtf8(definition.unit), center, 1.005,
                Qt::AlignHCenter | Qt::AlignVCenter, 8, false);
            m_valuesText[std::size_t(i)] = makeText(
                QStringLiteral("--"), center + 0.05, -0.115,
                Qt::AlignHCenter | Qt::AlignVCenter, 16, true);

            for (int tick = 0; tick < kMinorTickCount; ++tick) {
                const double fraction = double(tick) / double(kMinorTickCount - 1);
                auto *line = new QCPItemLine(m_plot);
                line->start->setCoords(barX + kBarTickLeft, fraction);
                line->end->setCoords(barX + kBarTickRight, fraction);
                line->setLayer(QStringLiteral("overlay"));
                m_tickLines[std::size_t(i)][std::size_t(tick)] = line;
            }

            for (int tick = 0; tick < kMajorTickCount; ++tick) {
                const double fraction = kMajorFractions[std::size_t(tick)];
                const double tickValue = definition.min
                    + (definition.max - definition.min) * fraction;
                auto *label = makeText(
                    QString::number(tickValue, 'f', definition.decimals),
                    center + kScaleXOffset,
                    fraction,
                    Qt::AlignRight | Qt::AlignVCenter,
                    8,
                    false);
                m_tickLabels[std::size_t(i)][std::size_t(tick)] = label;
            }
        }

        m_trackBars = new QCPBars(m_plot->xAxis, m_plot->yAxis);
        m_trackBars->setWidth(0.25);
        m_trackBars->setPen(QPen(QColor(Theme::kBorder), 1));
        for (int i = 0; i < int(kMetricDefinitions.size()); ++i) {
            m_trackBars->addData(double(i) + 0.12, 1.0);
        }

        for (int i = 0; i < int(kMetricDefinitions.size()); ++i) {
            auto *bar = new QCPBars(m_plot->xAxis, m_plot->yAxis);
            bar->setWidth(0.25);
            bar->setPen(Qt::NoPen);
            bar->setData(QVector<double>{double(i) + kBarXOffset}, QVector<double>{0.0});
            m_valueBars.append(bar);
        }

        root->addWidget(m_plot, 1);

        Theme::watch(this, [this]() { applyTheme(); });
        applyTheme();
    }

    void setValue(const QString &key, double value, bool hasData)
    {
        const int index = indexForKey(key);
        if (index < 0 || index >= m_valueBars.size()) {
            return;
        }

        const MetricDefinition &definition = kMetricDefinitions[std::size_t(index)];
        const double clamped = hasData ? qBound(definition.min, value, definition.max)
                                       : definition.min;
        const double denominator = definition.max - definition.min;
        const double ratio = denominator > 0.0
            ? qBound(0.0, (clamped - definition.min) / denominator, 1.0)
            : 0.0;

        m_valid[std::size_t(index)] = hasData;
        m_values[std::size_t(index)] = value;
        m_valueBars[index]->setData(QVector<double>{double(index) + 0.12}, QVector<double>{ratio});
        m_valueBars[index]->setBrush(hasData ? gradientBrush()
                                              : QBrush(Qt::NoBrush));
        m_valuesText[std::size_t(index)]->setText(hasData
            ? QString::number(value, 'f', definition.decimals)
            : QStringLiteral("--"));
        m_valuesText[std::size_t(index)]->setColor(
            hasData ? QColor(Theme::kStateWarning) : QColor(Theme::kTextMuted));
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }

    void clear()
    {
        for (int i = 0; i < int(kMetricDefinitions.size()); ++i) {
            setValue(QString::fromLatin1(kMetricDefinitions[std::size_t(i)].key), 0.0, false);
        }
    }

    void applyTheme()
    {
        if (!m_plot) {
            return;
        }
        m_plot->setBackground(QColor(Theme::kPanelBg));
        m_plot->axisRect()->setBackground(QColor(Theme::kPanelBg));
        m_trackBars->setBrush(QColor(Theme::kCanvasBg));
        m_trackBars->setPen(QPen(QColor(Theme::kBorder), 1));
        for (int i = 0; i < int(kMetricDefinitions.size()); ++i) {
            m_tiles[std::size_t(i)]->setBrush(QColor(Theme::kControlBg));
            m_tiles[std::size_t(i)]->setPen(QPen(QColor(Theme::kBorder), 1));
            m_titles[std::size_t(i)]->setColor(QColor(Theme::kTextPrimary));
            m_units[std::size_t(i)]->setColor(QColor(Theme::kTextMuted));
            m_valuesText[std::size_t(i)]->setColor(
                m_valid[std::size_t(i)]
                    ? QColor(Theme::kStateWarning)
                    : QColor(Theme::kTextMuted));
            for (int tick = 0; tick < 9; ++tick) {
                m_tickLines[std::size_t(i)][std::size_t(tick)]->setPen(
                    QPen(QColor(Theme::kBorder), tick % 2 == 0 ? 1 : 0.75));
            }
            for (int tick = 0; tick < 5; ++tick) {
                m_tickLabels[std::size_t(i)][std::size_t(tick)]->setColor(
                    QColor(Theme::kTextMuted));
            }
            m_valueBars[i]->setBrush(m_valid[std::size_t(i)]
                ? gradientBrush()
                : QBrush(Qt::NoBrush));
        }
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }

private:
    static int indexForKey(const QString &key)
    {
        for (int i = 0; i < int(kMetricDefinitions.size()); ++i) {
            if (key == QLatin1String(kMetricDefinitions[std::size_t(i)].key)) {
                return i;
            }
        }
        return -1;
    }

    QCPItemText *makeText(const QString &text,
                          double x,
                          double y,
                          Qt::Alignment alignment,
                          int pixelSize,
                          bool bold)
    {
        auto *item = new QCPItemText(m_plot);
        item->position->setCoords(x, y);
        item->setText(text);
        item->setPositionAlignment(alignment);
        item->setTextAlignment(Qt::AlignCenter);
        item->setColor(QColor(Theme::kTextPrimary));
        item->setPen(Qt::NoPen);
        item->setBrush(Qt::NoBrush);
        QFont font(QStringLiteral("Microsoft YaHei"));
        font.setPixelSize(pixelSize);
        font.setBold(bold);
        item->setFont(font);
        item->setLayer(QStringLiteral("overlay"));
        return item;
    }

    static QBrush gradientBrush()
    {
        QLinearGradient gradient(0.0, 1.0, 0.0, 0.0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0, QColor("#238DBB"));
        gradient.setColorAt(0.52, QColor("#65B7A5"));
        gradient.setColorAt(1.0, QColor("#E3D77F"));
        return QBrush(gradient);
    }

    QCustomPlot *m_plot{nullptr};
    QCPBars *m_trackBars{nullptr};
    QVector<QCPBars *> m_valueBars;
    std::array<QCPItemRect *, kMetricDefinitions.size()> m_tiles{};
    std::array<QCPItemText *, kMetricDefinitions.size()> m_titles{};
    std::array<QCPItemText *, kMetricDefinitions.size()> m_units{};
    std::array<QCPItemText *, kMetricDefinitions.size()> m_valuesText{};
    std::array<std::array<QCPItemText *, 5>, kMetricDefinitions.size()> m_tickLabels{};
    std::array<std::array<QCPItemLine *, 9>, kMetricDefinitions.size()> m_tickLines{};
    std::array<double, kMetricDefinitions.size()> m_values{};
    std::array<bool, kMetricDefinitions.size()> m_valid{};
};

} // namespace

MachineStatusPanel::MachineStatusPanel(QWidget *parent)
    : QFrame(parent)
{
    m_plcPill = new QLabel(this);
    m_plcPill->setFixedHeight(24);
    m_plcPill->setAlignment(Qt::AlignCenter);

    auto *panel = createPanel(QStringLiteral("修磨实时状态"), m_plcPill, this);
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);
    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    panelLayout->setSpacing(10);

    auto *taskRow = new QHBoxLayout();
    taskRow->setContentsMargins(0, 0, 0, 0);
    taskRow->setSpacing(10);
    m_taskLine = new QLabel(panel);
    m_taskLine->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:600;background:transparent;border:none;")
                                  .arg(Theme::kTextPrimary));
    taskRow->addWidget(m_taskLine, 0);
    m_progress = new QProgressBar(panel);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(8);
    Theme::bindStyle(m_progress, []() {
        return QStringLiteral(
                   "QProgressBar { background:%1; border:none; border-radius:4px; }"
                   "QProgressBar::chunk { background:%2; border-radius:4px; }")
            .arg(Theme::kBorder, Theme::markColor());
    });
    m_percent = new QLabel(QStringLiteral("0%"), panel);
    m_percent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    Theme::bindStyle(m_percent, []() {
        return QStringLiteral("color:%1;font-size:13px;font-weight:700;background:transparent;border:none;")
            .arg(Theme::kTextPrimary);
    });
    taskRow->addWidget(m_progress, 1);
    taskRow->addWidget(m_percent, 0);
    panelLayout->addLayout(taskRow);

    // ---- 实时数据：QCustomPlot 竖向仪表条 ----
    m_metricColumn = new QcpGaugeBoard(this);

    // ---- 控制按钮（等待后端控制接口）----
    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(8);
    m_startButton = new ElaPushButton(QStringLiteral("启动修磨"), panel);
    applyPrimaryButtonStyle(m_startButton);
    m_pauseButton = new ElaPushButton(QStringLiteral("暂停修磨"), panel);
    applySecondaryButtonStyle(m_pauseButton);
    m_stopButton = new ElaPushButton(QStringLiteral("停止"), panel);
    applySecondaryButtonStyle(m_stopButton);
    m_stopButton->setLightTextColor(QColor(Theme::kStateError));
    m_stopButton->setDarkTextColor(QColor(Theme::kStateError));
    for (ElaPushButton *button : {m_startButton, m_pauseButton, m_stopButton}) {
        button->setFixedHeight(32);
        button->setEnabled(false);
        button->setToolTip(QStringLiteral("后端尚未提供控制指令接口"));
        buttons->addWidget(button, 1);
    }
    panelLayout->addLayout(buttons);

    clear();
    applyTheme();
    Theme::watch(this, [this]() { applyTheme(); });
}

QWidget *MachineStatusPanel::metricColumn() const
{
    return m_metricColumn;
}

void MachineStatusPanel::setMetric(const QString &key, double value, int decimals, bool hasData)
{
    Q_UNUSED(decimals)
    auto *gaugeBoard = static_cast<QcpGaugeBoard *>(m_metricColumn);
    if (!gaugeBoard) {
        return;
    }
    gaugeBoard->setValue(key, value, hasData);
}

void MachineStatusPanel::setPlcFeedback(const PlcFeedbackVM &feedback)
{
    const PlcStatusPresentation plc = plcStatusPresentation(feedback.statusWord, feedback.faultCode);
    QString text = plc.text;
    if (feedback.faultCode != 0) {
        text += QStringLiteral(" %1").arg(feedback.faultCode);
    }
    m_plcPill->setText(QStringLiteral("PLC \xc2\xb7 %1").arg(text));
    m_plcLinked = true;
    m_plcColor = plc.color;
    applyPlcPill();
    m_plcPill->setToolTip(QStringLiteral("状态字 0x%1 \xc2\xb7 故障码 %2")
                              .arg(feedback.statusWord, 4, 16, QLatin1Char('0'))
                              .arg(feedback.faultCode));

    const PlcPathParamsVM &p = feedback.pathParams;
    setMetric(QStringLiteral("spindle"), double(p.spindleSpeed), 0);
    setMetric(QStringLiteral("torque"), double(p.spindleTorque), 1);
    setMetric(QStringLiteral("feed"), std::hypot(double(p.xSpeed), double(p.ySpeed)), 1);
    setMetric(QStringLiteral("x"), std::abs(double(p.xSpeed)), 1);
    setMetric(QStringLiteral("y"), std::abs(double(p.ySpeed)), 1);
    setMetric(QStringLiteral("z"), std::abs(double(p.zSpeed)), 1);

    m_completedPath = feedback.completedPath;
    m_currentPath = feedback.currentPath;
    refreshTaskLine();
}

void MachineStatusPanel::setTask(quint64 taskId, int pathTotal)
{
    m_taskId = taskId;
    m_pathTotal = pathTotal;
    m_completedPath = 0;
    m_currentPath = 0;
    m_finished = false;
    m_success = false;
    setTaskProgress(0.0f);
    refreshTaskLine();
}

void MachineStatusPanel::setTaskProgress(float progressPercent)
{
    const int percent = qRound(qBound(0.0f, progressPercent, 100.0f));
    if (m_progress) {
        m_progress->setValue(percent);
    }
    if (m_percent) {
        m_percent->setText(QStringLiteral("%1%").arg(percent));
    }
}

void MachineStatusPanel::setTaskFinished(bool success)
{
    m_finished = true;
    m_success = success;
    if (success) {
        setTaskProgress(100.0f);
    }
    refreshTaskLine();
}

void MachineStatusPanel::clear()
{
    m_taskId = 0;
    m_pathTotal = 0;
    m_completedPath = 0;
    m_currentPath = 0;
    m_finished = false;
    m_success = false;
    setTaskProgress(0.0f);
    m_plcPill->setText(QStringLiteral("PLC \xc2\xb7 \xe6\x9c\xaa\xe8\xbf\x9e\xe6\x8e\xa5"));
    m_plcLinked = false;
    m_plcColor.clear();
    applyPlcPill();
    m_plcPill->setToolTip(QString());
    for (const char *key : {"spindle", "torque", "feed", "x", "y", "z"}) {
        setMetric(QString::fromLatin1(key), 0.0, 0, false);
    }
    refreshTaskLine();
}

void MachineStatusPanel::refreshTaskLine()
{
    if (m_taskId == 0) {
        m_taskLine->setText(QStringLiteral("\xe5\xbe\x85\xe5\x91\xbd \xc2\xb7 \xe7\xad\x89\xe5\xbe\x85\xe5\x90\x8e\xe7\xab\xaf\xe4\xb8\x8b\xe5\x8f\x91\xe4\xbb\xbb\xe5\x8a\xa1"));
        m_taskLine->setStyleSheet(QStringLiteral("color:%1;font-size:13px;background:transparent;border:none;")
                                      .arg(Theme::kTextMuted));
        return;
    }
    QString text = QStringLiteral("\xe4\xbb\xbb\xe5\x8a\xa1 #%1").arg(m_taskId);
    if (m_finished) {
        text += m_success ? QStringLiteral(" \xc2\xb7 \xe5\xb7\xb2\xe5\xae\x8c\xe6\x88\x90")
                          : QStringLiteral(" \xc2\xb7 \xe5\xa4\xb1\xe8\xb4\xa5");
    } else if (m_currentPath > 0) {
        text += QStringLiteral(" \xc2\xb7 \xe8\xb7\xaf\xe5\xbe\x84 %1/%2").arg(m_currentPath).arg(m_pathTotal);
    } else if (m_pathTotal > 0) {
        text += QStringLiteral(" \xc2\xb7 \xe5\x85\xb1 %1 \xe6\x9d\xa1\xe8\xb7\xaf\xe5\xbe\x84").arg(m_pathTotal);
    }
    const QString color = m_finished ? (m_success ? Theme::kStateSuccess : Theme::kStateError)
                                     : Theme::kTextPrimary;
    m_taskLine->setText(text);
    m_taskLine->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:600;background:transparent;border:none;")
                                  .arg(color));
}

void MachineStatusPanel::applyPlcPill()
{
    if (!m_plcPill) {
        return;
    }
    if (!m_plcLinked) {
        m_plcPill->setStyleSheet(QStringLiteral("color:%1;background:%2;border:1px solid %3;border-radius:4px;"
                                                "padding:0 10px;font-size:12px;")
                                     .arg(Theme::kTextMuted, Theme::kControlBg, Theme::kBorder));
        return;
    }
    m_plcPill->setStyleSheet(QStringLiteral("color:%1;background:%2;border:1px solid %1;border-radius:4px;"
                                            "padding:0 10px;font-size:12px;font-weight:600;")
                                 .arg(m_plcColor, Theme::kPanelBg));
}

void MachineStatusPanel::applyTheme()
{
    if (auto *gaugeBoard = static_cast<QcpGaugeBoard *>(m_metricColumn)) {
        gaugeBoard->applyTheme();
    }
    applyPlcPill();
    refreshTaskLine();
    if (m_startButton) {
        applyPrimaryButtonStyle(m_startButton);
    }
    if (m_pauseButton) {
        applySecondaryButtonStyle(m_pauseButton);
    }
    if (m_stopButton) {
        applySecondaryButtonStyle(m_stopButton);
        m_stopButton->setLightTextColor(QColor(Theme::kStateError));
        m_stopButton->setDarkTextColor(QColor(Theme::kStateError));
    }
}
