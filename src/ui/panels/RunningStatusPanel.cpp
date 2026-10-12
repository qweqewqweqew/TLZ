#include "ui/panels/RunningStatusPanel.h"

#include "ElaProgressRing.h"
#include "bridge/PlcFeedbackVM.h"
#include "bridge/PlcStatusPresentation.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "ui/widgets/VerticalGauge.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QSizePolicy>
#include <QStringList>
#include <QtGlobal>
#include <QVBoxLayout>

#include <cmath>

namespace {

const QColor kRealtimeValue(Theme::kRealtime);
const QColor kDirectionPositive(Theme::kStateSuccess);
const QColor kDirectionNegative(Theme::kStateWarning);
const QColor kDirectionStopped(Theme::kStateUnknown);
const QString kValueColorNoData = Theme::kTextMuted;

constexpr double kSpindleSpeedMaximum = 3500.0;
constexpr double kSpindleTorqueMaximum = 35.0;
constexpr double kAxisSpeedMaximum = 300.0;

QString positionPanelStyle()
{
    return QStringLiteral(R"(
        QFrame#positionPanel {
            background: transparent;
            border: none;
        }
    )");
}

QString positionRowStyle()
{
    return QStringLiteral(R"(
        QFrame#positionRow {
            background: %1;
            border: 1px solid %2;
            border-radius: 4px;
        }
    )").arg(Theme::kControlBg, Theme::kDivider);
}

QString taskProgressPanelStyle()
{
    return QStringLiteral(R"(
        QFrame#taskProgressPanel {
            background: %1;
            border: 1px solid %2;
            border-radius: 4px;
        }
    )").arg(Theme::kControlBg, Theme::kDivider);
}

void configureGauge(VerticalGauge *gauge,
                    const QString &title,
                    const QString &unit,
                    double minimum,
                    double maximum)
{
    gauge->setTitle(title);
    gauge->setUnit(unit);
    gauge->setRange(minimum, maximum);
}

} // namespace

RunningStatusPanel::RunningStatusPanel(QWidget *parent)
    : QFrame(parent)
{
    auto *panel = createPanel(QStringLiteral("运行状态"), nullptr, this);
    panel->setMinimumWidth(480);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    panelLayout->setSpacing(7);
    panelLayout->addWidget(createOverviewRow(panel));
    panelLayout->addWidget(createGaugeRow(panel));
    panelLayout->addWidget(createPositionGrid(QStringLiteral("当前位置"),
                                               QStringLiteral("position"),
                                               panel));
    panelLayout->addStretch();
}

void RunningStatusPanel::appendSample(double time, double speed, double torque)
{
    Q_UNUSED(time)
    m_spindleSpeedGauge->setValue(speed);
    m_spindleTorqueGauge->setValue(torque);
}

void RunningStatusPanel::setPlcFeedback(const PlcFeedbackVM &feedback)
{
    const auto &p = feedback.pathParams;

    appendSample(0.0, p.spindleSpeed, p.spindleTorque);
    applyAxisSpeed(m_xSpeedGauge, m_xMotion, p.xSpeed);
    applyAxisSpeed(m_ySpeedGauge, m_yMotion, p.ySpeed);
    applyAxisSpeed(m_zSpeedGauge, m_zMotion, p.zSpeed);

    setPositionValue(QStringLiteral("positionX"), QString::number(p.xPos, 'f', 2));
    setPositionValue(QStringLiteral("positionY"), QString::number(p.yPos, 'f', 2));
    setPositionValue(QStringLiteral("positionZ"), QString::number(p.zPos, 'f', 2));
    updatePlcStatus(feedback.statusWord, feedback.faultCode);
}

void RunningStatusPanel::setTaskProgress(float progress)
{
    if (!m_taskProgressRing) {
        return;
    }

    const int displayProgress = qRound(qBound(0.0f, progress, 100.0f));
    m_taskProgressRing->setProgressColor(
        QColor(displayProgress >= 100 ? Theme::kStateSuccess : Theme::kAccent));
    m_taskProgressRing->setValue(displayProgress);
}

void RunningStatusPanel::clearTaskProgress()
{
    if (m_taskProgressRing) {
        m_taskProgressRing->setProgressColor(QColor(Theme::kAccent));
        m_taskProgressRing->setValue(0);
    }
}

