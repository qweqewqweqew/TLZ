#include "ui/panels/RunDataPanel.h"

#include "ElaIcon.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const QColor kAccent(Theme::kAccent);

const QString kValueColorNormal = Theme::kTextPrimary;
const QString kValueColorNoData = Theme::kTextMuted;

QString cardStyle()
{
    return QStringLiteral(R"(
        QFrame#metricCard {
            background: %1;
            border: none;
            border-radius: 6px;
        }
    )").arg(Theme::kControlBg);
}

QString headerCardStyle()
{
    return QStringLiteral(R"(
        QFrame#taskHeaderCard {
            background: %1;
            border: none;
            border-radius: 6px;
        }
    )").arg(Theme::kControlBg);
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
    panelLayout->setSpacing(10);

    panelLayout->addWidget(createTaskHeader(panel));

    auto *grid = new QGridLayout();
    grid->setContentsMargins(2, 0, 2, 0);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(12);

    auto *icons = ElaIcon::getInstance();
    grid->addWidget(createMetricCard(
                        "particleHeight", "粒子最大高度", "--", "mm",
                        icons->getElaIcon(ElaIconType::RulerVertical, 16, kAccent), panel),
                    0, 0);
    grid->addWidget(createMetricCard(
                        "taskId", "任务ID", "--", "",
                        icons->getElaIcon(ElaIconType::Hashtag, 16, kAccent), panel),
                    0, 1);
    grid->addWidget(createMetricCard(
                        "completedPath", "已完成路径", "--", "条",
                        icons->getElaIcon(ElaIconType::CircleCheck, 16, kAccent), panel),
                    1, 0);
    grid->addWidget(createMetricCard(
                        "currentPath", "当前路径", "--", "条",
                        icons->getElaIcon(ElaIconType::Route, 16, kAccent), panel),
                    1, 1);
    grid->addWidget(createMetricCard(
                        "particleGrade", "等级", "--", "",
                        icons->getElaIcon(ElaIconType::LayerGroup, 16, kAccent), panel),
                    2, 0);
    grid->addWidget(createMetricCard(
                        "cutEmptyPath", "切削/空移", "--", "",
                        icons->getElaIcon(ElaIconType::Split, 16, kAccent), panel),
                    2, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid->setRowStretch(2, 1);

    panelLayout->addLayout(grid, 1);

    applyStatus(TaskStatus::Idle);
}

