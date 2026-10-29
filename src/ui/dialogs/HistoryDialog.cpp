#include "ui/dialogs/HistoryDialog.h"

#include "app/AppStyle.h"
#include "ElaComboBox.h"
#include "ElaPushButton.h"
#include "data/InspectionRepository.h"
#include "app/UiHelpers.h"
#include "app/Theme.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTextStream>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

// 头文件里 forward declare 的汇总结构体：只在本 .cpp 内使用
struct HistorySummary
{
    int    taskCount = 0;
    qint64 totalParticles = 0;
    int    particleBoards = 0;
    int    completedBoards = 0;
    int    pendingBoards = 0;
};

namespace {

// 铜板编号 / 检测时间 / 面 / 粒子数 / 最大高度 / 已修磨 / 结果 / 操作
constexpr int kColumnCount = 8;
constexpr int kPageSize = 8;
constexpr int kMainTableColumnWeights[kColumnCount] = { 16, 16, 6, 8, 12, 8, 10, 8 };
constexpr int kMainTableColumnWeightTotal = 84;

QColor resultColor(TaskStatus status)
{
    switch (status) {
    case TaskStatus::MillCompleted:
    case TaskStatus::ReviewCompleted:
    case TaskStatus::ManualPassed:
        return QColor(Theme::kStateSuccess);
    case TaskStatus::Abnormal:
        return QColor(Theme::kStateError);
    case TaskStatus::Unset:
        break;
    }
    return QColor(Theme::kTextMuted);
}

const char *resultBackground(TaskStatus status)
{
    switch (status) {
    case TaskStatus::MillCompleted:
    case TaskStatus::ReviewCompleted:
    case TaskStatus::ManualPassed:
        return Theme::kSuccessBg;
    case TaskStatus::Abnormal:
        return Theme::kErrorBg;
    case TaskStatus::Unset:
        break;
    }
    return Theme::kControlBg;
}

QIcon downloadIcon()
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(Theme::kTextPrimary), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.drawLine(QPointF(8, 2), QPointF(8, 10));
    painter.drawLine(QPointF(4.5, 7), QPointF(8, 10.5));
    painter.drawLine(QPointF(11.5, 7), QPointF(8, 10.5));
    painter.drawLine(QPointF(3, 13.5), QPointF(13, 13.5));
    painter.end();
    return QIcon(pixmap);
}

QWidget *makeResultPill(TaskStatus status, QWidget *parent)
{
    auto *host = new QWidget(parent);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->addStretch(1);
    auto *pill = new QLabel(taskStatusText(status), host);
    pill->setAlignment(Qt::AlignCenter);
    const QColor color = resultColor(status);
    pill->setStyleSheet(QStringLiteral(
        "color:%1;background:%2;border:1px solid %1;border-radius:4px;"
        "font-size:12px;font-weight:600;padding:1px 8px;")
                            .arg(color.name(), QLatin1String(resultBackground(status))));
    pill->setFixedHeight(22);
    layout->addWidget(pill);
    layout->addStretch(1);
    return host;
}

QString plateNumber(const InspectionRecordRow &row)
{
    if (row.backendTaskId != 0) {
        return QString::number(row.backendTaskId);
    }
    return QString::number(row.id);
}

QString durationSeconds(const QDateTime &from, const QDateTime &to)
{
    if (!from.isValid() || !to.isValid()) {
        return QStringLiteral("--");
    }
    const qint64 seconds = qAbs(from.secsTo(to));
    return QStringLiteral("%1 s").arg(seconds);
}

QString formatDateTime(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm:ss") : QStringLiteral("--");
}

QString historyDialogExtraStyle()
{
    // Map former hard-coded palette onto Theme tokens so history page matches the shell.
    QString css = QStringLiteral(
        "QDialog { background: __PANEL__; }"
        "#historyContent { background: __PANEL__; }"
        "QTableWidget {"
        "  background: __CTRL__; color: __TEXT__; gridline-color: __BORDER__;"
        "  border: 1px solid __BORDER__; border-radius: 6px;"
        "  alternate-background-color: __HOVER__;"
        "  selection-background-color: __SEL__; selection-color: __SELTEXT__;"
        "}"
        "QTableWidget::item { padding: 6px 8px; }"
        "QTableWidget::item:hover { background: __HOVER__; }"
        "QTableWidget::item:selected { background: __SEL__; color: __SELTEXT__; }"
        "QTableWidget::item:selected:hover { background: __SEL_HOVER__; }"
        "QSplitter::handle { background: __HOVER__; }"
        "QSplitter::handle:hover { background: __ACCENT__; }"
        "QScrollArea { background: __WIN__; border: none; }"
        "QHeaderView::section {"
        "  background: __WIN__; color: __TEXT__; padding: 6px 10px; border: none;"
        "  border-right: 1px solid __BORDER__; border-bottom: 1px solid __BORDER__; font-weight: 600;"
        "}"
        "QTableCornerButton::section { background: __WIN__; border: 1px solid __BORDER__; }"
        "QTabWidget::pane {"
        "  border: 1px solid __BORDER__; border-radius: 6px; background: __CTRL__; top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background: __WIN__; color: __TEXT2__; padding: 6px 16px;"
        "  border: 1px solid __BORDER__; border-bottom: none;"
        "  border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px;"
        "}"
        "QTabBar::tab:selected { background: __SEL__; color: __SELTEXT__; border-color: __FOCUS__; }"
        "QTabBar::tab:selected:hover { background: __SEL_HOVER__; }"
        "QTabBar::tab:selected:pressed { background: __SEL_PRESSED__; }"
        "QDateEdit, QLineEdit {"
        "  background: __WIN__; color: __TEXT__; border: 1px solid __BORDER__;"
        "  border-radius: 5px; padding: 2px 8px; min-height: 26px;"
        "}"
        "QDateEdit:disabled { color: __MUTED__; background: __HOVER__; border: 1px solid __HOVER__; }"
        "QDateEdit::drop-down {"
        "  subcontrol-origin: padding; subcontrol-position: right; width: 18px;"
        "  border-left: 1px solid __BORDER__;"
        "}"
        "QCalendarWidget QWidget { alternate-background-color: __HOVER__; color: __TEXT__; }"
        "QCalendarWidget QAbstractItemView:enabled {"
        "  background: __PANEL__; color: __TEXT__;"
        "  selection-background-color: __SEL__; selection-color: __SELTEXT__;"
        "}"
        "QScrollBar:vertical { background: __WIN__; width: 10px; margin: 0; border: none; }"
        "QScrollBar::handle:vertical { background: __BORDER__; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: __HOVER__; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  background: none; border: none; height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QScrollBar:horizontal { background: __WIN__; height: 10px; margin: 0; border: none; }"
        "QScrollBar::handle:horizontal { background: __BORDER__; border-radius: 4px; min-width: 30px; }"
        "QScrollBar::handle:horizontal:hover { background: __HOVER__; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  background: none; border: none; width: 0;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }"
    );
    css.replace(QStringLiteral("__PANEL__"), QLatin1String(Theme::kPanelBg));
    css.replace(QStringLiteral("__CTRL__"), QLatin1String(Theme::kControlBg));
    css.replace(QStringLiteral("__TEXT__"), QLatin1String(Theme::kTextPrimary));
    css.replace(QStringLiteral("__BORDER__"), QLatin1String(Theme::kBorder));
    css.replace(QStringLiteral("__HOVER__"), QLatin1String(Theme::kHoverBg));
    css.replace(QStringLiteral("__SEL__"), QLatin1String(Theme::kSelectionBg));
    css.replace(QStringLiteral("__SEL_HOVER__"), QLatin1String(Theme::kSelectedHoverBg));
    css.replace(QStringLiteral("__SEL_PRESSED__"), QLatin1String(Theme::kSelectedPressedBg));
    css.replace(QStringLiteral("__SELTEXT__"), QLatin1String(Theme::kSelectionText));
    css.replace(QStringLiteral("__FOCUS__"), QLatin1String(Theme::kFocusRing));
    css.replace(QStringLiteral("__ACCENT__"), QLatin1String(Theme::kAccent));
    css.replace(QStringLiteral("__WIN__"), QLatin1String(Theme::kWindowBg));
    css.replace(QStringLiteral("__TEXT2__"), QLatin1String(Theme::kTextSecondary));
    css.replace(QStringLiteral("__MUTED__"), QLatin1String(Theme::kTextMuted));
    return css;
}


