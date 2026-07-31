#include "HistoryDialog.h"

#include "AppStyle.h"
#include "ElaComboBox.h"
#include "ElaIcon.h"
#include "ElaIconButton.h"
#include "ElaPushButton.h"
#include "InspectionRepository.h"
#include "UiHelpers.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QColor>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <utility>

// 头文件里 forward declare 的汇总结构体：只在本 .cpp 内使用
struct HistorySummary
{
    int    taskCount      = 0;
    qint64 totalParticles = 0;
    qint64 totalCleared   = 0;
    double maxHeight      = 0.0;
};

namespace {

enum RangeIndex {
    RangeAll     = 0,
    RangeToday   = 1,
    Range7Days   = 2,
    Range30Days  = 3,
    RangeCustom  = 4,
};

// 主表列宽权重（编号 / 配方 / 创建时间 / 检测结束 / 颗粒 / 已清 / 峰高）。
// 所有列按权重共享可用宽度，不再让最后一列独占剩余空间。
constexpr int kMainTableColumnWeights[7] = { 8, 14, 18, 18, 8, 8, 12 };
constexpr int kMainTableColumnWeightTotal = 86;

QString formatDateTime(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm:ss") : QStringLiteral("--");
}

ElaIconButton *createTitleButton(ElaIconType::IconName icon, const QString &tooltip, QWidget *parent)
{
    auto *button = new ElaIconButton(icon, 16, 40, 40, parent);
    button->setFixedSize(40, 40);
    button->setToolTip(tooltip);
    button->setCursor(Qt::ArrowCursor);
    button->setBorderRadius(4);
    button->setLightIconColor(QColor("#E6EEF5"));
    button->setDarkIconColor(QColor("#E6EEF5"));
    button->setLightHoverIconColor(QColor("#FFFFFF"));
    button->setDarkHoverIconColor(QColor("#FFFFFF"));
    button->setLightHoverColor(QColor("#22303D"));
    button->setDarkHoverColor(QColor("#22303D"));
    return button;
}

const char *kDialogExtraStyle = R"(
    QDialog {
        background: #16202B;
    }
    #historyTitleBar {
        background: #0E151D;
        border-bottom: 1px solid #2E3E4E;
    }
    #historyContent {
        background: #16202B;
    }
    QTableWidget {
        background: #182431;
        color: #E4EAF0;
        gridline-color: #2E3E4E;
        border: 1px solid #2E3E4E;
        border-radius: 6px;
        alternate-background-color: #1B2836;
        selection-background-color: #2D6F9F;
        selection-color: #F1F5F9;
    }
    QTableWidget::item {
        padding: 6px 8px;
    }
    QTableWidget::item:hover {
        background: #223246;
    }
    QTableWidget::item:selected {
        background: #2D6F9F;
        color: #F7FBFF;
    }
    QSplitter::handle {
        background: #22303D;
    }
    QSplitter::handle:hover {
        background: #2D6F9F;
    }
    QScrollArea {
        background: #131E28;
        border: none;
    }
    QHeaderView::section {
        background: #0E151D;
        color: #DDE7F0;
        padding: 6px 10px;
        border: none;
        border-right: 1px solid #2E3E4E;
        border-bottom: 1px solid #2E3E4E;
        font-weight: 600;
    }
    QTableCornerButton::section {
        background: #0E151D;
        border: 1px solid #2E3E4E;
    }
    QTabWidget::pane {
        border: 1px solid #2E3E4E;
        border-radius: 6px;
        background: #182431;
        top: -1px;
    }
    QTabBar::tab {
        background: #131E28;
        color: #B8C8D8;
        padding: 6px 16px;
        border: 1px solid #2E3E4E;
        border-bottom: none;
        border-top-left-radius: 4px;
        border-top-right-radius: 4px;
        margin-right: 2px;
    }
    QTabBar::tab:selected {
        background: #182431;
        color: #F1F5F9;
    }
    QDateEdit {
        background: #101923;
        color: #E6EEF5;
        border: 1px solid #334353;
        border-radius: 5px;
        padding: 2px 6px;
        min-height: 26px;
    }
    QDateEdit:disabled {
        color: #4A5866;
        background: #131C25;
        border: 1px solid #263340;
    }
    QDateEdit::drop-down {
        subcontrol-origin: padding;
        subcontrol-position: right;
        width: 18px;
        border-left: 1px solid #2E3E4E;
    }
    QCalendarWidget QWidget {
        alternate-background-color: #1B2836;
        color: #E4EAF0;
    }
    QCalendarWidget QAbstractItemView:enabled {
        background: #16202B;
        color: #E4EAF0;
        selection-background-color: #2D6F9F;
        selection-color: #F1F5F9;
    }
    QScrollBar:vertical {
        background: #101923;
        width: 10px;
        margin: 0;
        border: none;
    }
    QScrollBar::handle:vertical {
        background: #334353;
        border-radius: 4px;
        min-height: 30px;
    }
    QScrollBar::handle:vertical:hover {
        background: #4A5C6E;
    }
    QScrollBar::add-line:vertical,
    QScrollBar::sub-line:vertical {
        background: none;
        border: none;
        height: 0;
    }
    QScrollBar::add-page:vertical,
    QScrollBar::sub-page:vertical {
        background: none;
    }
    QScrollBar:horizontal {
        background: #101923;
        height: 10px;
        margin: 0;
        border: none;
    }
    QScrollBar::handle:horizontal {
        background: #334353;
        border-radius: 4px;
        min-width: 30px;
    }
    QScrollBar::handle:horizontal:hover {
        background: #4A5C6E;
    }
    QScrollBar::add-line:horizontal,
    QScrollBar::sub-line:horizontal {
        background: none;
        border: none;
        width: 0;
    }
    QScrollBar::add-page:horizontal,
    QScrollBar::sub-page:horizontal {
        background: none;
    }
)";

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
    setWindowTitle("历史检测记录");
    setModal(false);

    setWindowFlags((windowFlags() | Qt::Window) | Qt::FramelessWindowHint);
    setMinimumSize(1100, 680);
    setSizeGripEnabled(true);

    QSize target(1500, 900);
    if (QScreen *scr = (parent && parent->screen()) ? parent->screen() : this->screen()) {
        const QRect avail = scr->availableGeometry();
        target.setWidth(qMin(target.width(), int(avail.width() * 0.92)));
        target.setHeight(qMin(target.height(), int(avail.height() * 0.9)));
    }
    resize(target);

    setStyleSheet(mainWindowStyleSheet() + QString::fromUtf8(kDialogExtraStyle));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildTitleBar());

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
            populateRecipeCombo();
            reloadRecords();
        });
    }

    populateRecipeCombo();
    updateCustomRangeEnabled();
    reloadRecords();
    setWindowState(windowState() | Qt::WindowMaximized);
    updateMaximizeButtonIcon();
}

void HistoryDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateMaximizeButtonIcon();
    }
}

bool HistoryDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (m_table && watched == m_table->viewport() && event->type() == QEvent::Resize) {
        updateMainTableColumnWidths();
    }

    if (watched != m_titleBar) {
        return QDialog::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            toggleMaximized();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && !isMaximized()) {
            m_dragging = true;
            m_dragPosition = mouseEvent->pos();
            return true;
        }
    }

    if (event->type() == QEvent::MouseMove && m_dragging) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        move(mouseEvent->globalPos() - m_dragPosition);
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        m_dragging = false;
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

    int widths[7] = {};
    int remainders[7] = {};
    int assignedWidth = 0;
    for (int i = 0; i < 7; ++i) {
        const int weightedWidth = availableWidth * kMainTableColumnWeights[i];
        widths[i] = weightedWidth / kMainTableColumnWeightTotal;
        remainders[i] = weightedWidth % kMainTableColumnWeightTotal;
        assignedWidth += widths[i];
    }

    // Distribute rounding pixels to the largest fractional columns.
    for (int extra = availableWidth - assignedWidth; extra > 0; --extra) {
        int bestIndex = 0;
        for (int i = 1; i < 7; ++i) {
            if (remainders[i] > remainders[bestIndex]) {
                bestIndex = i;
            }
        }
        ++widths[bestIndex];
        remainders[bestIndex] = -1;
    }

    for (int i = 0; i < 7; ++i) {
        m_table->setColumnWidth(i, widths[i]);
    }
}

