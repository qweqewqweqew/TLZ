#include "ui/panels/KpiStripPanel.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVariant>
#include <QVBoxLayout>

namespace {

constexpr int kTileHeight = 84;
constexpr int kIconBox = 40;

} // namespace

KpiStripPanel::KpiStripPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(makeTile(QStringLiteral("检测粒子"), QStringLiteral("个"),
                               QStringLiteral(":/img/kpi_target.svg"), Theme::kAccent, m_detected));
    layout->addWidget(makeTile(QStringLiteral("待修磨"), QStringLiteral("个"),
                               QStringLiteral(":/img/kpi_clock.svg"), Theme::kStateWarning, m_pending));
    layout->addWidget(makeTile(QStringLiteral("已完成"), QStringLiteral("个"),
                               QStringLiteral(":/img/kpi_check.svg"), Theme::kStateSuccess, m_completed));
    layout->addWidget(makeTile(QStringLiteral("板级判定"), QString(),
                               QStringLiteral(":/img/kpi_alert.svg"), Theme::kAccent, m_review));
    clear();
    Theme::watch(this, [this]() {
        setCounts(m_detectedCount, m_pendingCount, m_completedCount);
        setBoardGrade(m_grade);
        for (QLabel *icon : findChildren<QLabel *>()) {
            const QString resource = icon->property("iconResource").toString();
            if (resource.isEmpty()) {
                continue;
            }
            const int tint = icon->property("kpiTint").toInt();
            const char *color = tint == 1 ? Theme::kStateWarning
                              : tint == 2 ? Theme::kStateSuccess
                                          : Theme::kAccent;
            icon->setPixmap(tintedIcon(resource, QColor(color), 22, devicePixelRatioF()));
        }
    });
}

QWidget *KpiStripPanel::makeTile(const QString &title, const QString &unit, const QString &iconResource,
                                 const QString &accent, QLabel *&valueOut)
{
    auto *tile = new QFrame(this);
    tile->setObjectName(QStringLiteral("panel"));
    tile->setFixedHeight(kTileHeight);
    auto *layout = new QHBoxLayout(tile);
    layout->setContentsMargins(18, 10, 18, 10);
    layout->setSpacing(12);

    auto *texts = new QVBoxLayout();
    texts->setContentsMargins(0, 0, 0, 0);
    texts->setSpacing(2);
    auto *caption = makeLabel(title);
    caption->setObjectName(QStringLiteral("kpiCaption"));
    Theme::bindStyle(caption, []() {
        return QStringLiteral("color:%1;font-size:13px;border:none;background:transparent;")
            .arg(Theme::kTextSecondary);
    });
    texts->addWidget(caption);

    auto *valueRow = new QHBoxLayout();
    valueRow->setContentsMargins(0, 0, 0, 0);
    valueRow->setSpacing(6);
    valueOut = makeLabel(QStringLiteral("--"));
    valueRow->addWidget(valueOut, 0, Qt::AlignBottom);
    if (!unit.isEmpty()) {
        auto *unitLabel = makeLabel(unit);
        unitLabel->setObjectName(QStringLiteral("kpiUnit"));
        Theme::bindStyle(unitLabel, []() {
            return QStringLiteral("color:%1;font-size:13px;border:none;background:transparent;padding-bottom:5px;")
                .arg(Theme::kTextMuted);
        });
        valueRow->addWidget(unitLabel, 0, Qt::AlignBottom);
    }
    valueRow->addStretch(1);
    texts->addLayout(valueRow);
    layout->addLayout(texts, 1);

    auto *icon = new QLabel(tile);
    icon->setFixedSize(kIconBox, kIconBox);
    icon->setAlignment(Qt::AlignCenter);
    int tint = 0;
    if (accent == QLatin1String(Theme::kStateWarning)) {
        tint = 1;
    } else if (accent == QLatin1String(Theme::kStateSuccess)) {
        tint = 2;
    }
    icon->setProperty("iconResource", iconResource);
    icon->setProperty("kpiTint", tint);
    icon->setPixmap(tintedIcon(iconResource, QColor(accent), 22, devicePixelRatioF()));
    Theme::bindStyle(icon, []() {
        return QStringLiteral("background:%1;border:none;border-radius:%2px;")
            .arg(Theme::kControlBg)
            .arg(kIconBox / 2);
    });
    layout->addWidget(icon, 0, Qt::AlignVCenter);
    return tile;
}

void KpiStripPanel::setValue(QLabel *label, int value, const QString &accent)
{
    if (!label) {
        return;
    }
    const bool hasData = value >= 0;
    label->setText(hasData ? QString::number(value) : QStringLiteral("--"));
    label->setStyleSheet(QStringLiteral("color:%1;font-size:28px;font-weight:700;border:none;background:transparent;")
                             .arg(hasData ? accent : QString::fromLatin1(Theme::kTextMuted)));
}

void KpiStripPanel::setCounts(int detected, int pending, int completed)
{
    m_detectedCount = detected;
    m_pendingCount = pending;
    m_completedCount = completed;
    const bool singleHighlight = Theme::current() == Theme::Id::Light;
    const char *rest = singleHighlight ? Theme::kTitle : Theme::kAccent;
    setValue(m_detected, detected, Theme::kAccent);
    setValue(m_pending, pending, rest);
    setValue(m_completed, completed, rest);
}

void KpiStripPanel::setBoardGrade(const QString &grade)
{
    if (!m_review) {
        return;
    }
    m_grade = grade;
    const QString text = grade.trimmed();
    const bool hasData = !text.isEmpty();
    m_review->setText(hasData ? text : QStringLiteral("--"));
    m_review->setStyleSheet(QStringLiteral("color:%1;font-size:28px;font-weight:700;border:none;background:transparent;")
                                .arg(hasData ? QString::fromLatin1(Theme::current() == Theme::Id::Light
                                                                             ? Theme::kTitle
                                                                             : Theme::kAccent)
                                             : QString::fromLatin1(Theme::kTextMuted)));
}

void KpiStripPanel::clear()
{
    setCounts(-1, -1, -1);
    setBoardGrade(QString());
}
