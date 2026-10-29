#include "ui/panels/MachineStatusPanel.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "bridge/PlcFeedbackVM.h"
#include "bridge/PlcStatusPresentation.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QEasingCurve>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QHideEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QShowEvent>
#include <QTimer>
#include <QVariantAnimation>
#include <QPushButton>
#include <QVariant>
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

// 控制按钮：语义色铺底，悬停/按下用同色加深；图标与文字一起按主题重刷。
void applyControlButtonStyle(QPushButton *button, const QColor &base)
{
    if (!button) {
        return;
    }
    // 黄色底用深色文字保证对比度，绿/红底用白色。
    const int hue = base.hue();
    const QColor textColor = (hue >= 25 && hue <= 70) ? QColor(0x1A, 0x1A, 0x1A) : QColor(Qt::white);
    const QString iconPath = button->property("controlIcon").toString();
    if (!iconPath.isEmpty()) {
        button->setIcon(QIcon(tintedIcon(iconPath, textColor, 16, button->devicePixelRatioF())));
        button->setIconSize(QSize(16, 16));
    }
    button->setStyleSheet(QStringLiteral(
                              "QPushButton {"
                              "  background:%1; color:%6; border:none; border-radius:6px;"
                              "  padding:4px 6px; font-size:13px; font-weight:600;"
                              "}"
                              "QPushButton:hover { background:%2; }"
                              "QPushButton:pressed { background:%3; }"
                              "QPushButton:disabled { background:%4; color:%5; }")
                              .arg(base.name(),
                                   base.darker(110).name(),
                                   base.darker(128).name(),
                                   QLatin1String(Theme::kControlBg),
                                   QLatin1String(Theme::kTextMuted),
                                   textColor.name()));
}

QPushButton *makeControlButton(const QString &text, const QString &iconPath,
                               const QColor &base, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("controlIcon", iconPath);
    applyControlButtonStyle(button, base);
    return button;
}

// 窄栏里的实时数：前三项用横条，X/Y/Z 用一排小格。数值仍走原来的六个键。
class QcpGaugeBoard : public QWidget
{
public:
    explicit QcpGaugeBoard(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(8);

        for (int i = 0; i < 3; ++i) {
            const MetricDefinition &definition = kMetricDefinitions[std::size_t(i)];
            auto *row = new QWidget(this);
            auto *layout = new QVBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(2);

            auto *head = new QHBoxLayout();
            head->setContentsMargins(0, 0, 0, 0);
            head->setSpacing(8);
            auto *name = new QLabel(QString::fromUtf8(definition.title), row);
            auto *value = new QLabel(QStringLiteral("--"), row);
            value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            head->addWidget(name, 1);
            head->addWidget(value, 0);

            auto *bar = new QProgressBar(row);
            bar->setRange(0, 1000);
            bar->setValue(0);
            bar->setTextVisible(false);
            bar->setFixedHeight(6);

            layout->addLayout(head);
            layout->addWidget(bar);
            root->addWidget(row);
            m_names[std::size_t(i)] = name;
            m_values[std::size_t(i)] = value;
            m_bars[std::size_t(i)] = bar;
        }

        auto *tiles = new QHBoxLayout();
        tiles->setContentsMargins(0, 0, 0, 0);
        tiles->setSpacing(6);
        for (int i = 3; i < int(kMetricDefinitions.size()); ++i) {
            const MetricDefinition &definition = kMetricDefinitions[std::size_t(i)];
            auto *tile = new QFrame(this);
            tile->setObjectName(QStringLiteral("metricTile"));
            auto *layout = new QVBoxLayout(tile);
            layout->setContentsMargins(8, 6, 8, 6);
            layout->setSpacing(2);
            auto *name = new QLabel(QString::fromUtf8(definition.title), tile);
            auto *value = new QLabel(QStringLiteral("--"), tile);
            layout->addWidget(name);
            layout->addWidget(value);
            tiles->addWidget(tile, 1);
            m_names[std::size_t(i)] = name;
            m_values[std::size_t(i)] = value;
            m_tiles[std::size_t(i - 3)] = tile;
        }
        root->addLayout(tiles);

        Theme::watch(this, [this]() { applyTheme(); });
        applyTheme();
    }