// -----------------------------------------------------------------
// 子控件构建
// -----------------------------------------------------------------

QWidget *HistoryDialog::buildTitleBar()
{
    auto *bar = new QFrame(this);
    bar->setObjectName("historyTitleBar");
    bar->setFixedHeight(52);
    bar->installEventFilter(this);
    m_titleBar = bar;

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(14, 0, 8, 0);
    layout->setSpacing(10);

    auto *logoLabel = new QLabel(bar);
    logoLabel->setFixedSize(34, 34);
    logoLabel->setPixmap(QPixmap(":/img/logo.png").scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setStyleSheet("background:transparent;border:none;");
    layout->addWidget(logoLabel);

    auto *titleLabel = makeLabel(QStringLiteral("历史检测记录"), "panelTitle");
    titleLabel->setStyleSheet("color:#F1F5F9;background:transparent;");
    layout->addWidget(titleLabel);
    layout->addStretch();

    auto *minimizeButton = createTitleButton(ElaIconType::Dash, QStringLiteral("最小化"), bar);
    m_maximizeButton = createTitleButton(ElaIconType::WindowRestore, QStringLiteral("还原"), bar);
    auto *closeButton = createTitleButton(ElaIconType::Xmark, QStringLiteral("关闭"), bar);
    closeButton->setLightHoverColor(QColor("#8B2B35"));
    closeButton->setDarkHoverColor(QColor("#8B2B35"));

    layout->addWidget(minimizeButton);
    layout->addWidget(m_maximizeButton);
    layout->addWidget(closeButton);

    connect(minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeButton, &QPushButton::clicked, this, &HistoryDialog::toggleMaximized);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);

    return bar;
}

QWidget *HistoryDialog::buildFilterBar()
{
    auto *bar = new QFrame(this);
    bar->setObjectName("controlBar");

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);

    auto *rangeLabel = makeLabel("时间范围", "sectionHint");
    m_rangeCombo = new ElaComboBox(bar);
    m_rangeCombo->addItem("全部时间");
    m_rangeCombo->addItem("今天");
    m_rangeCombo->addItem("最近 7 天");
    m_rangeCombo->addItem("最近 30 天");
    m_rangeCombo->addItem("自定义");
    m_rangeCombo->setCurrentIndex(RangeAll);
    m_rangeCombo->setMinimumWidth(140);

    const QDate today = QDate::currentDate();
    m_customFrom = new QDateEdit(today.addDays(-7), bar);
    m_customFrom->setCalendarPopup(true);
    m_customFrom->setDisplayFormat("yyyy-MM-dd");
    m_customFrom->setFixedWidth(130);

    m_customDash = makeLabel("—", "sectionHint");

    m_customTo = new QDateEdit(today, bar);
    m_customTo->setCalendarPopup(true);
    m_customTo->setDisplayFormat("yyyy-MM-dd");
    m_customTo->setFixedWidth(130);

    auto *recipeLabel = makeLabel("配方", "sectionHint");
    m_recipeCombo = new ElaComboBox(bar);
    m_recipeCombo->setMinimumWidth(200);

    m_refreshButton = new ElaPushButton(QStringLiteral("查询"), bar);
    m_refreshButton->setFixedSize(96, 32);
    applyPrimaryButtonStyle(m_refreshButton);

    layout->addWidget(rangeLabel);
    layout->addWidget(m_rangeCombo);
    layout->addWidget(m_customFrom);
    layout->addWidget(m_customDash);
    layout->addWidget(m_customTo);
    layout->addSpacing(8);
    layout->addWidget(recipeLabel);
    layout->addWidget(m_recipeCombo);
    layout->addStretch();
    layout->addWidget(m_refreshButton);

    connect(m_rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                updateCustomRangeEnabled();
                if (!m_reloading && m_statusLabel) {
                    m_statusLabel->setText(QStringLiteral("筛选条件已变化，点击查询更新结果"));
                }
            });
    connect(m_recipeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (!m_reloading && m_statusLabel) {
                    m_statusLabel->setText(QStringLiteral("筛选条件已变化，点击查询更新结果"));
                }
            });
    connect(m_customFrom, &QDateEdit::dateChanged, this, [this](const QDate &) {
        if (m_rangeCombo && m_rangeCombo->currentIndex() == RangeCustom && !m_reloading) {
            if (m_statusLabel) {
                m_statusLabel->setText(QStringLiteral("筛选条件已变化，点击查询更新结果"));
            }
        }
    });
    connect(m_customTo, &QDateEdit::dateChanged, this, [this](const QDate &) {
        if (m_rangeCombo && m_rangeCombo->currentIndex() == RangeCustom && !m_reloading) {
            if (m_statusLabel) {
                m_statusLabel->setText(QStringLiteral("筛选条件已变化，点击查询更新结果"));
            }
        }
    });

    return bar;
}