void RunningStatusPanel::clearPlcFeedback()
{
    const QList<VerticalGauge *> gauges = {
        m_spindleSpeedGauge,
        m_spindleTorqueGauge,
        m_xSpeedGauge,
        m_ySpeedGauge,
        m_zSpeedGauge,
    };
    for (VerticalGauge *gauge : gauges) {
        gauge->clearValue();
    }
    m_xSpeedGauge->setStatusText(QStringLiteral("--"), kDirectionStopped);
    m_ySpeedGauge->setStatusText(QStringLiteral("--"), kDirectionStopped);
    m_zSpeedGauge->setStatusText(QStringLiteral("--"), kDirectionStopped);
    m_xMotion = AxisMotion::Unknown;
    m_yMotion = AxisMotion::Unknown;
    m_zMotion = AxisMotion::Unknown;

    static const QStringList kPositionKeys = {
        QStringLiteral("positionX"),
        QStringLiteral("positionY"),
        QStringLiteral("positionZ"),
    };
    for (const QString &key : kPositionKeys) {
        setPositionValue(key, QStringLiteral("--"));
    }
    clearPlcStatus();
}

RunningStatusPanel::AxisMotion RunningStatusPanel::nextAxisMotion(AxisMotion last,
                                                                  double speed) const
{
    // PLC 反馈在零附近会抖，严格用正负号会让正向/负向/停止来回跳。
    // 进入运动要越过较大门槛，回停止要用较小门槛（滞回）。
    constexpr double kEnter = 0.5;
    constexpr double kLeave = 0.15;
    const double mag = std::abs(speed);

    switch (last) {
    case AxisMotion::Positive:
        if (speed <= -kEnter) {
            return AxisMotion::Negative;
        }
        if (mag < kLeave) {
            return AxisMotion::Stopped;
        }
        return AxisMotion::Positive;
    case AxisMotion::Negative:
        if (speed >= kEnter) {
            return AxisMotion::Positive;
        }
        if (mag < kLeave) {
            return AxisMotion::Stopped;
        }
        return AxisMotion::Negative;
    case AxisMotion::Stopped:
    case AxisMotion::Unknown:
        break;
    }

    if (speed >= kEnter) {
        return AxisMotion::Positive;
    }
    if (speed <= -kEnter) {
        return AxisMotion::Negative;
    }
    return AxisMotion::Stopped;
}

void RunningStatusPanel::applyAxisSpeed(VerticalGauge *gauge,
                                        AxisMotion &last,
                                        double speed)
{
    if (!gauge) {
        return;
    }
    if (!std::isfinite(speed)) {
        last = AxisMotion::Stopped;
        gauge->clearValue();
        gauge->setStatusText(QStringLiteral("停止"), kDirectionStopped);
        return;
    }
    last = nextAxisMotion(last, speed);
    gauge->setValue(std::abs(speed));
    switch (last) {
    case AxisMotion::Positive:
        gauge->setStatusText(QStringLiteral("正向"), kDirectionPositive);
        break;
    case AxisMotion::Negative:
        gauge->setStatusText(QStringLiteral("负向"), kDirectionNegative);
        break;
    case AxisMotion::Stopped:
    case AxisMotion::Unknown:
        gauge->setStatusText(QStringLiteral("停止"), kDirectionStopped);
        break;
    }
}

QWidget *RunningStatusPanel::createOverviewRow(QWidget *parent)
{
    auto *overview = new QWidget(parent);
    overview->setFixedHeight(185);

    auto *layout = new QHBoxLayout(overview);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(createPlcStatusBlock(overview), 50);
    layout->addWidget(createTaskProgressBlock(overview), 50);
    return overview;
}

