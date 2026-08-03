#include "RunningStatusPanel.h"

#include "PlcFeedbackVM.h"
#include "TelemetryPlotWidget.h"
#include "UiHelpers.h"

#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const QColor kAccent("#00E5FF");
const QColor kAccentSpeed   = kAccent;
const QColor kAccentTorque  = kAccent;
const QColor kAccentAxisX   = kAccent;
const QColor kAccentAxisY   = kAccent;
const QColor kAccentAxisZ   = kAccent;

const QString kValueColorNormal   = "#E6EEF5";
const QString kValueColorNoData   = "#5A6A78";
const QString kValueColorWarning  = "#F1C40F";
const QString kValueColorCritical = "#E74C3C";

QString cardStyle()
{
    return QStringLiteral(R"(
        QFrame#metricCard {
            background: #1B2632;
            border: 1px solid #2E3D4D;
            border-radius: 6px;
        }
    )");
}

QFrame *createAccentBar(const QColor &accent, int height, QWidget *parent)
{
    auto *bar = new QFrame(parent);
    bar->setFixedSize(3, height);
    bar->setStyleSheet(QString("background:%1;border:none;").arg(accent.name()));
    return bar;
}

QString valueColorFor(RunningStatusPanel * /*panel*/, const QString &value)
{
    return value == "--" ? kValueColorNoData : kValueColorNormal;
}

} // namespace

RunningStatusPanel::RunningStatusPanel(QWidget *parent)
    : QFrame(parent)
{
    auto *panel = createPanel("运行状态", nullptr, this);
    panel->setMinimumWidth(420);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    panelLayout->setSpacing(12);

    panelLayout->addWidget(createSpindleGrid(panel));

    auto *waveRow = new QHBoxLayout();
    waveRow->setSpacing(14);

    m_speedPlot = new TelemetryPlotWidget({
        "主轴速度",
        "RPM",
        kAccentSpeed,
        0,
        3500,
        3000,
        QColor("#E74C3C")
    }, panel);

    m_torquePlot = new TelemetryPlotWidget({
        "主轴扭矩",
        "N·m",
        kAccentTorque,
        0,
        30,
        25,
        QColor("#F1C40F")
    }, panel);

    waveRow->addWidget(m_speedPlot);
    waveRow->addWidget(m_torquePlot);
    panelLayout->addLayout(waveRow);

    panelLayout->addWidget(createPositionGrid("当前位置", "position", panel));
    panelLayout->addWidget(createPositionGrid("当前速度", "velocity", panel));
    panelLayout->addWidget(createDeviceStatusSection(panel));

    m_cameraStatusTimer = new QTimer(this);
    m_cameraStatusTimer->setSingleShot(true);
    m_cameraStatusTimer->setInterval(3000);
    connect(m_cameraStatusTimer, &QTimer::timeout, this, [this]() {
        applyCameraStatus(ConnectionState::Unknown, QStringLiteral("状态消息超时"));
    });
    applyCameraStatus(ConnectionState::Unknown, QStringLiteral("等待状态"));

    panelLayout->addStretch();
}

void RunningStatusPanel::appendSample(double time, double speed, double torque)
{
    constexpr double kVisibleSeconds = 30;
    m_speedPlot->appendSample(time, speed, kVisibleSeconds);
    m_torquePlot->appendSample(time, torque, kVisibleSeconds);

    auto stateForRatio = [](double ratio) {
        if (ratio >= 1.0) return MetricState::Critical;
        if (ratio >= 0.8) return MetricState::Warning;
        return MetricState::Normal;
    };

    setMetricValue("speed",
                   QString::number(speed, 'f', 0),
                   stateForRatio(speed / 3000.0));
    setMetricValue("torque",
                   QString::number(torque, 'f', 1),
                   stateForRatio(torque / 25.0));
}

void RunningStatusPanel::setPlcFeedback(const PlcFeedbackVM &feedback)
{
    const auto &p = feedback.pathParams;

    if (!m_plcElapsedTimer.isValid()) {
        m_plcElapsedTimer.start();
    }
    appendSample(m_plcElapsedTimer.elapsed() / 1000.0,
                 p.spindleSpeed,
                 p.spindleTorque);

    setMetricValue("positionX", QString::number(p.xPos, 'f', 2));
    setMetricValue("positionY", QString::number(p.yPos, 'f', 2));
    setMetricValue("positionZ", QString::number(p.zPos, 'f', 2));

    setMetricValue("velocityX", QString::number(p.xSpeed, 'f', 2));
    setMetricValue("velocityY", QString::number(p.ySpeed, 'f', 2));
    setMetricValue("velocityZ", QString::number(p.zSpeed, 'f', 2));
}