    void setValue(const QString &key, double value, bool hasData)
    {
        const int index = indexForKey(key);
        if (index < 0 || index >= int(kMetricDefinitions.size())) {
            return;
        }

        const MetricDefinition &definition = kMetricDefinitions[std::size_t(index)];
        m_valid[std::size_t(index)] = hasData;
        if (!hasData) {
            m_values[std::size_t(index)]->setText(QStringLiteral("--"));
            if (m_bars[std::size_t(index)]) {
                m_bars[std::size_t(index)]->setValue(0);
            }
        } else {
            m_values[std::size_t(index)]->setText(
                QStringLiteral("%1 %2")
                    .arg(QString::number(value, 'f', definition.decimals),
                         QString::fromUtf8(definition.unit)));
            if (QProgressBar *bar = m_bars[std::size_t(index)]) {
                const double span = definition.max - definition.min;
                const double ratio = span > 0.0
                    ? qBound(0.0, (value - definition.min) / span, 1.0)
                    : 0.0;
                bar->setValue(int(std::lround(ratio * 1000.0)));
            }
        }
        refreshValueStyle(index);
    }

    void clear()
    {
        for (int i = 0; i < int(kMetricDefinitions.size()); ++i) {
            setValue(QString::fromLatin1(kMetricDefinitions[std::size_t(i)].key), 0.0, false);
        }
    }

    void applyTheme()
    {
        const QString nameStyle = QStringLiteral(
            "color:%1;font-size:12px;background:transparent;border:none;")
                                      .arg(Theme::kTextSecondary);
        const QString barStyle = QStringLiteral(
            "QProgressBar { background:%1; border:none; border-radius:3px; }"
            "QProgressBar::chunk { background:%2; border-radius:3px; }")
                                     .arg(Theme::kBorder, Theme::kAccent);
        const QString tileStyle = QStringLiteral(
            "QFrame#metricTile { background:%1; border:1px solid %2; border-radius:6px; }")
                                      .arg(Theme::kControlBg, Theme::kBorder);
        for (int i = 0; i < int(kMetricDefinitions.size()); ++i) {
            if (m_names[std::size_t(i)]) {
                m_names[std::size_t(i)]->setStyleSheet(nameStyle);
            }
            if (m_bars[std::size_t(i)]) {
                m_bars[std::size_t(i)]->setStyleSheet(barStyle);
            }
            refreshValueStyle(i);
        }
        for (QFrame *tile : m_tiles) {
            if (tile) {
                tile->setStyleSheet(tileStyle);
            }
        }
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

    void refreshValueStyle(int index)
    {
        if (index < 0 || !m_values[std::size_t(index)]) {
            return;
        }
        const char *color = m_valid[std::size_t(index)] ? Theme::liveColor() : Theme::kTextMuted;
        m_values[std::size_t(index)]->setStyleSheet(
            QStringLiteral("color:%1;font-size:13px;font-weight:700;background:transparent;border:none;")
                .arg(QLatin1String(color)));
    }

    std::array<QLabel *, kMetricDefinitions.size()> m_names{};
    std::array<QLabel *, kMetricDefinitions.size()> m_values{};
    std::array<QProgressBar *, kMetricDefinitions.size()> m_bars{};
    std::array<QFrame *, 3> m_tiles{};
    std::array<bool, kMetricDefinitions.size()> m_valid{};
};

class MillProgressTrack : public QWidget
{
public:
    enum class Mode { Idle, Running, Done, Failed };

    explicit MillProgressTrack(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(28);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(300);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_shown = value.toReal();
            update();
        });

