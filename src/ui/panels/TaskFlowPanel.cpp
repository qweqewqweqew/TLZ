#include "ui/panels/TaskFlowPanel.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString cardStyle(bool active)
{
    return QStringLiteral(R"(
        QFrame#commandCard {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(active ? Theme::kSelectionBg : Theme::kControlBg,
            active ? Theme::kAccent : Theme::kControlBg);
}

QString fieldText(const QString &name, const QString &value)
{
    return QString("%1  %2").arg(name, value);
}

QString formatFloat(float value, int precision = 2)
{
    return QString::number(double(value), 'f', precision);
}

QLabel *createFieldLabel(const QString &text, QWidget *parent, bool highlight = false)
{
    auto *label = makeLabel(text, "commandField");
    label->setParent(parent);
    label->setStyleSheet(QString(
        "color:%1;font-size:12px;border:none;background:transparent;")
        .arg(highlight ? Theme::kRealtime : Theme::kTextSecondary));
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
    m_scrollArea->setStyleSheet(QString(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:vertical{background:%1;width:8px;margin:0;border:none;}"
        "QScrollBar::handle:vertical{background:%2;border-radius:4px;min-height:24px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;background:transparent;}")
        .arg(Theme::kWindowBg, Theme::kBorder));

    auto *content = new QWidget(m_scrollArea);
    content->setStyleSheet("background:transparent;");
    m_flowLayout = new QVBoxLayout(content);
    m_flowLayout->setContentsMargins(2, 2, 2, 2);
    m_flowLayout->setSpacing(8);
    m_flowLayout->setAlignment(Qt::AlignTop);

    m_scrollArea->setWidget(content);

    m_stickyHost = new QWidget(panel);
    m_stickyHost->setVisible(false);
    auto *stickyWrap = new QVBoxLayout(m_stickyHost);
    stickyWrap->setContentsMargins(2, 2, 2, 6);
    stickyWrap->setSpacing(4);

    auto *caption = makeLabel(QStringLiteral("当前执行"), "stickyCurrentCaption");
    caption->setStyleSheet(QString(
        "color:%1;font-size:12px;font-weight:600;border:none;background:transparent;")
        .arg(Theme::kAccent));
    stickyWrap->addWidget(caption);

    m_stickyLayout = new QVBoxLayout();
    m_stickyLayout->setContentsMargins(0, 0, 0, 0);
    m_stickyLayout->setSpacing(0);
    stickyWrap->addLayout(m_stickyLayout);
    panelLayout->addWidget(m_stickyHost, 0);
    panelLayout->addWidget(m_scrollArea, 1);

    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) { onScrollValueChanged(); });
}

void TaskFlowPanel::setMillingPaths(const QVector<MillingPathVM> &paths)
{
    const int activeIndex = m_activePathIndex;
    const bool browseMode = m_browseMode;
    clearCommands();
    m_activePathIndex = activeIndex;
    m_browseMode = browseMode;
    m_paths = paths;
    revealExecutedPaths();
    refreshActivePathStyles();
    updateStickyCurrentCard();
    if (m_browseMode) {
        return;
    }
    QTimer::singleShot(0, this, [this]() { scrollToActivePath(); });
}

void TaskFlowPanel::setActivePath(int currentPath)
{
    const int newIndex = currentPath > 0 ? currentPath - 1 : -1;
    if (newIndex == m_activePathIndex) {
        return;
    }
    m_activePathIndex = newIndex;
    revealExecutedPaths();
    refreshActivePathStyles();
    updateStickyCurrentCard();
    if (m_browseMode) {
        return;
    }
    QTimer::singleShot(0, this, [this]() { scrollToActivePath(); });
}

void TaskFlowPanel::selectPath(int pathIndex)
{
    if (pathIndex < 0 || pathIndex >= m_paths.size()) {
        return;
    }
    m_selectedPathIndex = pathIndex;
    revealPathsUpTo(qMax(m_activePathIndex, m_selectedPathIndex));
    refreshActivePathStyles();
    QTimer::singleShot(0, this, [this, pathIndex]() { scrollToPath(pathIndex); });
}