void setupCompactTable(QTableWidget *table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setFrameShape(QFrame::NoFrame);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setHighlightSections(false);
    // 水平方向靠列宽/stretch 自动铺满，不需要滚动条；
    // 垂直方向仅在装不下记录时出现。
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 关键：默认是 ScrollPerItem（一行一格），平滑动画会被"贴"到整行边界，
    // 视觉上就是"一格一格跳"。切成像素级滚动才有真正连续的平滑效果。
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    // 让滚动条 setValue 时不再触发每次 item 边界对齐
    if (auto *vbar = table->verticalScrollBar()) {
        vbar->setSingleStep(8);   // 键盘/箭头一次滚 8px
    }
}

// 详情表占满 Tab 页面，记录超出可视区时使用表格自身滚动。
void setupDetailTable(QTableWidget *table)
{
    setupCompactTable(table);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

// 把表格里所有单元格文本居中对齐（连同表头）
void centerTableCells(QTableWidget *table)
{
    for (int r = 0; r < table->rowCount(); ++r) {
        for (int c = 0; c < table->columnCount(); ++c) {
            if (auto *it = table->item(r, c)) {
                it->setTextAlignment(Qt::AlignCenter);
            }
        }
    }
    for (int c = 0; c < table->columnCount(); ++c) {
        if (auto *hdr = table->horizontalHeaderItem(c)) {
            hdr->setTextAlignment(Qt::AlignCenter);
        }
    }
}

} // namespace

// -----------------------------------------------------------------
// 构造 + 顶层布局
// -----------------------------------------------------------------

HistoryDialog::HistoryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Widget);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setStyleSheet(mainWindowStyleSheet() + historyDialogExtraStyle());

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *content = new QWidget(this);
    content->setObjectName("historyContent");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 14, 16, 16);
    contentLayout->setSpacing(10);

    contentLayout->addWidget(buildFilterBar());
    contentLayout->addWidget(buildSummaryRow());

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("sectionHint");
    contentLayout->addWidget(m_statusLabel);

    contentLayout->addWidget(buildTableCard(), 1);
    root->addWidget(content, 1);

    if (m_refreshButton) {
        connect(m_refreshButton, &ElaPushButton::clicked, this, [this]() {
            m_page = 0;
            reloadRecords();
        });
    }
    if (m_resetButton) {
        connect(m_resetButton, &ElaPushButton::clicked, this, [this]() {
            const QDate today = QDate::currentDate();
            if (m_from) {
                m_from->setDate(today.addDays(-14));
            }
            if (m_to) {
                m_to->setDate(today);
            }
            if (m_plateEdit) {
                m_plateEdit->clear();
            }
            if (m_statusCombo) {
                m_statusCombo->setCurrentIndex(0);
            }
            if (m_shiftCombo) {
                m_shiftCombo->setCurrentIndex(0);
            }
            m_page = 0;
            reloadRecords();
        });
    }
    if (m_exportButton) {
        connect(m_exportButton, &QPushButton::clicked, this, &HistoryDialog::exportCurrentList);
    }
    if (m_prevPage) {
        connect(m_prevPage, &ElaPushButton::clicked, this, [this]() {
            if (m_page > 0) {
                --m_page;
                renderPage();
            }
        });
    }
    if (m_nextPage) {
        connect(m_nextPage, &ElaPushButton::clicked, this, [this]() {
            const int pages = qMax(1, (m_filtered.size() + kPageSize - 1) / kPageSize);
            if (m_page + 1 < pages) {
                ++m_page;
                renderPage();
            }
        });
    }

    reloadRecords();
    Theme::watch(this, [this]() {
        setStyleSheet(mainWindowStyleSheet() + historyDialogExtraStyle());
        if (m_tileTaskCount) {
            m_tileTaskCount->setStyleSheet(QStringLiteral("color:%1;font-size:28px;font-weight:700;border:none;background:transparent;")
                                               .arg(Theme::kAccent));
        }
        if (m_tileParticles) {
            m_tileParticles->setStyleSheet(QStringLiteral("color:%1;font-size:28px;font-weight:700;border:none;background:transparent;")
                                               .arg(Theme::kStateWarning));
        }
        if (m_tileClearRate) {
            m_tileClearRate->setStyleSheet(QStringLiteral("color:%1;font-size:28px;font-weight:700;border:none;background:transparent;")
                                               .arg(Theme::kStateSuccess));
        }
        if (m_tilePending) {
            m_tilePending->setStyleSheet(QStringLiteral("color:%1;font-size:28px;font-weight:700;border:none;background:transparent;")
                                             .arg(Theme::kStateError));
        }
        renderPage();
        if (m_selectedRecordId > 0) {
            showDetailForRecord(m_selectedRecordId);
        }
    });
}