QWidget *HistoryDialog::buildSummaryRow()
{
    auto *host = new QWidget(this);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addWidget(makeTile("任务数",   "次",  m_tileTaskCount));
    layout->addWidget(makeTile("颗粒总数", "颗",  m_tileParticles));
    layout->addWidget(makeTile("清除率",   "%",   m_tileClearRate));
    layout->addWidget(makeTile("峰值高度", "mm",  m_tileMaxHeight));

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

    auto *listPanel = createPanel(QStringLiteral("检测任务列表"), nullptr, m_splitter);
    auto *listLayout = qobject_cast<QVBoxLayout *>(listPanel->layout());

    m_tableStack = new QStackedWidget(listPanel);

    m_table = new QTableWidget(m_tableStack);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("编号"),
        QStringLiteral("配方"),
        QStringLiteral("创建时间"),
        QStringLiteral("检测结束"),
        QStringLiteral("颗粒"),
        QStringLiteral("已清"),
        QStringLiteral("峰高(mm)")
    });
    setupCompactTable(m_table);
    m_table->setMouseTracking(true);
    m_table->viewport()->installEventFilter(this);

    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setStretchLastSection(false);
    header->setSectionsClickable(false);

    // 默认行高
    m_table->verticalHeader()->setDefaultSectionSize(32);

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

    if (listLayout) {
        listLayout->addWidget(m_tableStack, 1);
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

    m_splitter->addWidget(listPanel);
    m_splitter->addWidget(detailPanel);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({1, 1});
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

QWidget *HistoryDialog::makeTile(const QString &title, const QString &suffix, QLabel *&valueLabelOut)
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

    auto *valueLabel = makeLabel("--", "largeValue");
    valueLabel->setObjectName("tileValue");
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

void HistoryDialog::updateCustomRangeEnabled()
{
    const bool custom = (m_rangeCombo && m_rangeCombo->currentIndex() == RangeCustom);
    if (m_customFrom) m_customFrom->setVisible(custom);
    if (m_customTo)   m_customTo->setVisible(custom);
    if (m_customDash) m_customDash->setVisible(custom);
}

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
        m_detailStack->setCurrentIndex(0);
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
    m_detailStack->setCurrentIndex(1);
}