void TaskFlowPanel::clearCommands()
{
    if (!m_flowLayout) {
        return;
    }

    m_browseMode = false;
    updateStickyCurrentCard();

    while (m_flowLayout->count() > 0) {
        auto *item = m_flowLayout->takeAt(0);
        if (item) {
            delete item->widget();
            delete item;
        }
    }
    m_pathCards.clear();
    m_paths.clear();
    m_activePathIndex = -1;
    m_selectedPathIndex = -1;
}

void TaskFlowPanel::revealExecutedPaths()
{
    revealPathsUpTo(m_activePathIndex);
}

void TaskFlowPanel::revealPathsUpTo(int lastIndex)
{
    if (!m_flowLayout || m_paths.isEmpty() || lastIndex < 0) {
        return;
    }

    QWidget *parent = m_flowLayout->parentWidget();
    const int targetCount = qMin(lastIndex + 1, m_paths.size());
    if (m_pathCards.size() >= targetCount) {
        return;
    }

    QScrollBar *bar = m_scrollArea ? m_scrollArea->verticalScrollBar() : nullptr;
    const int oldValue = bar ? bar->value() : 0;
    const int oldMax = bar ? bar->maximum() : 0;
    const bool preserveBrowseScroll = m_browseMode && bar;

    m_programmaticScroll = true;
    m_pathCards.reserve(targetCount);
    while (m_pathCards.size() < targetCount) {
        const int index = m_pathCards.size();
        auto *card = createPathCard(m_paths.at(index), index + 1, parent);
        m_flowLayout->insertWidget(0, card);
        m_pathCards.push_back(card);
    }
    m_programmaticScroll = false;

    if (preserveBrowseScroll && bar) {
        QTimer::singleShot(0, this, [this, oldValue, oldMax]() {
            if (!m_scrollArea || !m_browseMode) {
                return;
            }
            QScrollBar *bar = m_scrollArea->verticalScrollBar();
            m_programmaticScroll = true;
            bar->setValue(oldValue + qMax(0, bar->maximum() - oldMax));
            m_programmaticScroll = false;
        });
    }
}

void TaskFlowPanel::refreshActivePathStyles()
{
    for (int i = 0; i < m_pathCards.size(); ++i) {
        if (auto *card = qobject_cast<QFrame *>(m_pathCards.at(i))) {
            const bool highlighted = (i == m_activePathIndex) || (i == m_selectedPathIndex);
            card->setStyleSheet(cardStyle(highlighted));
        }
    }
}

void TaskFlowPanel::scrollToActivePath()
{
    scrollToPath(m_activePathIndex);
}

void TaskFlowPanel::scrollToPath(int pathIndex)
{
    if (!m_scrollArea) {
        return;
    }
    if (pathIndex < 0 || pathIndex >= m_pathCards.size()) {
        return;
    }
    QWidget *card = m_pathCards.at(pathIndex);
    if (!card) {
        return;
    }
    m_programmaticScroll = true;
    m_scrollArea->ensureWidgetVisible(card, 0, 16);
    m_programmaticScroll = false;
}

void TaskFlowPanel::updateStickyCurrentCard()
{
    const bool showSticky = m_browseMode
        && m_stickyHost
        && m_stickyLayout
        && m_activePathIndex >= 0
        && m_activePathIndex < m_paths.size();
    if (!showSticky) {
        if (m_stickyCard) {
            m_stickyLayout->removeWidget(m_stickyCard);
            delete m_stickyCard;
            m_stickyCard = nullptr;
        }
        m_stickyPathIndex = -1;
        if (m_stickyHost && m_stickyHost->isVisible()) {
            m_programmaticScroll = true;
            m_stickyHost->setVisible(false);
            m_programmaticScroll = false;
        }
        return;
    }

    if (m_stickyCard && m_stickyPathIndex == m_activePathIndex && m_stickyHost->isVisible()) {
        return;
    }

    if (m_stickyCard) {
        m_stickyLayout->removeWidget(m_stickyCard);
        delete m_stickyCard;
        m_stickyCard = nullptr;
    }

    m_programmaticScroll = true;
    m_stickyCard = createPathCard(
        m_paths.at(m_activePathIndex), m_activePathIndex + 1, m_stickyHost);
    if (auto *frame = qobject_cast<QFrame *>(m_stickyCard)) {
        frame->setStyleSheet(cardStyle(true));
    }
    m_stickyLayout->addWidget(m_stickyCard);
    m_stickyPathIndex = m_activePathIndex;
    m_stickyHost->setVisible(true);
    m_programmaticScroll = false;
}