bool HistoryDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (m_table && watched == m_table->viewport() && event->type() == QEvent::Resize) {
        updateMainTableColumnWidths();
    }
    if (m_splitter && watched == m_splitter && event->type() == QEvent::Resize && !m_splitterSized) {
        const int width = m_splitter->width();
        if (width > 400) {
            m_splitterSized = true;
            const int detail = qBound(460, int(width * 0.42), 620);
            m_splitter->setSizes({qMax(360, width - detail - m_splitter->handleWidth()), detail});
            updateMainTableColumnWidths();
        }
    }

    return QDialog::eventFilter(watched, event);
}

void HistoryDialog::updateMainTableColumnWidths()
{
    if (!m_table || !m_table->viewport()) {
        return;
    }

    const int availableWidth = m_table->viewport()->width();
    if (availableWidth <= 0) {
        return;
    }

    int widths[kColumnCount] = {};
    int remainders[kColumnCount] = {};
    int assignedWidth = 0;
    for (int i = 0; i < kColumnCount; ++i) {
        const int weightedWidth = availableWidth * kMainTableColumnWeights[i];
        widths[i] = weightedWidth / kMainTableColumnWeightTotal;
        remainders[i] = weightedWidth % kMainTableColumnWeightTotal;
        assignedWidth += widths[i];
    }

    for (int extra = availableWidth - assignedWidth; extra > 0; --extra) {
        int bestIndex = 0;
        for (int i = 1; i < kColumnCount; ++i) {
            if (remainders[i] > remainders[bestIndex]) {
                bestIndex = i;
            }
        }
        ++widths[bestIndex];
        remainders[bestIndex] = -1;
    }

    for (int i = 0; i < kColumnCount; ++i) {
        m_table->setColumnWidth(i, widths[i]);
    }
}

// -----------------------------------------------------------------
// 子控件构建
// -----------------------------------------------------------------

QWidget *HistoryDialog::buildFilterBar()
{
    auto *bar = new QFrame(this);
    bar->setObjectName("controlBar");

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    const QDate today = QDate::currentDate();
    layout->addWidget(makeLabel(QStringLiteral("时间范围"), QStringLiteral("sectionHint")));
    m_from = new QDateEdit(today.addDays(-14), bar);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy/MM/dd"));
    m_from->setFixedWidth(128);
    layout->addWidget(m_from);
    layout->addWidget(makeLabel(QStringLiteral("至"), QStringLiteral("sectionHint")));
    m_to = new QDateEdit(today, bar);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy/MM/dd"));
    m_to->setFixedWidth(128);
    layout->addWidget(m_to);

    layout->addSpacing(8);
    layout->addWidget(makeLabel(QStringLiteral("铜板编号"), QStringLiteral("sectionHint")));
    m_plateEdit = new QLineEdit(bar);
    m_plateEdit->setPlaceholderText(QStringLiteral("输入铜板编号"));
    m_plateEdit->setClearButtonEnabled(true);
    m_plateEdit->setFixedWidth(160);
    layout->addWidget(m_plateEdit);

    layout->addWidget(makeLabel(QStringLiteral("班次"), QStringLiteral("sectionHint")));
    m_shiftCombo = new ElaComboBox(bar);
    m_shiftCombo->addItem(QStringLiteral("全部"));
    m_shiftCombo->setMinimumWidth(100);
    m_shiftCombo->setToolTip(QStringLiteral("后端尚未提供班次，目前只能查看全部"));
    layout->addWidget(m_shiftCombo);

    layout->addWidget(makeLabel(QStringLiteral("任务状态"), QStringLiteral("sectionHint")));
    m_statusCombo = new ElaComboBox(bar);
    m_statusCombo->addItems({QStringLiteral("全部"),
                             QStringLiteral("异常"),
                             QStringLiteral("修磨完成"),
                             QStringLiteral("复核完成"),
                             QStringLiteral("人工审核")});
    m_statusCombo->setMinimumWidth(120);
    layout->addWidget(m_statusCombo);

    layout->addStretch(1);
    m_refreshButton = new ElaPushButton(QStringLiteral("查询"), bar);
    m_refreshButton->setFixedSize(88, 32);
    applyPrimaryButtonStyle(m_refreshButton);
    layout->addWidget(m_refreshButton);
    m_resetButton = new ElaPushButton(QStringLiteral("重置"), bar);
    m_resetButton->setFixedSize(88, 32);
    applySecondaryButtonStyle(m_resetButton);
    layout->addWidget(m_resetButton);
    return bar;
}

QWidget *HistoryDialog::buildSummaryRow()
{
    auto *host = new QWidget(this);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addWidget(makeTile(QStringLiteral("当前范围铜板数"), QStringLiteral("张"),
                               QString::fromLatin1(Theme::kAccent), m_tileTaskCount));
    layout->addWidget(makeTile(QStringLiteral("检测粒子"), QStringLiteral("个"),
                               QString::fromLatin1(Theme::kStateWarning), m_tileParticles));
    layout->addWidget(makeTile(QStringLiteral("修磨完成率"), QStringLiteral("%"),
                               QString::fromLatin1(Theme::kStateSuccess), m_tileClearRate));
    layout->addWidget(makeTile(QStringLiteral("异常"), QStringLiteral("张"),
                               QString::fromLatin1(Theme::kStateError), m_tilePending));

    return host;
}

