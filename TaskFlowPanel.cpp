#include "TaskFlowPanel.h"

#include "PlcPathCommandParamsVM.h"
#include "UiHelpers.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int kMaxCommandCards = 50;

QString cardStyle()
{
    return QStringLiteral(R"(
        QFrame#commandCard {
            background: #1B2632;
            border: 1px solid #2E3D4D;
            border-radius: 6px;
        }
    )");
}

QString fieldText(const QString &name, const QString &value)
{
    return QString("%1  %2").arg(name, value);
}

QString formatFloat(float value, int precision = 2)
{
    return QString::number(value, 'f', precision);
}

QString feedDirectionText(qint16 direction)
{
    switch (direction) {
    case 10: return QStringLiteral("X 轴正向");
    case 20: return QStringLiteral("X 轴负向");
    case 30: return QStringLiteral("Y 轴正向");
    case 40: return QStringLiteral("Y 轴负向");
    default: return QStringLiteral("未定义 (%1)").arg(direction);
    }
}

QLabel *createFieldLabel(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, "commandField");
    label->setParent(parent);
    label->setStyleSheet(
        "color:#B8C7D4;font-size:13px;border:none;background:transparent;");
    label->setMinimumHeight(22);
    return label;
}

} // namespace

TaskFlowPanel::TaskFlowPanel(QWidget *parent)
    : QFrame(parent)
{
    auto *panel = createPanel(QStringLiteral("任务流程"), nullptr, this);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    panelLayout->setSpacing(8);

    m_scrollArea = new QScrollArea(panel);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:vertical{background:#121A23;width:8px;margin:0;border:none;}"
        "QScrollBar::handle:vertical{background:#2E3D4D;border-radius:4px;min-height:24px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;background:transparent;}");

    auto *content = new QWidget(m_scrollArea);
    content->setStyleSheet("background:transparent;");
    m_flowLayout = new QVBoxLayout(content);
    m_flowLayout->setContentsMargins(2, 2, 2, 2);
    m_flowLayout->setSpacing(8);
    m_flowLayout->setAlignment(Qt::AlignTop);

    m_scrollArea->setWidget(content);
    panelLayout->addWidget(m_scrollArea, 1);
}

void TaskFlowPanel::addPlcCommand(const PlcPathCommandParamsVM &command)
{
    if (!m_flowLayout) {
        return;
    }

    auto *card = createCommandCard(command, m_nextSequence++, m_flowLayout->parentWidget());
    m_flowLayout->insertWidget(0, card);

    while (m_flowLayout->count() > kMaxCommandCards) {
        auto *item = m_flowLayout->takeAt(m_flowLayout->count() - 1);
        if (item) {
            delete item->widget();
            delete item;
        }
    }
}

void TaskFlowPanel::clearCommands()
{
    if (!m_flowLayout) {
        return;
    }

    while (m_flowLayout->count() > 0) {
        auto *item = m_flowLayout->takeAt(0);
        if (item) {
            delete item->widget();
            delete item;
        }
    }
    m_nextSequence = 1;
}

QWidget *TaskFlowPanel::createCommandCard(const PlcPathCommandParamsVM &command,
                                          int sequence,
                                          QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("commandCard");
    card->setStyleSheet(cardStyle());

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);

    auto *sequenceBadge = makeLabel(QString::number(sequence), "commandSequence");
    sequenceBadge->setFixedSize(32, 32);
    sequenceBadge->setAlignment(Qt::AlignCenter);
    sequenceBadge->setToolTip(QStringLiteral("第 %1 条 PLC 指令").arg(sequence));
    sequenceBadge->setStyleSheet(
        "color:#E6EEF5;font-size:16px;font-weight:700;border:none;border-radius:16px;"
        "background:#2D6CDF;");
    header->addWidget(sequenceBadge);

    auto *titleGroup = new QVBoxLayout();
    titleGroup->setContentsMargins(0, 0, 0, 0);
    titleGroup->setSpacing(0);

    auto *title = makeLabel(QStringLiteral("PLC 工艺指令"), "commandTitle");
    title->setStyleSheet(
        "color:#E6EEF5;font-size:14px;font-weight:600;border:none;background:transparent;");
    titleGroup->addWidget(title);

    auto *subtitle = makeLabel(QStringLiteral("坐标定位 · 进给参数"), "commandSubtitle");
    subtitle->setStyleSheet(
        "color:#718395;font-size:11px;border:none;background:transparent;");
    titleGroup->addWidget(subtitle);
    header->addLayout(titleGroup);
    header->addStretch();

    auto *direction = makeLabel(
        QStringLiteral("进给方向：%1").arg(feedDirectionText(command.feedDirection)),
        "commandDirection");
    direction->setStyleSheet(
        "color:#00E5FF;font-size:12px;font-weight:600;border:none;background:transparent;");
    header->addWidget(direction);
    layout->addLayout(header);

    auto *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(0);

    grid->addWidget(createFieldLabel(fieldText("X", formatFloat(command.x)), card), 0, 0);
    grid->addWidget(createFieldLabel(fieldText("Y", formatFloat(command.y)), card), 0, 1);
    grid->addWidget(createFieldLabel(fieldText("Z", formatFloat(command.z)), card), 0, 2);
    grid->addWidget(createFieldLabel(fieldText("进给速度", formatFloat(command.feedSpeed)), card), 0, 3);
    grid->addWidget(createFieldLabel(fieldText("进给量", formatFloat(command.feedAmount)), card), 1, 0);
    grid->addWidget(createFieldLabel(fieldText("下刀次数", QString::number(command.plungeCount)), card), 1, 1);
    grid->addWidget(createFieldLabel(fieldText("下刀量", QString::number(command.plungeAmount)), card), 1, 2);

    for (int column = 0; column < 4; ++column) {
        grid->setColumnStretch(column, 1);
    }
    layout->addLayout(grid);

    return card;
}