void HistoryDialog::populateRecipeCombo()
{
    if (!m_recipeCombo) {
        return;
    }
    const bool prev = m_reloading;
    m_reloading = true;

    const QString kept = m_recipeCombo->currentText();

    QString err;
    const QStringList names = InspectionRepository::loadRecipeNames(&err);

    m_recipeCombo->clear();
    m_recipeCombo->addItem("全部配方");
    for (const auto &n : names) {
        m_recipeCombo->addItem(n);
    }

    int idx = m_recipeCombo->findText(kept);
    m_recipeCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    m_reloading = prev;

    if (!err.isEmpty() && m_statusLabel) {
        m_statusLabel->setText(QStringLiteral("配方列表加载失败：%1").arg(err));
    }
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
    const int savedScroll =
        (m_table && m_table->verticalScrollBar())
            ? m_table->verticalScrollBar()->value()
            : 0;

    if (m_table) m_table->clearSpans();
    showDetailPlaceholder(QStringLiteral("正在查询历史记录..."));
    if (m_statusLabel) {
        m_statusLabel->setText(QStringLiteral("正在查询历史记录..."));
    }

    InspectionQuery q;
    q.limit = 200;
    if (m_recipeCombo && m_recipeCombo->currentIndex() > 0) {
        q.recipe = m_recipeCombo->currentText();
    }

    // 时间范围口径统一：都按本地自然日 [from 0:00, to 0:00) 的半开区间。
    // "今天" = [今天 0:00, 明天 0:00)
    // "最近 N 天" = [今天-(N-1) 0:00, 明天 0:00)
    // "自定义" = [from 0:00, to+1 0:00)，from > to 时自动交换。
    // createdTo 现在作为“不含”上界（Repository 已改为 CreatedAt < ?）。
    if (m_rangeCombo) {
        const QDate today = QDate::currentDate();
        const QDateTime tomorrowStart(today.addDays(1), QTime(0, 0));
        switch (m_rangeCombo->currentIndex()) {
        case RangeToday:
            q.createdFrom = QDateTime(today, QTime(0, 0)).toUTC();
            q.createdTo   = tomorrowStart.toUTC();
            break;
        case Range7Days:
            q.createdFrom = QDateTime(today.addDays(-6), QTime(0, 0)).toUTC();
            q.createdTo   = tomorrowStart.toUTC();
            break;
        case Range30Days:
            q.createdFrom = QDateTime(today.addDays(-29), QTime(0, 0)).toUTC();
            q.createdTo   = tomorrowStart.toUTC();
            break;
        case RangeCustom: {
            QDate fromDate = m_customFrom ? m_customFrom->date() : today;
            QDate toDate   = m_customTo   ? m_customTo->date()   : today;
            if (fromDate > toDate) {
                std::swap(fromDate, toDate);
            }
            q.createdFrom = QDateTime(fromDate, QTime(0, 0)).toUTC();
            q.createdTo   = QDateTime(toDate.addDays(1), QTime(0, 0)).toUTC();
            break;
        }
        default:
            break;
        }
    }

    QString err;
    const QList<InspectionRecordRow> rows = InspectionRepository::loadFiltered(q, &err);

    if (!err.isEmpty()) {
        m_table->setRowCount(0);
        if (m_tableStack) {
            m_tableStack->setCurrentIndex(1);
        }
        showDetailPlaceholder(QStringLiteral("历史记录加载失败"));
        HistorySummary empty;
        updateSummary(empty);
        if (m_statusLabel) {
            m_statusLabel->setText(QStringLiteral("加载失败：%1").arg(err));
        }
        m_reloading = false;
        return;
    }

    populateTable(rows);

    HistorySummary sum;
    sum.taskCount = rows.size();
    for (const auto &r : rows) {
        sum.totalParticles += r.particleCount;
        sum.totalCleared   += r.clearedCount;
        if (r.maxParticleHeight > sum.maxHeight) {
            sum.maxHeight = r.maxParticleHeight;
        }
    }
    updateSummary(sum);

    if (m_tableStack) {
        m_tableStack->setCurrentIndex(rows.isEmpty() ? 1 : 0);
    }
    if (m_statusLabel) {
        m_statusLabel->setText(rows.isEmpty()
                                   ? QStringLiteral("暂无符合条件的历史记录")
                                   : QStringLiteral("共 %1 条记录，选择左侧行查看详情").arg(rows.size()));
    }

    // 恢复选中和滚动条位置（按 recordId 找回；找不到就退回滚动位置）
    if (m_table) {
        int restoreRow = (savedSelectedId > 0) ? findRowByRecordId(savedSelectedId) : -1;
        if (restoreRow < 0 && m_table->rowCount() > 0) {
            restoreRow = 0;
        }
        if (restoreRow >= 0) {
            m_table->selectRow(restoreRow);
            if (auto *it = m_table->item(restoreRow, 0)) {
                showDetailForRecord(it->data(Qt::UserRole).toInt());
            }
        } else {
            showDetailPlaceholder(QStringLiteral("请选择左侧记录查看详情"));
        }
        if (auto *bar = m_table->verticalScrollBar()) {
            bar->setValue(qBound(bar->minimum(), savedScroll, bar->maximum()));
        }
    }

    m_reloading = false;
}

