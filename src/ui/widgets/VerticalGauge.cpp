#include "ui/widgets/VerticalGauge.h"

#include "app/Theme.h"

#include <QColor>
#include <QEasingCurve>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QVariantAnimation>
#include <QtMath>

#include <algorithm>

namespace {

const QColor kGaugeCardSurface(Theme::kControlBg);
const QColor kGaugeCardOutline(Theme::kDivider);
const QColor kGaugeTrack(Theme::kWindowBg);
const QColor kGaugeTrackOutline(Theme::kBorder);
const QColor kGaugeTick(Theme::kTextMuted);
const QColor kGaugeZero(Theme::kTextSecondary);
const QColor kTextPrimary(Theme::kTextPrimary);
const QColor kTextSecondary(Theme::kTextSecondary);
const QColor kRealtimeValue(Theme::kRealtime);
const QColor kNoData(Theme::kStateUnknown);

constexpr int kAnimationDurationMs = 140;
constexpr int kMajorIntervals = 4;
constexpr int kMinorIntervals = 20;

QFont gaugeFont(qreal pointSize, int weight = QFont::Normal)
{
    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setPointSizeF(pointSize);
    font.setWeight(weight);
    return font;
}

} // namespace

VerticalGauge::VerticalGauge(QWidget *parent)
    : QWidget(parent)
    , m_statusColor(Theme::kStateUnknown)
    , m_animation(new QVariantAnimation(this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_animation->setDuration(kAnimationDurationMs);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_displayedValue = value.toDouble();
                update();
            });
}

void VerticalGauge::setRange(double minimum, double maximum)
{
    if (!qIsFinite(minimum) || !qIsFinite(maximum) || maximum <= minimum) {
        return;
    }
    m_minimum = minimum;
    m_maximum = maximum;
    update();
}

void VerticalGauge::setValue(double value)
{
    if (!qIsFinite(value)) {
        clearValue();
        return;
    }

    m_animation->stop();
    m_value = value;
    if (!m_hasValue) {
        m_displayedValue = value;
        m_hasValue = true;
        update();
    } else {
        m_animation->setStartValue(m_displayedValue);
        m_animation->setEndValue(value);
        m_animation->start();
    }
    updateAccessibleText();
}

void VerticalGauge::clearValue()
{
    m_animation->stop();
    m_value = 0.0;
    m_displayedValue = 0.0;
    m_hasValue = false;
    updateAccessibleText();
    update();
}

void VerticalGauge::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    updateAccessibleText();
    update();
}

void VerticalGauge::setUnit(const QString &unit)
{
    if (m_unit == unit) {
        return;
    }
    m_unit = unit;
    updateAccessibleText();
    update();
}

void VerticalGauge::setStatusText(const QString &text, const QColor &color)
{
    const QColor resolvedColor = color.isValid() ? color : kTextSecondary;
    if (m_statusText == text && m_statusColor == resolvedColor) {
        return;
    }
    m_statusText = text;
    m_statusColor = resolvedColor;
    updateAccessibleText();
    update();
}

void VerticalGauge::setValueDecimals(int decimals)
{
    if (decimals < 0 || decimals > 3 || m_valueDecimals == decimals) {
        return;
    }
    m_valueDecimals = decimals;
    updateAccessibleText();
    update();
}

QSize VerticalGauge::sizeHint() const
{
    return QSize(112, 267);
}

QSize VerticalGauge::minimumSizeHint() const
{
    return QSize(86, 240);
}