void RunningStatusPanel::setCameraStatus(bool connected,
                                         const QString &cameraId,
                                         const QString &message)
{
    QString detail = connected ? cameraId : message;
    if (detail.isEmpty()) {
        detail = connected ? QStringLiteral("已连接") : QStringLiteral("未连接");
    }

    if (m_cameraStatusDetail) {
        m_cameraStatusDetail->setToolTip(message);
    }
    applyCameraStatus(connected ? ConnectionState::Online : ConnectionState::Offline,
                      detail);
    if (m_cameraStatusTimer) {
        m_cameraStatusTimer->start();
    }
}

QWidget *RunningStatusPanel::createSpindleGrid(QWidget *parent)
{
    auto *gridWidget = new QWidget(parent);
    auto *grid = new QGridLayout(gridWidget);
    grid->setContentsMargins(2, 0, 2, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);

    grid->addWidget(createMetricCard("speed",  "主轴速度", "--", "RPM",  kAccentSpeed,  gridWidget), 0, 0);
    grid->addWidget(createMetricCard("torque", "主轴扭矩", "--", "N·m",  kAccentTorque, gridWidget), 0, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    return gridWidget;
}

QWidget *RunningStatusPanel::createPositionGrid(const QString &title,
                                                const QString &keyPrefix,
                                                QWidget *parent)
{
    auto *gridWidget = new QWidget(parent);
    auto *outer = new QVBoxLayout(gridWidget);
    outer->setContentsMargins(2, 0, 2, 0);
    outer->setSpacing(6);

    auto *header = makeLabel(title, "metricName");
    header->setStyleSheet("color:#8A9AA8;font-size:16px;border:none;background:transparent;");
    outer->addWidget(header);

    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    row->addWidget(createAxisCard("X", keyPrefix, kAccentAxisX, gridWidget));
    row->addWidget(createAxisCard("Y", keyPrefix, kAccentAxisY, gridWidget));
    row->addWidget(createAxisCard("Z", keyPrefix, kAccentAxisZ, gridWidget));

    outer->addLayout(row);
    return gridWidget;
}

QWidget *RunningStatusPanel::createDeviceStatusSection(QWidget *parent)
{
    auto *section = new QWidget(parent);
    auto *outer = new QVBoxLayout(section);
    outer->setContentsMargins(2, 0, 2, 0);
    outer->setSpacing(6);

    auto *header = makeLabel(QStringLiteral("设备状态"), "metricName");
    header->setStyleSheet("color:#8A9AA8;font-size:16px;border:none;background:transparent;");
    outer->addWidget(header);

    auto *card = new QFrame(section);
    card->setObjectName("metricCard");
    card->setFixedHeight(48);
    card->setStyleSheet(cardStyle());

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(10);

    m_cameraStatusDot = makeLabel(QStringLiteral("●"), "cameraStatusDot");
    m_cameraStatusDot->setFixedWidth(16);
    layout->addWidget(m_cameraStatusDot);

    auto *name = makeLabel(QStringLiteral("相机"), "deviceName");
    name->setStyleSheet("color:#E6EEF5;font-size:16px;font-weight:600;border:none;background:transparent;");
    layout->addWidget(name);

    m_cameraStatusDetail = makeLabel(QStringLiteral("等待状态"), "cameraStatusDetail");
    m_cameraStatusDetail->setFixedWidth(150);
    m_cameraStatusDetail->setStyleSheet("color:#8A9AA8;font-size:13px;border:none;background:transparent;");
    layout->addWidget(m_cameraStatusDetail);
    layout->addStretch();

    m_cameraStatusText = makeLabel(QStringLiteral("状态未知"), "cameraStatusText");
    m_cameraStatusText->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_cameraStatusText);

    outer->addWidget(card);
    return section;
}