        m_shimmer = new QTimer(this);
        m_shimmer->setInterval(32);
        connect(m_shimmer, &QTimer::timeout, this, [this]() {
            m_phase += 0.012;
            if (m_phase > 1.0) {
                m_phase -= 1.0;
            }
            update();
        });
        m_shimmer->start();
        Theme::watch(this, [this]() { update(); });
    }

    void setMode(Mode mode)
    {
        if (m_mode == mode) {
            return;
        }
        m_mode = mode;
        if (m_mode == Mode::Idle) {
            if (!m_shimmer->isActive()) {
                m_shimmer->start();
            }
        } else {
            m_shimmer->stop();
        }
        update();
    }

    void setValue(int value)
    {
        value = qBound(0, value, 100);
        if (m_target == value) {
            return;
        }
        m_target = value;
        m_anim->stop();
        m_anim->setStartValue(m_shown);
        m_anim->setEndValue(static_cast<qreal>(value));
        m_anim->start();
    }

    int value() const { return m_target; }

    QSize sizeHint() const override { return QSize(160, 28); }

protected:
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        if (m_mode == Mode::Idle && m_shimmer && !m_shimmer->isActive()) {
            m_shimmer->start();
        }
    }

    void hideEvent(QHideEvent *event) override
    {
        if (m_shimmer) {
            m_shimmer->stop();
        }
        QWidget::hideEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = track.height() / 2.0;
        const bool light = Theme::current() == Theme::Id::Light;
        const QColor trackColor = light ? QColor(QStringLiteral("#EBEDF0"))
                                         : QColor(Theme::kControlBg);
        QPainterPath clip;
        clip.addRoundedRect(track, radius, radius);
        painter.setClipPath(clip);
        painter.fillRect(track, trackColor);

        QColor fillColor = QColor(Theme::kAccent);
        qreal fillRatio = m_shown / 100.0;
        if (m_mode == Mode::Done) {
            fillColor = QColor(Theme::kStateSuccess);
            fillRatio = 1.0;
        } else if (m_mode == Mode::Failed) {
            fillColor = QColor(Theme::kStateError);
            fillRatio = 1.0;
        } else if (m_mode == Mode::Idle) {
            fillRatio = 0.0;
            const qreal band = 56.0;
            const qreal travel = track.width() + band * 2.0;
            const qreal x = track.left() - band + travel * m_phase;
            QLinearGradient sheen(x, 0.0, x + band, 0.0);
            const QColor glow = light ? QColor(255, 255, 255, 180) : QColor(255, 255, 255, 36);
            sheen.setColorAt(0.0, QColor(glow.red(), glow.green(), glow.blue(), 0));
            sheen.setColorAt(0.5, glow);
            sheen.setColorAt(1.0, QColor(glow.red(), glow.green(), glow.blue(), 0));
            painter.fillRect(QRectF(x, track.top(), band, track.height()), sheen);
        }

        const qreal fillWidth = qBound(0.0, track.width() * fillRatio, track.width());
        if (fillWidth > 0.0) {
            painter.fillRect(QRectF(track.left(), track.top(), fillWidth, track.height()), fillColor);
        }

        const QString label = m_mode == Mode::Failed ? QStringLiteral("失败")
                             : QStringLiteral("%1%").arg(m_target);
        QFont font(QStringLiteral("Segoe UI"));
        font.setPixelSize(14);
        font.setWeight(QFont::DemiBold);
        font.setStyleStrategy(QFont::PreferQuality);
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const bool showCheck = m_mode == Mode::Done;
        const int checkSlot = showCheck ? 16 : 0;
        const int textWidth = metrics.horizontalAdvance(label);
        const qreal groupWidth = textWidth + checkSlot;
        const qreal groupLeft = track.center().x() - groupWidth / 2.0;
        const QRectF textRect(groupLeft, track.top(), textWidth, track.height());
        const QColor onTrack = light ? QColor(QStringLiteral("#1F2937")) : QColor(QStringLiteral("#F8FAFC"));
        const QColor onFill = fillColor.lightness() > 170 ? QColor(QStringLiteral("#0F172A")) : QColor(Qt::white);

        const auto paintLabel = [&](const QColor &color) {
            painter.setPen(color);
            painter.drawText(textRect, Qt::AlignCenter, label);
            if (!showCheck) {
                return;
            }
            QPen pen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            const QPointF origin(textRect.right() + 4.0, track.center().y() - 4.0);
            QPainterPath check;
            check.moveTo(origin + QPointF(0.0, 4.0));
            check.lineTo(origin + QPointF(3.2, 7.0));
            check.lineTo(origin + QPointF(9.0, 0.6));
            painter.drawPath(check);
        };

        painter.save();
        painter.setClipRect(QRectF(track.left(), track.top(), fillWidth, track.height()), Qt::IntersectClip);
        paintLabel(onFill);
        painter.restore();
        painter.save();
        painter.setClipRect(QRectF(track.left() + fillWidth, track.top(),
                                   track.width() - fillWidth, track.height()),
                            Qt::IntersectClip);
        paintLabel(onTrack);
        painter.restore();
    }

