#include "RunDataPanel.h"

#include "UiHelpers.h"

#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

const QColor kAccent("#00E5FF");

const QString kValueColorNormal = "#E6EEF5";
const QString kValueColorNoData = "#5A6A78";

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

} // namespace

RunDataPanel::RunDataPanel(QWidget *parent)
    : QFrame(parent)
{
    auto *panel = createPanel("运行数据", nullptr, this);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    panelLayout->setSpacing(8);

    auto *grid = new QGridLayout();
    grid->setContentsMargins(2, 0, 2, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);

    grid->addWidget(createMetricCard("particleCount",  "粒子数量",     "--", "个",  kAccent, panel), 0, 0);
    grid->addWidget(createMetricCard("particleArea",   "粒子面积",     "--", "mm²", kAccent, panel), 0, 1);
    grid->addWidget(createMetricCard("particleHeight", "粒子最大高度", "--", "mm",  kAccent, panel), 1, 0);
    grid->addWidget(createMetricCard("remaining",      "剩余",         "--", "个",  kAccent, panel), 1, 1);
    grid->addWidget(createMetricCard("totalTime",      "总耗时",       "--", "",    kAccent, panel), 2, 0);
    grid->addWidget(createMetricCard("cuttingVolume",  "切削量",       "--", "mm³", kAccent, panel), 2, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    panelLayout->addLayout(grid);
    panelLayout->addStretch();
}

void RunDataPanel::setMetricValue(const QString &key, const QString &value)
{
    auto *label = m_metricLabels.value(key, nullptr);
    if (!label)
        return;

    const QString color = (value == "--") ? kValueColorNoData : kValueColorNormal;
    label->setText(value);
    label->setStyleSheet(QString("color:%1;font-size:24px;font-weight:600;border:none;background:transparent;")
                             .arg(color));
}

QWidget *RunDataPanel::createMetricCard(const QString &key,
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
                                  .arg(kValueColorNoData));
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