QWidget *RunningStatusPanel::createGaugeRow(QWidget *parent)
{
    auto *rowWidget = new QWidget(parent);
    rowWidget->setFixedHeight(267);

    auto *row = new QHBoxLayout(rowWidget);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    m_spindleSpeedGauge = new VerticalGauge(rowWidget);
    configureGauge(m_spindleSpeedGauge,
                   QStringLiteral("主轴转速"),
                   QStringLiteral("RPM"),
                   0.0,
                   kSpindleSpeedMaximum);
    m_spindleSpeedGauge->setValueDecimals(0);

    m_spindleTorqueGauge = new VerticalGauge(rowWidget);
    configureGauge(m_spindleTorqueGauge,
                   QStringLiteral("主轴扭矩"),
                   QStringLiteral("N·m"),
                   0.0,
                   kSpindleTorqueMaximum);
    m_spindleTorqueGauge->setValueDecimals(1);

    m_xSpeedGauge = new VerticalGauge(rowWidget);
    configureGauge(m_xSpeedGauge,
                   QStringLiteral("X速度"),
                   QStringLiteral("mm/min"),
                   0.0,
                   kAxisSpeedMaximum);
    m_xSpeedGauge->setValueDecimals(2);
    m_xSpeedGauge->setStatusText(QStringLiteral("--"), kDirectionStopped);

    m_ySpeedGauge = new VerticalGauge(rowWidget);
    configureGauge(m_ySpeedGauge,
                   QStringLiteral("Y速度"),
                   QStringLiteral("mm/min"),
                   0.0,
                   kAxisSpeedMaximum);
    m_ySpeedGauge->setValueDecimals(2);
    m_ySpeedGauge->setStatusText(QStringLiteral("--"), kDirectionStopped);

    m_zSpeedGauge = new VerticalGauge(rowWidget);
    configureGauge(m_zSpeedGauge,
                   QStringLiteral("Z速度"),
                   QStringLiteral("mm/min"),
                   0.0,
                   kAxisSpeedMaximum);
    m_zSpeedGauge->setValueDecimals(2);
    m_zSpeedGauge->setStatusText(QStringLiteral("--"), kDirectionStopped);

    const QList<VerticalGauge *> gauges = {
        m_spindleSpeedGauge,
        m_spindleTorqueGauge,
        m_xSpeedGauge,
        m_ySpeedGauge,
        m_zSpeedGauge,
    };
    for (VerticalGauge *gauge : gauges) {
        row->addWidget(gauge, 1);
    }

    return rowWidget;
}