void VerticalGauge::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(kGaugeCardOutline, 1.0));
    painter.setBrush(kGaugeCardSurface);
    painter.drawRoundedRect(surface, 4.0, 4.0);

    painter.setFont(gaugeFont(8.8, QFont::DemiBold));
    painter.setPen(kTextPrimary);
    painter.drawText(QRectF(3.0, 5.0, width() - 6.0, 18.0),
                     Qt::AlignCenter, m_title);

    painter.setFont(gaugeFont(7.5));
    painter.setPen(kTextSecondary);
    painter.drawText(QRectF(3.0, 22.0, width() - 6.0, 14.0),
                     Qt::AlignCenter, m_unit);

    painter.setFont(gaugeFont(7.5, QFont::DemiBold));
    painter.setPen(m_statusColor);
    painter.drawText(QRectF(3.0, 37.0, width() - 6.0, 14.0),
                     Qt::AlignCenter, m_statusText);

    const qreal gaugeTop = 57.0;
    const qreal gaugeBottom = std::max(gaugeTop + 60.0, height() - 47.0);
    const qreal scaleX = std::clamp(width() * 0.48, 42.0, 55.0);
    const qreal trackWidth = std::clamp(width() * 0.14, 12.0, 16.0);
    const qreal trackLeft = scaleX + 6.0;
    const QRectF track(trackLeft, gaugeTop, trackWidth, gaugeBottom - gaugeTop);

    painter.setPen(QPen(kGaugeTrackOutline, 1.0));
    painter.setBrush(kGaugeTrack);
    painter.drawRoundedRect(track, 2.0, 2.0);

    painter.setFont(gaugeFont(7.0));
    painter.setPen(kGaugeTick);
    painter.drawLine(QPointF(scaleX, track.top()),
                     QPointF(scaleX, track.bottom()));
    for (int index = 0; index <= kMinorIntervals; ++index) {
        const double ratio = double(index) / kMinorIntervals;
        const qreal y = track.top() + ratio * track.height();
        const bool isMajor = index % (kMinorIntervals / kMajorIntervals) == 0;
        const qreal tickLength = isMajor ? 6.0 : 3.5;

        painter.drawLine(QPointF(scaleX - tickLength, y),
                         QPointF(scaleX, y));
        if (isMajor) {
            const double tickValue = m_maximum - ratio * (m_maximum - m_minimum);
            painter.drawText(QRectF(1.0, y - 8.0, scaleX - 9.0, 16.0),
                             Qt::AlignRight | Qt::AlignVCenter,
                             formattedValue(tickValue, true));
        }
    }

    const double zeroValue = std::clamp(0.0, m_minimum, m_maximum);
    const qreal zeroY = valueToY(zeroValue, track);
    if (m_hasValue) {
        const qreal valueY = valueToY(m_displayedValue, track);
        QRectF fill(track.left() + 2.0,
                    std::min(valueY, zeroY),
                    track.width() - 4.0,
                    std::max(2.0, qAbs(valueY - zeroY)));
        if (valueY > zeroY) {
            fill.moveTop(zeroY);
        }
        fill.setBottom(std::max(valueY, zeroY));

        QLinearGradient fillGradient(track.bottomLeft(), track.topLeft());
        fillGradient.setColorAt(0.0, QColor(Theme::kAccent));
        fillGradient.setColorAt(1.0, QColor(Theme::kRealtime));

        painter.setPen(Qt::NoPen);
        painter.setBrush(fillGradient);
        painter.drawRoundedRect(fill, 1.0, 1.0);
    }

    painter.setPen(QPen(kGaugeZero, 1.2));
    painter.drawLine(QPointF(track.left() - 3.0, zeroY),
                     QPointF(track.right() + 3.0, zeroY));

    painter.setFont(gaugeFont(13.5, QFont::DemiBold));
    painter.setPen(m_hasValue ? kRealtimeValue : kNoData);
    painter.drawText(QRectF(3.0,
                            track.bottom() + 4.0,
                            width() - 6.0,
                            height() - track.bottom() - 7.0),
                     Qt::AlignCenter,
                     m_hasValue ? formattedValue(m_displayedValue) : QStringLiteral("--"));
}

double VerticalGauge::valueToY(double value, const QRectF &track) const
{
    const double clamped = std::clamp(value, m_minimum, m_maximum);
    const double ratio = (clamped - m_minimum) / (m_maximum - m_minimum);
    return track.bottom() - ratio * track.height();
}

QString VerticalGauge::formattedValue(double value, bool tickLabel) const
{
    const double magnitude = std::max(qAbs(m_minimum), qAbs(m_maximum));
    const int precision = !tickLabel && m_valueDecimals >= 0
        ? m_valueDecimals
        : ((tickLabel && magnitude >= 100.0) || magnitude >= 1000.0 ? 0 : 1);
    return QString::number(value, 'f', precision);
}

void VerticalGauge::updateAccessibleText()
{
    const QString name = m_unit.isEmpty()
        ? m_title
        : QStringLiteral("%1，单位 %2").arg(m_title, m_unit);
    setAccessibleName(name);
    QString description = m_hasValue
        ? QStringLiteral("当前值 %1").arg(formattedValue(m_value))
        : QStringLiteral("暂无数据");
    if (!m_statusText.isEmpty()) {
        description += QStringLiteral("，状态 %1").arg(m_statusText);
    }
    setAccessibleDescription(description);
}