QWidget *HistoryDialog::buildTableCard()
{
    auto *host = new QWidget(this);
    auto *hostLayout = new QHBoxLayout(host);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, host);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(8);

    m_exportButton = new QPushButton(QStringLiteral("导出当前列表"), this);
    m_exportButton->setIcon(downloadIcon());
    m_exportButton->setIconSize(QSize(14, 14));
    m_exportButton->setCursor(Qt::PointingHandCursor);
    m_exportButton->setFixedHeight(32);
    m_exportButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  color:%1; background:%2; border:1px solid %3; border-radius:6px;"
        "  padding:0 14px; font-size:13px;"
        "}"
        "QPushButton:hover { background:%4; }"
        "QPushButton:pressed { background:%5; }")
                                      .arg(Theme::kTextPrimary,
                                           Theme::kPanelBg,
                                           Theme::kBorder,
                                           Theme::kHoverBg,
                                           Theme::kPressedBg));
    auto *listPanel = createPanel(QStringLiteral("检测修磨记录"), m_exportButton, m_splitter);
    auto *listLayout = qobject_cast<QVBoxLayout *>(listPanel->layout());

    m_tableStack = new QStackedWidget(listPanel);

    m_table = new QTableWidget(m_tableStack);
    m_table->setColumnCount(kColumnCount);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("铜板编号"),
        QStringLiteral("检测时间"),
        QStringLiteral("面"),
        QStringLiteral("粒子数"),
        QStringLiteral("最大高度(mm)"),
        QStringLiteral("已修磨"),
        QStringLiteral("结果"),
        QStringLiteral("操作")
    });
    setupCompactTable(m_table);
    m_table->setMouseTracking(true);
    m_table->viewport()->installEventFilter(this);

    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setStretchLastSection(false);
    header->setSectionsClickable(false);

    // 默认行高
    m_table->verticalHeader()->setDefaultSectionSize(40);

    auto *empty = new QWidget(m_tableStack);
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setAlignment(Qt::AlignCenter);
    auto *emptyLabel = makeLabel("暂无符合条件的历史记录", "imageMainText");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyLabel);

    m_tableStack->addWidget(m_table);   // index 0
    m_tableStack->addWidget(empty);     // index 1
    m_tableStack->setCurrentIndex(0);

    auto *pager = new QWidget(listPanel);
    auto *pagerLayout = new QHBoxLayout(pager);
    pagerLayout->setContentsMargins(0, 4, 0, 0);
    m_totalLabel = makeLabel(QStringLiteral("共 0 条 · 每页 8 条"), QStringLiteral("sectionHint"));
    pagerLayout->addWidget(m_totalLabel);
    pagerLayout->addStretch(1);
    m_prevPage = new ElaPushButton(QStringLiteral("上一页"), pager);
    m_prevPage->setFixedSize(72, 28);
    applySecondaryButtonStyle(m_prevPage);
    m_pageLabel = makeLabel(QStringLiteral("1 / 1"), QStringLiteral("sectionHint"));
    m_nextPage = new ElaPushButton(QStringLiteral("下一页"), pager);
    m_nextPage->setFixedSize(72, 28);
    applySecondaryButtonStyle(m_nextPage);
    pagerLayout->addWidget(m_prevPage);
    pagerLayout->addWidget(m_pageLabel);
    pagerLayout->addWidget(m_nextPage);

    if (listLayout) {
        listLayout->addWidget(m_tableStack, 1);
        listLayout->addWidget(pager, 0);
    }

    auto *detailPanel = createPanel(QStringLiteral("记录详情"), nullptr, m_splitter);
    auto *detailLayout = qobject_cast<QVBoxLayout *>(detailPanel->layout());

    m_detailStack = new QStackedWidget(detailPanel);

    auto *placeholder = new QWidget(m_detailStack);
    auto *placeholderLayout = new QVBoxLayout(placeholder);
    placeholderLayout->setContentsMargins(12, 12, 12, 12);
    placeholderLayout->setAlignment(Qt::AlignCenter);
    m_detailPlaceholderLabel = makeLabel(QStringLiteral("请选择左侧记录查看详情"), "imageMainText");
    m_detailPlaceholderLabel->setAlignment(Qt::AlignCenter);
    m_detailPlaceholderLabel->setWordWrap(true);
    placeholderLayout->addWidget(m_detailPlaceholderLabel);

    m_detailScroll = new QScrollArea(m_detailStack);
    m_detailScroll->setWidgetResizable(true);
    m_detailScroll->setFrameShape(QFrame::NoFrame);
    m_detailScroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_detailStack->addWidget(placeholder);
    m_detailStack->addWidget(m_detailScroll);
    m_detailStack->setCurrentIndex(0);

    if (detailLayout) {
        detailLayout->addWidget(m_detailStack, 1);
    }

    detailPanel->setMinimumWidth(460);
    listPanel->setMinimumWidth(520);
    m_splitter->addWidget(listPanel);
    m_splitter->addWidget(detailPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);
    m_splitter->installEventFilter(this);
    connect(m_splitter, &QSplitter::splitterMoved, this,
            [this](int, int) { updateMainTableColumnWidths(); });
    hostLayout->addWidget(m_splitter, 1);
    updateMainTableColumnWidths();

    connect(m_table, &QTableWidget::cellClicked,
            this, &HistoryDialog::onRowClicked);
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int row, int column, int, int) {
                onRowClicked(row, column);
            });

    return host;
}

QLabel *HistoryDialog::makeTileValue(QWidget *tile)
{
    return tile ? tile->findChild<QLabel *>("tileValue") : nullptr;
}

QWidget *HistoryDialog::makeTile(const QString &title, const QString &suffix, const QString &valueColor,
                                 QLabel *&valueLabelOut)
{
    auto *tile = new QFrame(this);
    tile->setObjectName("panel");
    tile->setMinimumHeight(88);

    auto *layout = new QVBoxLayout(tile);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);

    auto *titleLabel = makeLabel(title, "sectionHint");

    auto *valueRow = new QHBoxLayout();
    valueRow->setSpacing(4);
    valueRow->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

    auto *valueLabel = makeLabel(QStringLiteral("--"));
    valueLabel->setObjectName("tileValue");
    QFont valueFont = valueLabel->font();
    valueFont.setPixelSize(28);
    valueFont.setBold(true);
    valueLabel->setFont(valueFont);
    valueLabel->setStyleSheet(QStringLiteral("color:%1;font-size:28px;font-weight:700;border:none;background:transparent;")
                                  .arg(valueColor));
    auto *suffixLabel = makeLabel(suffix, "sectionHint");

    valueRow->addWidget(valueLabel);
    valueRow->addWidget(suffixLabel);
    valueRow->addStretch();

    layout->addWidget(titleLabel);
    layout->addLayout(valueRow);

    valueLabelOut = valueLabel;
    return tile;
}

// -----------------------------------------------------------------
// 数据装填
// -----------------------------------------------------------------

void HistoryDialog::showDetailPlaceholder(const QString &message)
{
    m_selectedRecordId = -1;
    if (m_detailScroll) {
        if (auto *old = m_detailScroll->takeWidget()) {
            old->deleteLater();
        }
        m_detailContent = nullptr;
    }
    if (m_detailPlaceholderLabel) {
        m_detailPlaceholderLabel->setText(message);
    }
    if (m_detailStack) {
        fadeToStackedWidgetPage(m_detailStack, m_detailStack->widget(0));
    }
}