private:
    Mode m_mode{Mode::Idle};
    int m_target{0};
    qreal m_shown{0.0};
    qreal m_phase{0.0};
    QVariantAnimation *m_anim{nullptr};
    QTimer *m_shimmer{nullptr};
};

} // namespace

MachineStatusPanel::MachineStatusPanel(QWidget *parent)
    : QFrame(parent)
{
    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("panel"));
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    auto *taskRow = new QHBoxLayout(panel);
    taskRow->setContentsMargins(12, 10, 12, 10);
    taskRow->setSpacing(0);
    auto *track = new MillProgressTrack(panel);
    m_progress = track;
    taskRow->addWidget(track, 1);

    // 实时数据单独放到右侧信息栏。
    m_metricColumn = new QcpGaugeBoard(this);

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
    m_plcLinked = true;
    m_plcColor = plc.color;
    if (m_plcPill) {
        m_plcPill->setText(QStringLiteral("PLC \xc2\xb7 %1").arg(text));
        m_plcPill->setToolTip(QStringLiteral("状态字 0x%1 \xc2\xb7 故障码 %2")
                                  .arg(feedback.statusWord, 4, 16, QLatin1Char('0'))
                                  .arg(feedback.faultCode));
        applyPlcPill();
    }

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
    if (auto *track = static_cast<MillProgressTrack *>(m_progress)) {
        track->setMode(MillProgressTrack::Mode::Running);
    }
    setTaskProgress(0.0f);
    refreshTaskLine();
}

void MachineStatusPanel::setTaskProgress(float progressPercent)
{
    const int percent = qRound(qBound(0.0f, progressPercent, 100.0f));
    if (auto *track = static_cast<MillProgressTrack *>(m_progress)) {
        // 上一轮已经画成完成或失败。新的进度低于 100 说明是下一轮，先退出那个状态。
        if (m_finished && percent < 100) {
            m_finished = false;
            m_success = false;
            track->setMode(MillProgressTrack::Mode::Running);
        } else if (!m_finished && percent > 0) {
            track->setMode(MillProgressTrack::Mode::Running);
        }
        track->setValue(percent);
    }
}

void MachineStatusPanel::setTaskFinished(bool success)
{
    m_finished = true;
    m_success = success;
    if (auto *track = static_cast<MillProgressTrack *>(m_progress)) {
        track->setMode(success ? MillProgressTrack::Mode::Done : MillProgressTrack::Mode::Failed);
    }
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
    if (auto *track = static_cast<MillProgressTrack *>(m_progress)) {
        track->setMode(MillProgressTrack::Mode::Idle);
    }
    setTaskProgress(0.0f);
    m_plcLinked = false;
    m_plcColor.clear();
    if (m_plcPill) {
        m_plcPill->setText(QStringLiteral("PLC \xc2\xb7 \xe6\x9c\xaa\xe8\xbf\x9e\xe6\x8e\xa5"));
        m_plcPill->setToolTip(QString());
        applyPlcPill();
    }
    for (const char *key : {"spindle", "torque", "feed", "x", "y", "z"}) {
        setMetric(QString::fromLatin1(key), 0.0, 0, false);
    }
    refreshTaskLine();
}

void MachineStatusPanel::refreshTaskLine()
{
    if (!m_taskLine) {
        return;
    }
    if (m_taskId == 0) {
        m_taskLine->setText(QStringLiteral("待命"));
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
    if (m_progress) {
        m_progress->update();
    }
    if (auto *gaugeBoard = static_cast<QcpGaugeBoard *>(m_metricColumn)) {
        gaugeBoard->applyTheme();
    }
    applyPlcPill();
    refreshTaskLine();
}