void HistoryDialog::populateTable(const QList<InspectionRecordRow> &rows)
{
    m_table->clearSpans();
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto &r = rows.at(i);
        int col = 0;
        auto *idItem = new QTableWidgetItem(QString::number(r.id));
        idItem->setData(Qt::UserRole, r.id);
        m_table->setItem(i, col++, idItem);
        m_table->setItem(i, col++, new QTableWidgetItem(r.recipeName));
        m_table->setItem(i, col++, new QTableWidgetItem(formatDateTime(r.createdAt)));
        m_table->setItem(i, col++, new QTableWidgetItem(formatDateTime(r.inspectEndAt)));
        m_table->setItem(i, col++, new QTableWidgetItem(QString::number(r.particleCount)));
        m_table->setItem(i, col++, new QTableWidgetItem(QString::number(r.clearedCount)));
        m_table->setItem(i, col++, new QTableWidgetItem(QString::number(r.maxParticleHeight, 'f', 3)));
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
        if (s.totalParticles <= 0) {
            m_tileClearRate->setText("--");
        } else {
            const double rate = 100.0 * double(s.totalCleared) / double(s.totalParticles);
            m_tileClearRate->setText(QString::number(rate, 'f', 1));
        }
    }
    if (m_tileMaxHeight) {
        m_tileMaxHeight->setText(s.taskCount > 0
                                     ? QString::number(s.maxHeight, 'f', 3)
                                     : QStringLiteral("--"));
    }
}

// -----------------------------------------------------------------
// Master-detail selection
// -----------------------------------------------------------------

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

void HistoryDialog::toggleMaximized()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
    updateMaximizeButtonIcon();
}

void HistoryDialog::updateMaximizeButtonIcon()
{
    if (!m_maximizeButton) {
        return;
    }
    m_maximizeButton->setAwesome(isMaximized() ? ElaIconType::WindowRestore : ElaIconType::Square);
    m_maximizeButton->setToolTip(isMaximized() ? QStringLiteral("还原") : QStringLiteral("最大化"));
}