void HistoryDialog::showDetailForRecord(int recordId)
{
    if (recordId <= 0 || !m_detailStack || !m_detailScroll) {
        return;
    }
    if (recordId == m_selectedRecordId && m_detailContent
        && m_detailStack->currentIndex() == 1) {
        return;
    }

    QWidget *detail = createDetailWidget(recordId);
    if (!detail) {
        showDetailPlaceholder(QStringLiteral("详情加载失败"));
        return;
    }

    if (auto *old = m_detailScroll->takeWidget()) {
        old->deleteLater();
    }
    detail->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    detail->setMinimumWidth(360);
    m_detailScroll->setWidget(detail);
    m_detailContent = detail;
    m_selectedRecordId = recordId;
    fadeToStackedWidgetPage(m_detailStack, m_detailScroll);
}

void HistoryDialog::reloadRecords()
{
    if (m_reloading) {
        return;
    }
    m_reloading = true;

    // 记住旧的选中记录 + 滚动位置，reload 完再恢复
    int savedSelectedId = -1;
    if (m_table) {
        const int sr = m_table->currentRow();
        if (sr >= 0 && sr < m_table->rowCount()) {
            if (auto *it = m_table->item(sr, 0)) {
                savedSelectedId = it->data(Qt::UserRole).toInt();
            }
        }
    }
    if (m_table) {
        m_table->clearSpans();
    }
    showDetailPlaceholder(QStringLiteral("正在查询历史记录..."));
    if (m_statusLabel) {
        m_statusLabel->setVisible(true);
        m_statusLabel->setText(QStringLiteral("正在查询历史记录..."));
    }

    InspectionQuery q;
    q.limit = 5000;
    const QDate today = QDate::currentDate();
    QDate fromDate = m_from ? m_from->date() : today.addDays(-14);
    QDate toDate = m_to ? m_to->date() : today;
    if (fromDate > toDate) {
        std::swap(fromDate, toDate);
    }
    q.createdFrom = QDateTime(fromDate, QTime(0, 0)).toUTC();
    q.createdTo = QDateTime(toDate.addDays(1), QTime(0, 0)).toUTC();

    QString err;
    const QList<InspectionRecordRow> rows = InspectionRepository::loadFiltered(q, &err);

    if (!err.isEmpty()) {
        m_table->setRowCount(0);
        if (m_tableStack) {
            fadeToStackedWidgetPage(m_tableStack, m_tableStack->widget(1));
        }
        showDetailPlaceholder(QStringLiteral("历史记录加载失败"));
        HistorySummary empty;
        updateSummary(empty);
        if (m_statusLabel) {
            m_statusLabel->setText(QStringLiteral("加载失败：%1").arg(err));
        }
        m_filtered.clear();
        renderPage();
        m_reloading = false;
        return;
    }

    const QString plate = m_plateEdit ? m_plateEdit->text().trimmed() : QString();
    const int statusIndex = m_statusCombo ? m_statusCombo->currentIndex() : 0;
    m_filtered.clear();
    for (const InspectionRecordRow &row : rows) {
        if (!plate.isEmpty() && !plateNumber(row).contains(plate)) {
            continue;
        }
        const TaskStatus status = taskStatusOf(row);
        if (statusIndex == 1 && status != TaskStatus::Abnormal) {
            continue;
        }
        if (statusIndex == 2 && status != TaskStatus::MillCompleted) {
            continue;
        }
        if (statusIndex == 3 && status != TaskStatus::ReviewCompleted) {
            continue;
        }
        if (statusIndex == 4 && status != TaskStatus::ManualPassed) {
            continue;
        }
        m_filtered.append(row);
    }

    HistorySummary sum;
    sum.taskCount = m_filtered.size();
    for (const InspectionRecordRow &row : m_filtered) {
        sum.totalParticles += row.particleCount;
        if (row.particleCount > 0) {
            ++sum.particleBoards;
            if (row.processOutcome == ProcessOutcome::Completed) {
                ++sum.completedBoards;
            }
        }
        if (taskStatusOf(row) == TaskStatus::Abnormal) {
            ++sum.pendingBoards;
        }
    }
    updateSummary(sum);
    if (m_statusLabel) {
        m_statusLabel->setVisible(false);
    }
    const int pages = qMax(1, (m_filtered.size() + kPageSize - 1) / kPageSize);
    if (savedSelectedId > 0) {
        int index = -1;
        for (int i = 0; i < m_filtered.size(); ++i) {
            if (m_filtered.at(i).id == savedSelectedId) {
                index = i;
                break;
            }
        }
        m_page = index >= 0 ? index / kPageSize : 0;
    }
    if (m_page >= pages) {
        m_page = pages - 1;
    }
    renderPage();
    m_reloading = false;
}

void HistoryDialog::renderPage()
{
    const int pages = qMax(1, (m_filtered.size() + kPageSize - 1) / kPageSize);
    if (m_page < 0) {
        m_page = 0;
    }
    if (m_page >= pages) {
        m_page = pages - 1;
    }
    const int begin = m_page * kPageSize;
    populateTable(m_filtered.mid(begin, kPageSize));
    if (m_tableStack) {
        fadeToStackedWidgetPage(m_tableStack,
                                m_tableStack->widget(m_filtered.isEmpty() ? 1 : 0));
    }
    if (m_totalLabel) {
        m_totalLabel->setText(QStringLiteral("共 %1 条 · 每页 %2 条").arg(m_filtered.size()).arg(kPageSize));
    }
    if (m_pageLabel) {
        m_pageLabel->setText(QStringLiteral("%1 / %2").arg(m_filtered.isEmpty() ? 0 : m_page + 1).arg(m_filtered.isEmpty() ? 0 : pages));
    }
    if (m_prevPage) {
        m_prevPage->setEnabled(m_page > 0);
    }
    if (m_nextPage) {
        m_nextPage->setEnabled(m_page + 1 < pages);
    }
    if (m_table && m_table->rowCount() > 0) {
        m_table->selectRow(0);
        if (auto *item = m_table->item(0, 0)) {
            showDetailForRecord(item->data(Qt::UserRole).toInt());
        }
    } else {
        showDetailPlaceholder(QStringLiteral("请选择左侧记录查看详情"));
    }
}

