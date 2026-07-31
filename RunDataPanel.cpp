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

QString headerCardStyle()
{
    return QStringLiteral(R"(
        QFrame#taskHeaderCard {
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

    panelLayout->addWidget(createTaskHeader(panel));

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
    grid->addWidget(createMetricCard("completedPath",  "已完成路径",   "--", "条",  kAccent, panel), 3, 0);
    grid->addWidget(createMetricCard("currentPath",    "当前路径",     "--", "条",  kAccent, panel), 3, 1);
    grid->addWidget(createMetricCard("millingProgress", "任务进度",     "--", "%",   kAccent, panel), 4, 0);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    panelLayout->addLayout(grid);
    panelLayout->addStretch();

    applyStatus(TaskStatus::Idle);
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

void RunDataPanel::setMaxParticleHeight(int maxParticleHeight)
{
    if (maxParticleHeight < 0) {
        setMetricValue(QStringLiteral("particleHeight"), QStringLiteral("--"));
    } else {
        setMetricValue(QStringLiteral("particleHeight"),
                       QString::number(maxParticleHeight));
    }
}

void RunDataPanel::setPlcPathCounts(int completedPath, int currentPath)
{
    setMetricValue(QStringLiteral("completedPath"), QString::number(completedPath));
    setMetricValue(QStringLiteral("currentPath"), QString::number(currentPath));
}

void RunDataPanel::setMillingProgress(float progress)
{
    if (progress < 0.0f) {
        progress = 0.0f;
    }
    if (progress > 100.0f) {
        progress = 100.0f;
    }
    setMetricValue(QStringLiteral("millingProgress"),
                   QString::number(progress, 'f', 0));
}

void RunDataPanel::clearMetrics()
{
    const QStringList keys = {
        "particleCount", "particleArea", "particleHeight",
        "remaining", "totalTime", "cuttingVolume",
        "completedPath", "currentPath", "millingProgress"
    };
    for (const auto &k : keys) {
        setMetricValue(k, QStringLiteral("--"));
    }
}

void RunDataPanel::setMillingTask(quint64 taskId, int pathTotal, bool calibrationApplied)
{
    if (m_taskIdLabel) {
        m_taskIdLabel->setText(QString("任务 #%1 · 共 %2 条")
                                   .arg(taskId)
                                   .arg(pathTotal));
    }
    if (m_calibLabel) {
        if (calibrationApplied) {
            m_calibLabel->setText(QStringLiteral("已标定"));
            m_calibLabel->setStyleSheet(
                "color:#00E5FF;font-size:12px;font-weight:600;padding:2px 8px;"
                "border:1px solid #00E5FF;border-radius:3px;background:transparent;");
        } else {
            m_calibLabel->setText(QStringLiteral("未标定"));
            m_calibLabel->setStyleSheet(
                "color:#F1C40F;font-size:12px;font-weight:600;padding:2px 8px;"
                "border:1px solid #F1C40F;border-radius:3px;background:transparent;");
        }
    }
    applyStatus(TaskStatus::Running);
}

void RunDataPanel::setMillingStatus(bool finished, bool success)
{
    if (!finished) {
        applyStatus(TaskStatus::Running);
        return;
    }
    applyStatus(success ? TaskStatus::Success : TaskStatus::Failed);
}

void RunDataPanel::clearMillingTask()
{
    if (m_taskIdLabel) {
        m_taskIdLabel->setText(QStringLiteral("任务 --"));
    }
    if (m_calibLabel) {
        m_calibLabel->setText(QStringLiteral("--"));
        m_calibLabel->setStyleSheet(
            "color:#5A6A78;font-size:12px;font-weight:600;padding:2px 8px;"
            "border:1px solid #2E3D4D;border-radius:3px;background:transparent;");
    }
    applyStatus(TaskStatus::Idle);
}

QWidget *RunDataPanel::createTaskHeader(QWidget *parent)
{
    auto *header = new QFrame(parent);
    header->setObjectName("taskHeaderCard");
    header->setFixedHeight(44);
    header->setStyleSheet(headerCardStyle());

    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(10, 0, 12, 0);
    layout->setSpacing(10);

    m_taskIdLabel = makeLabel(QStringLiteral("任务 --"), "taskId");
    m_taskIdLabel->setStyleSheet(
        "color:#E6EEF5;font-size:15px;font-weight:600;border:none;background:transparent;");
    layout->addWidget(m_taskIdLabel);

    layout->addStretch();

    m_statusDot = makeLabel(QStringLiteral("●"), "statusDot");
    m_statusDot->setStyleSheet(
        QString("color:%1;font-size:16px;border:none;background:transparent;")
            .arg(kValueColorNoData));
    layout->addWidget(m_statusDot);

    m_statusText = makeLabel(QStringLiteral("待命"), "statusText");
    m_statusText->setStyleSheet(
        "color:#8A9AA8;font-size:14px;border:none;background:transparent;");
    layout->addWidget(m_statusText);

    m_calibLabel = makeLabel(QStringLiteral("--"), "calibTag");
    m_calibLabel->setStyleSheet(
        "color:#5A6A78;font-size:12px;font-weight:600;padding:2px 8px;"
        "border:1px solid #2E3D4D;border-radius:3px;background:transparent;");
    layout->addWidget(m_calibLabel);

    return header;
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

void RunDataPanel::applyStatus(TaskStatus status)
{
    if (!m_statusDot || !m_statusText) {
        return;
    }

    QString dotColor;
    QString text;
    switch (status) {
    case TaskStatus::Idle:
        dotColor = kValueColorNoData;
        text = QStringLiteral("待命");
        break;
    case TaskStatus::Running:
        dotColor = "#00E5FF";
        text = QStringLiteral("进行中");
        break;
    case TaskStatus::Success:
        dotColor = "#2ECC71";
        text = QStringLiteral("已完成");
        break;
    case TaskStatus::Failed:
        dotColor = "#E74C3C";
        text = QStringLiteral("失败");
        break;
    }

    m_statusDot->setStyleSheet(
        QString("color:%1;font-size:16px;border:none;background:transparent;")
            .arg(dotColor));
    m_statusText->setText(text);
    m_statusText->setStyleSheet(
        QString("color:%1;font-size:14px;border:none;background:transparent;")
            .arg(status == TaskStatus::Idle ? kValueColorNoData : kValueColorNormal));
}