QWidget *HistoryDialog::createDetailWidget(int recordId)
{
    QString err;
    const InspectionDetail detail = InspectionRepository::loadDetail(recordId, &err);

    auto *host = new QWidget();
    host->setAutoFillBackground(true);
    host->setStyleSheet("background:#131E28;");

    auto *hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(10, 6, 10, 10);
    hostLayout->setSpacing(3);

    if (!detail.loaded) {
        auto *msg = makeLabel(err.isEmpty() ? QStringLiteral("详情加载失败")
                                            : QStringLiteral("详情加载失败：%1").arg(err),
                              "sectionHint");
        hostLayout->addWidget(msg);
        hostLayout->addStretch();
        return host;
    }

    const auto &h = detail.head;
    auto compactDetailText = [](QLabel *label) {
        if (!label) {
            return;
        }
        label->setWordWrap(false);
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        label->setFixedHeight(label->sizeHint().height());
    };

    auto *title = makeLabel(QStringLiteral("任务 #%1 · %2")
                                .arg(h.id)
                                .arg(h.recipeName.isEmpty() ? QStringLiteral("未命名配方")
                                                            : h.recipeName),
                            "panelTitle");
    hostLayout->addWidget(title, 0, Qt::AlignTop);
    compactDetailText(title);

    auto *meta = makeLabel(QStringLiteral("创建时间：%1  |  加工：%2 — %3  |  检测：%4 — %5")
                               .arg(formatDateTime(h.createdAt))
                               .arg(formatDateTime(h.processStartAt))
                               .arg(formatDateTime(h.processEndAt))
                               .arg(formatDateTime(h.inspectStartAt))
                               .arg(formatDateTime(h.inspectEndAt)),
                           "sectionHint");
    hostLayout->addWidget(meta, 0, Qt::AlignTop);
    compactDetailText(meta);

    const double rate = (h.particleCount > 0)
                            ? 100.0 * double(h.clearedCount) / double(h.particleCount)
                            : 0.0;
    auto *kpi = makeLabel(QStringLiteral("颗粒数 %1  ·  已清除 %2  ·  清除率 %3%%  ·  峰值高度 %4 mm")
                              .arg(h.particleCount)
                              .arg(h.clearedCount)
                              .arg(QString::number(rate, 'f', 1))
                              .arg(QString::number(h.maxParticleHeight, 'f', 3)),
                          "metricValue");
    hostLayout->addWidget(kpi, 0, Qt::AlignTop);
    compactDetailText(kpi);

    QStringList overviewParts;
    if (!h.remark.isEmpty()) {
        overviewParts << QStringLiteral("备注：%1").arg(h.remark);
    }
    overviewParts << QStringLiteral("总览图：%1")
                         .arg(h.overviewImagePath.isEmpty() ? QStringLiteral("无")
                                                            : h.overviewImagePath);
    auto *overview = makeLabel(overviewParts.join(QStringLiteral("    ")), "sectionHint");
    hostLayout->addWidget(overview, 0, Qt::AlignTop);
    compactDetailText(overview);

    auto *tabs = new QTabWidget(host);
    tabs->setDocumentMode(true);

    // 颗粒明细
    auto *particleTable = new QTableWidget(tabs);
    particleTable->setColumnCount(8);
    particleTable->setHorizontalHeaderLabels({
        "序号", "X", "Y", "Z", "高度(mm)", "已清除", "检测时间", "清除时间"
    });
    setupDetailTable(particleTable);
    particleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    particleTable->setRowCount(detail.particles.size());
    for (int i = 0; i < detail.particles.size(); ++i) {
        const auto &p = detail.particles.at(i);
        int col = 0;
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.particleIndex)));
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.positionX, 'f', 2)));
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.positionY, 'f', 2)));
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.positionZ, 'f', 2)));
        particleTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.height, 'f', 3)));
        particleTable->setItem(i, col++, new QTableWidgetItem(p.isCleared ? QStringLiteral("是")
                                                                          : QStringLiteral("否")));
        particleTable->setItem(i, col++, new QTableWidgetItem(formatDateTime(p.detectedAt)));
        particleTable->setItem(i, col++, new QTableWidgetItem(formatDateTime(p.clearedAt)));
    }
    centerTableCells(particleTable);
    particleTable->resizeRowsToContents();

    // 工艺快照
    auto *processTable = new QTableWidget(tabs);
    processTable->setColumnCount(11);
    processTable->setHorizontalHeaderLabels({
        "路径", "峰高(mm)", "TargetX", "TargetY", "TargetZ",
        "进给速度", "进给量", "主轴转速", "切次", "单切量", "快照时间"
    });
    setupDetailTable(processTable);
    processTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    processTable->setRowCount(detail.processHistory.size());
    for (int i = 0; i < detail.processHistory.size(); ++i) {
        const auto &p = detail.processHistory.at(i);
        int col = 0;
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.pathIndex)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.maxParticleHeight, 'f', 3)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.targetX, 'f', 3)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.targetY, 'f', 3)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.targetZ, 'f', 3)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.feedSpeed, 'f', 2)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.feedAmount, 'f', 3)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.spindleSpeed, 'f', 1)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.cutCount)));
        processTable->setItem(i, col++, new QTableWidgetItem(QString::number(p.singleCutAmount, 'f', 3)));
        processTable->setItem(i, col++, new QTableWidgetItem(formatDateTime(p.snapshotAt)));
    }
    centerTableCells(processTable);
    processTable->resizeRowsToContents();

    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    tabs->addTab(particleTable, QStringLiteral("颗粒明细 (%1)").arg(detail.particles.size()));
    tabs->addTab(processTable,  QStringLiteral("工艺快照 (%1)").arg(detail.processHistory.size()));
    hostLayout->addWidget(tabs, 1);

    return host;
}