QWidget *RunningStatusPanel::createTaskProgressBlock(QWidget *parent)
{
    auto *block = new QFrame(parent);
    block->setObjectName(QStringLiteral("taskProgressPanel"));
    block->setStyleSheet(taskProgressPanelStyle());

    auto *layout = new QVBoxLayout(block);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(3);

    auto *title = makeLabel(QStringLiteral("任务进度"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString(
        "color:%1;font-size:14px;font-weight:600;"
        "border:none;background:transparent;")
        .arg(Theme::kTextPrimary));
    layout->addWidget(title);

    m_taskProgressRing = new ElaProgressRing(block);
    m_taskProgressRing->setFixedSize(145, 145);
    m_taskProgressRing->setRange(0, 100);
    m_taskProgressRing->setIsBusying(false);
    m_taskProgressRing->setIsSegmentedArc(true);
    m_taskProgressRing->setSegmentCount(24);
    m_taskProgressRing->setIsTransparent(false);
    m_taskProgressRing->setIsDisplayValue(true);
    m_taskProgressRing->setValueDisplayMode(
        ElaProgressRingType::ValueDisplayMode::Percent);
    m_taskProgressRing->setBusyingWidth(12);
    m_taskProgressRing->setValuePixelSize(30);
    m_taskProgressRing->setProgressColor(QColor(Theme::kAccent));
    m_taskProgressRing->setTrackColor(QColor(Theme::kDivider));
    m_taskProgressRing->setValueColor(QColor(Theme::kRealtime));
    m_taskProgressRing->setValue(0);
    layout->addWidget(m_taskProgressRing, 0, Qt::AlignHCenter);

    return block;
}

QWidget *RunningStatusPanel::createPositionGrid(const QString &title,
                                                const QString &keyPrefix,
                                                QWidget *parent)
{
    auto *gridWidget = new QFrame(parent);
    gridWidget->setObjectName(QStringLiteral("positionPanel"));
    gridWidget->setStyleSheet(positionPanelStyle());

    auto *outer = new QVBoxLayout(gridWidget);
    outer->setContentsMargins(0, 2, 0, 0);
    outer->setSpacing(5);

    auto *header = makeLabel(title, QStringLiteral("metricName"));
    header->setStyleSheet(QString(
        "color:%1;font-size:13px;font-weight:600;"
        "border:none;background:transparent;").arg(Theme::kTextSecondary));
    outer->addWidget(header);
    auto *rowWidget = new QWidget(gridWidget);
    auto *row = new QHBoxLayout(rowWidget);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    row->addWidget(createPositionBlock(QStringLiteral("X"), keyPrefix, rowWidget), 1);
    row->addWidget(createPositionBlock(QStringLiteral("Y"), keyPrefix, rowWidget), 1);
    row->addWidget(createPositionBlock(QStringLiteral("Z"), keyPrefix, rowWidget), 1);
    outer->addWidget(rowWidget);
    return gridWidget;
}

QWidget *RunningStatusPanel::createPositionBlock(const QString &axis,
                                                 const QString &keyPrefix,
                                                 QWidget *parent)
{
    auto *block = new QFrame(parent);
    block->setObjectName(QStringLiteral("positionRow"));
    block->setFixedHeight(46);
    block->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    block->setStyleSheet(positionRowStyle());

    auto *layout = new QHBoxLayout(block);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(7);

    auto *axisLabel = makeLabel(axis);
    axisLabel->setFixedWidth(22);
    axisLabel->setStyleSheet(QString(
        "color:%1;font-size:15px;font-weight:600;"
        "border:none;background:transparent;").arg(Theme::kAccent));

    auto *valueLabel = makeLabel(QStringLiteral("--"), QStringLiteral("metricValue"));
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setStyleSheet(QString(
        "color:%1;font-size:18px;font-weight:600;border:none;background:transparent;")
        .arg(kValueColorNoData));

    auto *unitLabel = makeLabel(QStringLiteral("mm"));
    unitLabel->setStyleSheet(QString(
        "color:%1;font-size:12px;border:none;background:transparent;")
        .arg(Theme::kTextMuted));

    layout->addWidget(axisLabel);
    layout->addStretch();
    layout->addWidget(valueLabel);
    layout->addWidget(unitLabel, 0, Qt::AlignVCenter);

    m_metricLabels.insert(QStringLiteral("%1%2").arg(keyPrefix, axis), valueLabel);
    return block;
}

QWidget *RunningStatusPanel::createPlcStatusBlock(QWidget *parent)
{
    m_plcStatusFrame = new QFrame(parent);
    m_plcStatusFrame->setObjectName(QStringLiteral("plcCurrentStatus"));
    m_plcStatusFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(m_plcStatusFrame);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(3);

    auto *title = makeLabel(QStringLiteral("PLC状态"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString(
        "color:%1;font-size:14px;font-weight:600;"
        "border:none;background:transparent;")
        .arg(Theme::kTextPrimary));
    layout->addWidget(title);

    m_plcStatusText = makeLabel(QStringLiteral("● 未知"));
    m_plcStatusText->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_plcStatusText, 1);

    clearPlcStatus();
    return m_plcStatusFrame;
}

void RunningStatusPanel::updatePlcStatus(quint16 statusWord, quint16 faultCode)
{
    if (!m_plcStatusFrame || !m_plcStatusText) {
        return;
    }

    const PlcStatusPresentation status = plcStatusPresentation(statusWord, faultCode);
    const QString borderColor = status.critical
        ? status.color
        : QString::fromLatin1(Theme::kDivider);
    const int borderWidth = status.critical ? 2 : 1;

    m_plcStatusFrame->setStyleSheet(QStringLiteral(R"(
        QFrame#plcCurrentStatus {
            background: %1;
            border: %2px solid %3;
            border-radius: 4px;
        }
    )").arg(Theme::kControlBg).arg(borderWidth).arg(borderColor));
    m_plcStatusText->setText(QStringLiteral("● %1").arg(status.text));
    m_plcStatusText->setStyleSheet(QString(
        "color:%1;font-size:26px;font-weight:600;"
        "border:none;background:transparent;").arg(status.color));
}

void RunningStatusPanel::clearPlcStatus()
{
    if (!m_plcStatusFrame || !m_plcStatusText) {
        return;
    }

    const QString unknownColor = QString::fromLatin1(Theme::kStateUnknown);
    m_plcStatusFrame->setStyleSheet(QStringLiteral(R"(
        QFrame#plcCurrentStatus {
            background: %1;
            border: 1px solid %2;
            border-radius: 4px;
        }
    )").arg(Theme::kControlBg, Theme::kDivider));
    m_plcStatusText->setText(QStringLiteral("● 未知"));
    m_plcStatusText->setStyleSheet(QString(
        "color:%1;font-size:26px;font-weight:600;"
        "border:none;background:transparent;").arg(unknownColor));
}

void RunningStatusPanel::setPositionValue(const QString &key, const QString &value)
{
    auto *label = m_metricLabels.value(key, nullptr);
    if (!label) {
        return;
    }

    const QColor color = value == QStringLiteral("--")
        ? QColor(kValueColorNoData)
        : kRealtimeValue;
    label->setText(value);
    label->setStyleSheet(QString(
        "color:%1;font-size:18px;font-weight:600;border:none;background:transparent;")
        .arg(color.name()));
}