void HistoryDialog::exportCurrentList()
{
    if (m_filtered.isEmpty()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出当前列表"), QStringLiteral("历史记录.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << QChar(0xFEFF);
    out << QStringLiteral("铜板编号,检测时间,粒子数,最大高度(mm),已修磨,结果,配方,备注\n");
    for (const InspectionRecordRow &row : m_filtered) {
        out << plateNumber(row) << ','
            << formatDateTime(row.createdAt) << ','
            << row.particleCount << ','
            << QString::number(row.maxParticleHeight, 'f', 3) << ','
            << row.clearedCount << ','
            << taskStatusDetail(row) << ','
            << row.recipeName << ','
            << QString(row.remark).replace(',', QStringLiteral("，")) << '\n';
    }
}

void HistoryDialog::populateTable(const QList<InspectionRecordRow> &rows)
{
    m_table->clearSpans();
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const InspectionRecordRow &row = rows.at(i);
        const TaskStatus result = taskStatusOf(row);
        int col = 0;
        auto *idItem = new QTableWidgetItem(plateNumber(row));
        idItem->setData(Qt::UserRole, row.id);
        m_table->setItem(i, col++, idItem);
        m_table->setItem(i, col++, new QTableWidgetItem(
            row.createdAt.isValid() ? row.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                    : QStringLiteral("--")));
        m_table->setItem(i, col++, new QTableWidgetItem(QStringLiteral("--")));
        m_table->setItem(i, col++, new QTableWidgetItem(QString::number(row.particleCount)));
        m_table->setItem(i, col++, new QTableWidgetItem(QString::number(row.maxParticleHeight, 'f', 1)));
        m_table->setItem(i, col++, new QTableWidgetItem(QString::number(row.clearedCount)));
        m_table->setCellWidget(i, col++, makeResultPill(result, m_table));

        auto *viewButton = new QPushButton(QStringLiteral("查看"), m_table);
        viewButton->setCursor(Qt::PointingHandCursor);
        viewButton->setFlat(true);
        viewButton->setStyleSheet(QStringLiteral(
            "QPushButton { color:%1; background:transparent; border:none; font-size:13px; }"
            "QPushButton:hover { color:%2; }")
                                      .arg(QLatin1String(Theme::kAccent),
                                           QLatin1String(Theme::kTextPrimary)));
        const int recordId = row.id;
        connect(viewButton, &QPushButton::clicked, this, [this, recordId]() {
            const int tableRow = findRowByRecordId(recordId);
            if (tableRow >= 0 && m_table) {
                m_table->selectRow(tableRow);
            }
            showDetailForRecord(recordId);
            openReviewPage(recordId);
        });
        auto *viewHost = new QWidget(m_table);
        auto *viewLayout = new QHBoxLayout(viewHost);
        viewLayout->setContentsMargins(0, 0, 0, 0);
        viewLayout->addStretch(1);
        viewLayout->addWidget(viewButton);
        viewLayout->addStretch(1);
        m_table->setCellWidget(i, col++, viewHost);
    }
    centerTableCells(m_table);
}

void HistoryDialog::updateSummary(const HistorySummary &s)
{
    if (m_tileTaskCount) {
        m_tileTaskCount->setText(QString::number(s.taskCount));
    }
    if (m_tileParticles) {
        m_tileParticles->setText(QString::number(s.totalParticles));
    }
    if (m_tileClearRate) {
        if (s.particleBoards <= 0) {
            m_tileClearRate->setText(QStringLiteral("--"));
        } else {
            const double rate = 100.0 * double(s.completedBoards) / double(s.particleBoards);
            m_tileClearRate->setText(QString::number(rate, 'f', 1));
        }
    }
    if (m_tilePending) {
        m_tilePending->setText(QString::number(s.pendingBoards));
    }
}

// -----------------------------------------------------------------
// Master-detail selection
// -----------------------------------------------------------------