QWidget *RunningStatusPanel::createMetricCard(const QString &key,
                                              const QString &title,
                                              const QString &value,
                                              const QString &unit,
                                              const QColor &accent,
                                              QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("metricCard");
    card->setFixedHeight(48);
    card->setStyleSheet(cardStyle());

    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(8, 0, 12, 0);
    cardLayout->setSpacing(10);

    auto *bar = createAccentBar(accent, 24, card);
    cardLayout->addWidget(bar, 0, Qt::AlignVCenter);

    auto *titleLabel = makeLabel(title, "metricName");
    titleLabel->setStyleSheet("color:#8A9AA8;font-size:16px;border:none;background:transparent;");
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    cardLayout->addWidget(titleLabel);

    cardLayout->addStretch();

    auto *valueLabel = makeLabel(value, "metricValue");
    valueLabel->setStyleSheet(QString("color:%1;font-size:24px;font-weight:600;border:none;background:transparent;")
                                  .arg(valueColorFor(this, value)));
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardLayout->addWidget(valueLabel);

    if (!unit.isEmpty()) {
        auto *unitLabel = makeLabel(unit);
        unitLabel->setStyleSheet("color:#8A9AA8;font-size:13px;border:none;background:transparent;");
        unitLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        cardLayout->addWidget(unitLabel);
    }

    m_metricLabels.insert(key, valueLabel);
    return card;
}

QWidget *RunningStatusPanel::createAxisCard(const QString &axis,
                                            const QString &keyPrefix,
                                            const QColor &accent,
                                            QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("metricCard");
    card->setFixedHeight(58);
    card->setStyleSheet(cardStyle());

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(8, 0, 10, 0);
    layout->setSpacing(8);

    auto *bar = createAccentBar(accent, 28, card);
    layout->addWidget(bar, 0, Qt::AlignVCenter);

    auto *axisLabel = makeLabel(axis);
    axisLabel->setStyleSheet(QString("color:%1;font-size:20px;font-weight:700;border:none;background:transparent;")
                                 .arg(accent.name()));
    axisLabel->setFixedWidth(18);
    axisLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *valueLabel = makeLabel("--", "metricValue");
    valueLabel->setStyleSheet(QString("color:%1;font-size:20px;font-weight:600;border:none;background:transparent;")
                                  .arg(kValueColorNoData));
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(axisLabel);
    layout->addStretch();
    layout->addWidget(valueLabel);

    m_metricLabels.insert(QString("%1%2").arg(keyPrefix, axis), valueLabel);
    return card;
}

void RunningStatusPanel::setMetricValue(const QString &key,
                                        const QString &value,
                                        MetricState state)
{
    auto *label = m_metricLabels.value(key, nullptr);
    if (!label) {
        return;
    }

    QString color = kValueColorNormal;
    switch (state) {
    case MetricState::NoData:   color = kValueColorNoData;   break;
    case MetricState::Normal:   color = kValueColorNormal;   break;
    case MetricState::Warning:  color = kValueColorWarning;  break;
    case MetricState::Critical: color = kValueColorCritical; break;
    }

    if (value == "--") {
        color = kValueColorNoData;
    }

    label->setText(value);
    label->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:600;border:none;background:transparent;")
                             .arg(color)
                             .arg((key.startsWith("position") || key.startsWith("velocity")) ? 20 : 24));
}

void RunningStatusPanel::applyCameraStatus(ConnectionState state, const QString &detail)
{
    if (!m_cameraStatusDot || !m_cameraStatusText || !m_cameraStatusDetail) {
        return;
    }

    QString color;
    QString statusText;
    switch (state) {
    case ConnectionState::Online:
        color = QStringLiteral("#2ECC71");
        statusText = QStringLiteral("在线");
        break;
    case ConnectionState::Offline:
        color = QStringLiteral("#E74C3C");
        statusText = QStringLiteral("离线");
        break;
    case ConnectionState::Unknown:
        color = kValueColorNoData;
        statusText = QStringLiteral("状态未知");
        break;
    }

    const QString shownDetail = m_cameraStatusDetail->fontMetrics().elidedText(
        detail, Qt::ElideRight, m_cameraStatusDetail->width());
    m_cameraStatusDetail->setText(shownDetail);
    m_cameraStatusDetail->setToolTip(detail);
    m_cameraStatusDot->setStyleSheet(
        QString("color:%1;font-size:16px;border:none;background:transparent;").arg(color));
    m_cameraStatusText->setText(statusText);
    m_cameraStatusText->setStyleSheet(
        QString("color:%1;font-size:14px;font-weight:600;border:none;background:transparent;")
            .arg(color));
}