void RunDataPanel::setMetricValue(const QString &key, const QString &value)
{
    auto *label = m_metricLabels.value(key, nullptr);
    if (!label)
        return;

    QString color = (value == "--") ? kValueColorNoData : kValueColorNormal;
    if (value != "--" && (key == QStringLiteral("currentPath")
                          || key == QStringLiteral("cutEmptyPath"))) {
        color = QString::fromLatin1(Theme::kRealtime);
    }
    label->setText(value);
    label->setStyleSheet(QString("color:%1;font-size:22px;font-weight:600;border:none;background:transparent;")
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

void RunDataPanel::setCutEmptyPathCounts(int cutCount, int emptyCount)
{
    if (cutCount < 0 && emptyCount < 0) {
        setMetricValue(QStringLiteral("cutEmptyPath"), QStringLiteral("--"));
        return;
    }
    setMetricValue(QStringLiteral("cutEmptyPath"),
                   QStringLiteral("%1/%2")
                       .arg(std::max(0, cutCount))
                       .arg(std::max(0, emptyCount)));
}

void RunDataPanel::clearMetrics()
{
    const QStringList keys = {
        "particleHeight", "taskId", "completedPath", "currentPath",
        "particleGrade", "cutEmptyPath"
    };
    for (const auto &k : keys) {
        setMetricValue(k, QStringLiteral("--"));
    }
}

void RunDataPanel::setMillingTask(quint64 taskId, int pathTotal)
{
    setMetricValue(QStringLiteral("taskId"),
                   taskId == 0 ? QStringLiteral("--") : QString::number(taskId));
    if (m_pathTotalLabel) {
        m_pathTotalLabel->setText(QStringLiteral("路径总数 %1 条").arg(pathTotal));
    }
    applyStatus(TaskStatus::Running);
}

void RunDataPanel::setParticleBoardGrade(const QString &grade)
{
    const QString text = grade.trimmed();
    setMetricValue(QStringLiteral("particleGrade"),
                   text.isEmpty() ? QStringLiteral("--") : text);
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
    setMetricValue(QStringLiteral("taskId"), QStringLiteral("--"));
    if (m_pathTotalLabel) {
        m_pathTotalLabel->setText(QStringLiteral("路径总数 -- 条"));
    }
    setParticleBoardGrade(QString());
    setCutEmptyPathCounts(-1, -1);
    applyStatus(TaskStatus::Idle);
}

QWidget *RunDataPanel::createTaskHeader(QWidget *parent)
{
    auto *header = new QFrame(parent);
    header->setObjectName("taskHeaderCard");
    header->setMinimumHeight(44);
    header->setStyleSheet(headerCardStyle());

    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(10, 0, 12, 0);
    layout->setSpacing(10);

    m_pathTotalLabel = makeLabel(QStringLiteral("路径总数 -- 条"), "pathTotal");
    m_pathTotalLabel->setStyleSheet(QString(
        "color:%1;font-size:14px;font-weight:600;border:none;background:transparent;")
        .arg(Theme::kTextPrimary));
    layout->addWidget(m_pathTotalLabel);

    layout->addStretch();

    m_statusDot = makeLabel(QStringLiteral("–"), "statusDot");
    m_statusDot->setStyleSheet(
        QString("color:%1;font-size:14px;border:none;background:transparent;")
            .arg(kValueColorNoData));
    layout->addWidget(m_statusDot);

    m_statusText = makeLabel(QStringLiteral("待命"), "statusText");
    m_statusText->setStyleSheet(QString(
        "color:%1;font-size:14px;border:none;background:transparent;")
        .arg(Theme::kStateUnknown));
    layout->addWidget(m_statusText);

    return header;
}

QWidget *RunDataPanel::createMetricCard(const QString &key,
                                        const QString &title,
                                        const QString &value,
                                        const QString &unit,
                                        const QIcon &icon,
                                        QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("metricCard");
    card->setMinimumHeight(56);
    card->setStyleSheet(cardStyle());

    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(12, 4, 14, 4);
    cardLayout->setSpacing(12);

    auto *iconLabel = new QLabel(card);
    iconLabel->setFixedSize(20, 20);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(icon.pixmap(16, 16));
    iconLabel->setStyleSheet(QStringLiteral("border:none;background:transparent;"));
    cardLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);

    auto *titleLabel = makeLabel(title, "metricName");
    titleLabel->setStyleSheet(QString(
        "color:%1;font-size:14px;border:none;background:transparent;")
        .arg(Theme::kTextSecondary));
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    cardLayout->addWidget(titleLabel);

    cardLayout->addStretch();

    auto *valueLabel = makeLabel(value, "metricValue");
    valueLabel->setStyleSheet(QString("color:%1;font-size:22px;font-weight:600;border:none;background:transparent;")
                                  .arg(kValueColorNoData));
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardLayout->addWidget(valueLabel);

    if (!unit.isEmpty()) {
        auto *unitLabel = makeLabel(unit);
        unitLabel->setStyleSheet(QString(
            "color:%1;font-size:12px;border:none;background:transparent;")
            .arg(Theme::kTextMuted));
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
    QString glyph;   // 状态符号（第二视觉线索，色盲也能区分）
    switch (status) {
    case TaskStatus::Idle:
        dotColor = kValueColorNoData;
        text = QStringLiteral("待命");
        glyph = QStringLiteral("–");
        break;
    case TaskStatus::Running:
        dotColor = Theme::kStateRunning;
        text = QStringLiteral("进行中");
        glyph = QStringLiteral("⟳");
        break;
    case TaskStatus::Success:
        dotColor = Theme::kStateSuccess;
        text = QStringLiteral("已完成");
        glyph = QStringLiteral("✓");
        break;
    case TaskStatus::Failed:
        dotColor = Theme::kStateError;
        text = QStringLiteral("失败");
        glyph = QStringLiteral("✕");
        break;
    }

    m_statusDot->setText(glyph);
    m_statusColor = dotColor;
    m_statusDot->setStyleSheet(
        QString("color:%1;font-size:14px;border:none;background:transparent;")
            .arg(dotColor));
    m_statusText->setText(text);
    m_statusText->setStyleSheet(
        QString("color:%1;font-size:14px;border:none;background:transparent;")
            .arg(m_statusColor));

    // 任务结束（完成/失败）是打磨流程的关键节点，给状态文字一次短暂的放大脉冲，
    // 帮操作员用余光捕捉结果变化（Peak-End Rule，克制使用，不全程动画）。
    if (status == TaskStatus::Success || status == TaskStatus::Failed) {
        pulseStatusText();
    }
}

void RunDataPanel::pulseStatusText()
{
    if (!m_statusText) {
        return;
    }

    if (!m_pulseTimer) {
        m_pulseTimer = new QTimer(this);
        m_pulseTimer->setInterval(80);
        connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
            // 步进：14 -> 18 -> 16 -> 14，一次跳动后复位。
            const int sizes[] = {18, 16, 14};
            if (m_pulseStep >= 3) {
                m_pulseTimer->stop();
                m_pulseStep = 0;
                return;
            }
            const int size = sizes[m_pulseStep];
            m_statusText->setStyleSheet(
                QString("color:%1;font-size:%2px;border:none;background:transparent;")
                    .arg(m_statusColor)
                    .arg(size));
            ++m_pulseStep;
        });
    } else {
        m_pulseTimer->stop();
    }
    m_pulseStep = 0;
    m_pulseTimer->start();
}