void HistoryDialog::openReviewPage(int recordId)
{
    if (recordId <= 0) {
        return;
    }

    QString plate = QString::number(recordId);
    for (const InspectionRecordRow &row : m_filtered) {
        if (row.id == recordId) {
            plate = plateNumber(row);
            break;
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("查看"));
    dialog.setModal(true);
    dialog.resize(780, 560);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background:%1; }"
        "QLabel#reviewTitle { color:%2; font-size:16px; font-weight:700; border:none; background:transparent; }"
        "QLabel#reviewPlate { color:%3; font-size:13px; border:none; background:transparent; }")
                             .arg(QLatin1String(Theme::kPanelBg),
                                  QLatin1String(Theme::kTextPrimary),
                                  QLatin1String(Theme::kTextSecondary)));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("查看"), &dialog);
    title->setObjectName(QStringLiteral("reviewTitle"));
    auto *plateLabel = new QLabel(QStringLiteral("铜板编号：%1").arg(plate), &dialog);
    plateLabel->setObjectName(QStringLiteral("reviewPlate"));

    InspectionRecordRow record;
    bool found = false;
    for (const InspectionRecordRow &row : m_filtered) {
        if (row.id == recordId) {
            record = row;
            found = true;
            break;
        }
    }

    auto *image = new QLabel(QStringLiteral("打磨后画面尚未保存"), &dialog);
    image->setAlignment(Qt::AlignCenter);
    image->setMinimumHeight(220);
    image->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    image->setStyleSheet(QStringLiteral(
        "QLabel { color:%1; background:%2; border:1px solid %3; border-radius:6px; font-size:14px; }")
                             .arg(QLatin1String(Theme::kTextMuted),
                                  QLatin1String(Theme::kCanvasBg),
                                  QLatin1String(Theme::kBorder)));

    const bool canReview = found && record.processOutcome == ProcessOutcome::Completed;
    QString hintText = QStringLiteral("打磨还没有正常结束，暂时不能人工审核。");
    if (!found) {
        hintText = QStringLiteral("找不到这条检测记录。");
    } else if (record.processOutcome == ProcessOutcome::Abnormal) {
        hintText = QStringLiteral("这次打磨没有正常结束，已记为异常。人工审核要等打磨正常结束后再做。");
    } else if (canReview && record.manualReview == ManualReview::Passed) {
        hintText = QStringLiteral("当前为人工审核通过。可以改成异常。");
    } else if (canReview && record.manualReview == ManualReview::Abnormal) {
        hintText = QStringLiteral("当前为人工审核异常。可以改成通过。");
    } else if (canReview) {
        hintText = QStringLiteral("看打磨后的画面，标记通过或异常。复核用的第二次扫描尚未接入。");
    }

    auto *hint = new QLabel(hintText, &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1; font-size:13px; border:none; background:transparent;")
                            .arg(QLatin1String(Theme::kTextSecondary)));

    const QString buttonStyle = QStringLiteral(
        "QPushButton { color:%1; background:%2; border:1px solid %3; border-radius:6px; padding:0 16px; }"
        "QPushButton:hover { background:%4; }"
        "QPushButton:disabled { color:%5; background:%2; }")
                                    .arg(QLatin1String(Theme::kTextPrimary),
                                         QLatin1String(Theme::kPanelBg),
                                         QLatin1String(Theme::kBorder),
                                         QLatin1String(Theme::kHoverBg),
                                         QLatin1String(Theme::kTextMuted));

    auto *passButton = new QPushButton(QStringLiteral("通过"), &dialog);
    auto *abnormalButton = new QPushButton(QStringLiteral("异常"), &dialog);
    passButton->setCursor(Qt::PointingHandCursor);
    abnormalButton->setCursor(Qt::PointingHandCursor);
    passButton->setFixedHeight(32);
    abnormalButton->setFixedHeight(32);
    passButton->setEnabled(canReview);
    abnormalButton->setEnabled(canReview);
    passButton->setStyleSheet(buttonStyle);
    abnormalButton->setStyleSheet(buttonStyle);

    auto *closeButton = new QPushButton(QStringLiteral("关闭"), &dialog);
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setFixedHeight(32);
    closeButton->setStyleSheet(QStringLiteral(
        "QPushButton { color:%1; background:%2; border:1px solid %3; border-radius:6px; padding:0 16px; }"
        "QPushButton:hover { background:%4; }")
                                   .arg(QLatin1String(Theme::kTextPrimary),
                                        QLatin1String(Theme::kPanelBg),
                                        QLatin1String(Theme::kBorder),
                                        QLatin1String(Theme::kHoverBg)));
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    bool reviewChanged = false;
    const auto markReview = [this, recordId, hint, &reviewChanged](ManualReview review) {
        QString error;
        if (!InspectionRepository::setManualReview(recordId, review, &error)) {
            hint->setText(error.isEmpty() ? QStringLiteral("保存人工审核失败") : error);
            return;
        }
        reviewChanged = true;
        hint->setText(review == ManualReview::Passed
                          ? QStringLiteral("已标记通过。这条记录计入人工审核。")
                          : QStringLiteral("已标记异常。这条记录计入异常。"));
    };
    connect(passButton, &QPushButton::clicked, &dialog, [markReview]() {
        markReview(ManualReview::Passed);
    });
    connect(abnormalButton, &QPushButton::clicked, &dialog, [markReview]() {
        markReview(ManualReview::Abnormal);
    });

    auto *footer = new QHBoxLayout();
    footer->addWidget(passButton);
    footer->addWidget(abnormalButton);
    footer->addStretch(1);
    footer->addWidget(closeButton);

    layout->addWidget(title);
    layout->addWidget(plateLabel);
    layout->addWidget(image, 1);
    layout->addWidget(hint);
    layout->addLayout(footer);
    dialog.exec();
    if (reviewChanged) {
        // 等「查看」按钮的点击处理返回后再刷新，避免刷新表格时删掉还在回调里的按钮。
        QTimer::singleShot(0, this, [this]() { reloadRecords(); });
    }
}

void HistoryDialog::onRowClicked(int row, int /*column*/)
{
    if (!m_table || row < 0 || row >= m_table->rowCount()) {
        return;
    }
    auto *item = m_table->item(row, 0);
    if (!item) {
        return;
    }
    const int recordId = item->data(Qt::UserRole).toInt();
    if (recordId <= 0) {
        return;
    }
    m_table->selectRow(row);
    showDetailForRecord(recordId);
}

int HistoryDialog::findRowByRecordId(int recordId) const
{
    if (!m_table || recordId <= 0) {
        return -1;
    }
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto *it = m_table->item(i, 0);
        if (it && it->data(Qt::UserRole).toInt() == recordId) {
            return i;
        }
    }
    return -1;
}