void TaskFlowPanel::onScrollValueChanged()
{
    if (m_programmaticScroll || m_pathCards.isEmpty() || m_activePathIndex < 0) {
        return;
    }

    const bool away = isScrolledAwayFromCurrent();
    if (away == m_browseMode) {
        return;
    }
    m_browseMode = away;
    updateStickyCurrentCard();
}

bool TaskFlowPanel::isScrolledAwayFromCurrent() const
{
    if (!m_scrollArea) {
        return false;
    }
    QScrollBar *bar = m_scrollArea->verticalScrollBar();
    if (!bar) {
        return false;
    }
    // Newest/current card sits at the top. Only pin when the user has
    // actually scrolled down; ignore tiny layout jitter.
    return bar->value() > 12;
}

QWidget *TaskFlowPanel::createPathCard(const MillingPathVM &path,
                                       int sequence,
                                       QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("commandCard");
    card->setStyleSheet(cardStyle(false));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);

    auto *sequenceBadge = makeLabel(QString::number(sequence), "commandSequence");
    sequenceBadge->setFixedSize(32, 32);
    sequenceBadge->setAlignment(Qt::AlignCenter);
    sequenceBadge->setToolTip(QStringLiteral("第 %1 条路径").arg(sequence));
    sequenceBadge->setStyleSheet(QString(
        "color:%1;font-size:14px;font-weight:700;border:none;border-radius:16px;"
        "background:%2;").arg(Theme::kTextPrimary, Theme::kAccent));
    header->addWidget(sequenceBadge);

    const QString kind = path.doCut ? QStringLiteral("切削路径") : QStringLiteral("空移路径");
    auto *title = makeLabel(kind, "commandTitle");
    title->setStyleSheet(QString(
        "color:%1;font-size:14px;font-weight:600;border:none;background:transparent;")
        .arg(Theme::kTextPrimary));
    header->addWidget(title, 1);

    auto *kindBadge = makeLabel(path.doCut ? QStringLiteral("切削") : QStringLiteral("空移"),
                                "commandDirection");
    kindBadge->setStyleSheet(QString(
        "color:%1;font-size:12px;font-weight:600;border:none;background:transparent;")
        .arg(path.doCut ? Theme::kStateRunning : Theme::kTextSecondary));
    header->addWidget(kindBadge);
    layout->addLayout(header);

    auto *coordRow = new QHBoxLayout();
    coordRow->setContentsMargins(0, 2, 0, 0);
    coordRow->setSpacing(8);

    auto *startCoord = createFieldLabel(
        fieldText(QStringLiteral("起点"),
                  QStringLiteral("(%1, %2)").arg(path.uStart).arg(path.vStart)),
        card);
    startCoord->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    startCoord->setWordWrap(false);

    auto *arrow = makeLabel(QStringLiteral("→"), "commandCoordArrow");
    arrow->setParent(card);
    arrow->setAlignment(Qt::AlignCenter);
    arrow->setStyleSheet(QString(
        "color:%1;font-size:12px;border:none;background:transparent;")
        .arg(Theme::kTextMuted));

    auto *endCoord = createFieldLabel(
        fieldText(QStringLiteral("终点"),
                  QStringLiteral("(%1, %2)").arg(path.uEnd).arg(path.vEnd)),
        card);
    endCoord->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    endCoord->setWordWrap(false);

    coordRow->addWidget(startCoord, 1);
    coordRow->addWidget(arrow, 0);
    coordRow->addWidget(endCoord, 1);
    layout->addLayout(coordRow);

    auto *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(0);

    grid->addWidget(createFieldLabel(fieldText(QStringLiteral("X 速"), formatFloat(path.xSpeed, 1)), card, true), 0, 0);
    grid->addWidget(createFieldLabel(fieldText(QStringLiteral("Y 速"), formatFloat(path.ySpeed, 1)), card), 0, 1);

    for (int column = 0; column < 2; ++column) {
        grid->setColumnStretch(column, 1);
    }
    layout->addLayout(grid);

    return card;
}