QWidget *HistoryDialog::createDetailWidget(int recordId)
{
    QString err;
    const InspectionDetail detail = InspectionRepository::loadDetail(recordId, &err);

    auto *host = new QWidget();
    host->setAutoFillBackground(true);
    host->setStyleSheet(QStringLiteral("background:%1;").arg(QLatin1String(Theme::kWindowBg)));
    auto *hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(8, 4, 8, 8);
    hostLayout->setSpacing(8);

    if (!detail.loaded) {
        hostLayout->addWidget(makeLabel(err.isEmpty() ? QStringLiteral("详情加载失败")
                                                      : QStringLiteral("详情加载失败：%1").arg(err),
                                         QStringLiteral("sectionHint")));
        hostLayout->addStretch(1);
        return host;
    }

    const InspectionRecordRow &head = detail.head;
    const TaskStatus result = taskStatusOf(head);
    auto *title = makeLabel(QStringLiteral("铜板编号：%1").arg(plateNumber(head)), QStringLiteral("panelTitle"));
    hostLayout->addWidget(title);

    auto *top = new QHBoxLayout();
    top->setSpacing(12);
    auto *image = new QLabel(host);
    image->setFixedSize(220, 160);
    image->setAlignment(Qt::AlignCenter);
    image->setStyleSheet(QStringLiteral("background:%1;border:1px solid %2;border-radius:4px;")
                             .arg(Theme::kCanvasBg, Theme::kBorder));
    QPixmap pixmap(head.overviewImagePath);
    if (!pixmap.isNull()) {
        image->setPixmap(pixmap.scaled(image->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        image->setText(QStringLiteral("无总览图"));
        image->setStyleSheet(image->styleSheet() + QStringLiteral("color:%1;").arg(Theme::kTextMuted));
    }
    top->addWidget(image, 0, Qt::AlignTop);

    auto *fields = new QVBoxLayout();
    fields->setSpacing(4);
    const auto addField = [&](const QString &name, const QString &value) {
        auto *row = new QHBoxLayout();
        auto *nameLabel = makeLabel(name, QStringLiteral("sectionHint"));
        nameLabel->setFixedWidth(72);
        auto *valueLabel = makeLabel(value);
        valueLabel->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:600;border:none;background:transparent;")
                                      .arg(Theme::kTextPrimary));
        row->addWidget(nameLabel);
        row->addWidget(valueLabel, 1);
        fields->addLayout(row);
    };
    addField(QStringLiteral("操作员"), QStringLiteral("--"));
    addField(QStringLiteral("开始时间"),
             head.processStartAt.isValid() ? head.processStartAt.toString(QStringLiteral("HH:mm:ss"))
                                           : QStringLiteral("--"));
    addField(QStringLiteral("检测用时"), durationSeconds(head.inspectStartAt, head.inspectEndAt));
    addField(QStringLiteral("修磨用时"), durationSeconds(head.processStartAt, head.processEndAt));
    addField(QStringLiteral("最大高度"), QStringLiteral("%1 mm").arg(head.maxParticleHeight, 0, 'f', 1));
    auto *statusName = makeLabel(QStringLiteral("任务状态"), QStringLiteral("sectionHint"));
    statusName->setFixedWidth(72);
    auto *statusValue = makeLabel(taskStatusDetail(head));
    statusValue->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:700;border:none;background:transparent;")
                                   .arg(resultColor(result).name()));
    auto *statusRow = new QHBoxLayout();
    statusRow->addWidget(statusName);
    statusRow->addWidget(statusValue, 1);
    fields->addLayout(statusRow);
    if (!head.remark.isEmpty()) {
        addField(QStringLiteral("备注"), head.remark);
    }
    fields->addStretch(1);
    top->addLayout(fields, 1);
    hostLayout->addLayout(top);

    hostLayout->addWidget(makeLabel(QStringLiteral("任务进度"), QStringLiteral("panelTitle")));
    auto *steps = new QHBoxLayout();
    const QStringList stepNames{QStringLiteral("扫描完成"), QStringLiteral("任务下发"), taskStatusText(result)};
    for (int i = 0; i < stepNames.size(); ++i) {
        const bool current = (result == TaskStatus::Unset && i == stepNames.size() - 1);
        auto *step = makeLabel(stepNames.at(i));
        step->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:600;border:none;background:transparent;")
                                .arg(current ? Theme::kAccent : Theme::kStateSuccess));
        steps->addWidget(step);
        if (i + 1 < stepNames.size()) {
            auto *line = new QFrame(host);
            line->setFixedHeight(1);
            line->setMinimumWidth(24);
            line->setStyleSheet(QStringLiteral("background:%1;border:none;").arg(Theme::kBorder));
            steps->addWidget(line, 1);
        }
    }
    hostLayout->addLayout(steps);
    hostLayout->addWidget(makeLabel(QStringLiteral("已处理 %1 / %2 个粒子")
                                        .arg(head.clearedCount)
                                        .arg(head.particleCount),
                                    QStringLiteral("sectionHint")),
                          0, Qt::AlignRight);

    auto *stack = new QStackedWidget(host);
    auto *particleTable = new QTableWidget(stack);
    particleTable->setColumnCount(6);
    particleTable->setHorizontalHeaderLabels({
        QStringLiteral("序号"), QStringLiteral("X"), QStringLiteral("Y"),
        QStringLiteral("Z"), QStringLiteral("高度(mm)"), QStringLiteral("已修磨")
    });
    setupDetailTable(particleTable);
    particleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    particleTable->setRowCount(detail.particles.size());
    for (int i = 0; i < detail.particles.size(); ++i) {
        const ParticleDetectionRow &particle = detail.particles.at(i);
        int col = 0;
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(particle.particleIndex)));
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(particle.positionX, 'f', 1)));
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(particle.positionY, 'f', 1)));
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(particle.positionZ, 'f', 1)));
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(particle.height, 'f', 2)));
        particleTable->setItem(i, col++, new QTableWidgetItem(particle.isCleared ? QStringLiteral("是")
                                                                                : QStringLiteral("否")));
    }
    centerTableCells(particleTable);

    auto *resultPage = new QWidget(stack);
    auto *resultLayout = new QVBoxLayout(resultPage);
    resultLayout->setContentsMargins(8, 8, 8, 8);
    resultLayout->addWidget(makeLabel(QStringLiteral("配方：%1").arg(head.recipeName.isEmpty()
                                                                      ? QStringLiteral("--")
                                                                      : head.recipeName)));
    resultLayout->addWidget(makeLabel(QStringLiteral("结果：%1").arg(taskStatusDetail(head))));
    resultLayout->addWidget(makeLabel(QStringLiteral("粒子 %1 个，已修磨 %2 个")
                                          .arg(head.particleCount)
                                          .arg(head.clearedCount)));
    resultLayout->addWidget(makeLabel(head.remark.isEmpty() ? QStringLiteral("备注：无")
                                                            : QStringLiteral("备注：%1").arg(head.remark),
                                      QStringLiteral("sectionHint")));
    resultLayout->addStretch(1);

    stack->addWidget(particleTable);
    stack->addWidget(resultPage);
    stack->setCurrentIndex(0);
    hostLayout->addWidget(stack, 1);

    auto *buttons = new QHBoxLayout();
    auto *particleButton = new ElaPushButton(QStringLiteral("查看粒子明细"), host);
    auto *resultButton = new ElaPushButton(QStringLiteral("查看检测结果"), host);
    particleButton->setFixedHeight(34);
    resultButton->setFixedHeight(34);
    applyPrimaryButtonStyle(particleButton);
    applySecondaryButtonStyle(resultButton);
    buttons->addWidget(particleButton, 1);
    buttons->addWidget(resultButton, 1);
    hostLayout->addLayout(buttons);
    particleButton->setCheckable(true);
    resultButton->setCheckable(true);
    particleButton->setChecked(true);
    applySelectedButtonStyle(particleButton);
    applySecondaryButtonStyle(resultButton);
    connect(particleButton, &ElaPushButton::clicked, stack, [stack, particleButton, resultButton]() {
        fadeToStackedWidgetPage(stack, stack->widget(0));
        particleButton->setChecked(true);
        resultButton->setChecked(false);
        applySelectedButtonStyle(particleButton);
        applySecondaryButtonStyle(resultButton);
        particleButton->update();
        resultButton->update();
    });
    connect(resultButton, &ElaPushButton::clicked, stack, [stack, particleButton, resultButton]() {
        fadeToStackedWidgetPage(stack, stack->widget(1));
        particleButton->setChecked(false);
        resultButton->setChecked(true);
        applySecondaryButtonStyle(particleButton);
        applySelectedButtonStyle(resultButton);
        particleButton->update();
        resultButton->update();
    });
    return host;
}
