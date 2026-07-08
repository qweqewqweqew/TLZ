#include "HistoryDialog.h"

#include "AppStyle.h"
#include "ElaComboBox.h"
#include "ElaPushButton.h"
#include "InspectionRepository.h"
#include "UiHelpers.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QEasingCurve>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QWheelEvent>

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

// 展开行的目标高度、动画时长
constexpr int kExpandedRowMinHeight = 220;
constexpr int kAnimDurationMs    = 220;

// 主表列宽（编号 / 配方 / 加工开始 / 加工结束 / 检测开始 / 检测结束 / 颗粒 / 已清 / 峰高）
// 最后一列 stretch 填满剩余空间，故只固定前 8 列。
constexpr int kColWidths[8] = { 72, 150, 160, 160, 160, 160, 72, 72 };

QString formatDateTime(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm:ss") : QStringLiteral("--");
}

const char *kDialogExtraStyle = R"(
    QDialog {
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

// 内嵌详情里用的表：完全关掉自身滚动条，由外层大表统一滚动。
// 通过撑高表格自身来避免出现自己的滚动条。
void setupNoScrollTable(QTableWidget *table)
{
    setupCompactTable(table);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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

// 按当前行数把表格自身高度撑到不出滚动条
int tableContentHeight(QTableWidget *table)
{
    int h = 2; // 边框
    if (table->horizontalHeader()->isVisible()) {
        h += table->horizontalHeader()->height();
    }
    for (int i = 0; i < table->rowCount(); ++i) {
        h += table->rowHeight(i);
    }
    return h;
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

    // 根据屏幕可用尺寸按比例自适应，然后 setFixedSize 锁死禁止拖拉
    QSize target(1280, 760);
    if (QScreen *scr = (parent && parent->screen()) ? parent->screen() : this->screen()) {
        const QRect avail = scr->availableGeometry();
        // 基准 1920x1080，按屏幕尺寸等比缩放；限制在合理区间
        const double scale = qMin(avail.width()  / 1920.0,
                                  avail.height() / 1080.0);
        const double clamped = qBound(0.75, scale, 1.5);
        target = QSize(int(1280 * clamped), int(760 * clamped));
        // 再保底：不超过 90% 可用区域
        target.setWidth (qMin(target.width(),  int(avail.width()  * 0.9)));
        target.setHeight(qMin(target.height(), int(avail.height() * 0.9)));
    }
    setFixedSize(target);

    // 去掉窗口最大化按钮和大小手柄
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setSizeGripEnabled(false);

    setStyleSheet(mainWindowStyleSheet() + QString::fromUtf8(kDialogExtraStyle));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    root->addWidget(buildFilterBar());
    root->addWidget(buildSummaryRow());

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("sectionHint");
    root->addWidget(m_statusLabel);

    root->addWidget(buildTableCard(), 1);

    // 过滤器 debounce：连续变更合并成一次查询
    m_reloadTimer = new QTimer(this);
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(180);
    connect(m_reloadTimer, &QTimer::timeout, this, &HistoryDialog::reloadRecords);

    if (m_refreshButton) {
        connect(m_refreshButton, &ElaPushButton::clicked, this, [this]() {
            // 手动刷新是显式意图，立即执行、不走 debounce
            if (m_reloadTimer) m_reloadTimer->stop();
            populateRecipeCombo();
            reloadRecords();
        });
    }

    // 展开/收起共用一个动画对象：每次调用前设定起止值即可
    m_expandAnim = new QVariantAnimation(this);
    m_expandAnim->setDuration(kAnimDurationMs);
    m_expandAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_expandAnim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                if (!m_table || m_expandedRow < 0
                    || m_expandedRow >= m_table->rowCount()) {
                    return;
                }
                m_table->setRowHeight(m_expandedRow, value.toInt());
            });

    // 平滑滚轮动画：把 verticalScrollBar 的当前值线性/缓动过渡到目标值
    m_scrollAnim = new QVariantAnimation(this);
    m_scrollAnim->setDuration(240);
    m_scrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_scrollAnim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                if (!m_table) return;
                m_table->verticalScrollBar()->setValue(value.toInt());
            });

    // 主表 viewport 装事件过滤器，接管滚轮
    if (m_table && m_table->viewport()) {
        m_table->viewport()->installEventFilter(this);
    }

    populateRecipeCombo();
    updateCustomRangeEnabled();
    reloadRecords();
}

// -----------------------------------------------------------------
// 事件过滤器：把滚轮转化成带缓动的平滑滚动
// -----------------------------------------------------------------

bool HistoryDialog::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::Wheel && m_table && m_scrollAnim) {
        // 只处理来自主表 viewport 或详情区域（含其后代）的滚轮；
        // 详情区里的 QTabBar / 内嵌表 viewport 都会偷吃 wheel，
        // 全部转给主表统一走平滑滚动。
        auto *w = qobject_cast<QWidget *>(obj);
        const bool fromMainViewport = (w == m_table->viewport());
        const bool fromDetail =
            (m_expandedWidget && w
             && (w == m_expandedWidget || m_expandedWidget->isAncestorOf(w)));
        if (!fromMainViewport && !fromDetail) {
            return QDialog::eventFilter(obj, ev);
        }

        auto *we = static_cast<QWheelEvent *>(ev);
        auto *bar = m_table->verticalScrollBar();
        if (!bar) {
            return false;
        }
        // 每次以当前动画目标为起点，避免连续滚动被打断后跳回原点
        const int cur = (m_scrollAnim->state() == QAbstractAnimation::Running)
                            ? m_scrollAnim->endValue().toInt()
                            : bar->value();

        // 一格滚轮 = 120 度；步长按行高来算，避免"一次跳一整屏"的顿挫感。
        // 用 pixelDelta 优先（触控板 / 高精度滚轮），退回 angleDelta。
        const int rowH = qMax(m_table->verticalHeader()->defaultSectionSize(), 24);
        int deltaPixels = we->pixelDelta().y();
        if (deltaPixels == 0) {
            const int deltaAngle = we->angleDelta().y();
            // 一格滚轮 ~ 1.5 行；这个值调低就更细腻，调高就更快
            deltaPixels = (deltaAngle * rowH * 10 / 1) / 120;
        }
        const int target = qBound(bar->minimum(), cur - deltaPixels, bar->maximum());

        // 目标没变就不重启动画，避免高频事件把动画一直"重设"导致视觉停滞
        if (target == cur) {
            return true;
        }

        m_scrollAnim->stop();
        m_scrollAnim->setStartValue(bar->value());
        m_scrollAnim->setEndValue(target);
        m_scrollAnim->start();
        return true; // 拦截默认滚动，同时也阻止了 QTabBar 用滚轮切页
    }
    return QDialog::eventFilter(obj, ev);
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

    m_refreshButton = new ElaPushButton("刷新", bar);
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
                if (!m_reloading) scheduleReload();
            });
    connect(m_recipeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (!m_reloading) scheduleReload();
            });
    connect(m_customFrom, &QDateEdit::dateChanged, this, [this](const QDate &) {
        if (m_rangeCombo && m_rangeCombo->currentIndex() == RangeCustom && !m_reloading) {
            scheduleReload();
        }
    });
    connect(m_customTo, &QDateEdit::dateChanged, this, [this](const QDate &) {
        if (m_rangeCombo && m_rangeCombo->currentIndex() == RangeCustom && !m_reloading) {
            scheduleReload();
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
    auto *panel = createPanel("检测任务列表", nullptr, this);
    auto *layout = qobject_cast<QVBoxLayout *>(panel->layout());

    m_tableStack = new QStackedWidget(panel);

    m_table = new QTableWidget(m_tableStack);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels({
        "编号", "配方", "加工开始", "加工结束",
        "检测开始", "检测结束", "颗粒", "已清", "峰高(mm)"
    });
    setupCompactTable(m_table);

    // 固定列宽 —— 前 8 列固定，最后一列 stretch 兜住剩余空间
    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setStretchLastSection(true);
    for (int i = 0; i < 8; ++i) {
        m_table->setColumnWidth(i, kColWidths[i]);
    }
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

    if (layout) {
        layout->addWidget(m_tableStack, 1);
    }

    connect(m_table, &QTableWidget::cellClicked,
            this, &HistoryDialog::onRowClicked);

    return panel;
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

void HistoryDialog::scheduleReload()
{
    if (m_reloadTimer) {
        m_reloadTimer->start();
    } else {
        reloadRecords();
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

    // 加载新数据前，先立即收起展开行 + 停掉正在跑的滚动/展开动画，
    // 避免动画回调作用在错误的行上。
    if (m_expandAnim && m_expandAnim->state() == QAbstractAnimation::Running) {
        m_expandAnim->stop();
    }
    if (m_expandAnimFinishedConn) {
        QObject::disconnect(m_expandAnimFinishedConn);
        m_expandAnimFinishedConn = {};
    }
    if (m_scrollAnim && m_scrollAnim->state() == QAbstractAnimation::Running) {
        m_scrollAnim->stop();
    }
    if (m_expandedRow >= 0 && m_table && m_expandedRow < m_table->rowCount()) {
        m_table->removeCellWidget(m_expandedRow, 0);
        m_table->removeRow(m_expandedRow);
    }
    if (m_table) m_table->clearSpans();
    m_expandedRow      = -1;
    m_expandedRecordId = -1;
    m_expandedWidget   = nullptr;
    m_animating        = false;

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
                                   : QStringLiteral("共 %1 条记录，点击行可展开详情").arg(rows.size()));
    }

    // 恢复选中和滚动条位置（按 recordId 找回；找不到就退回滚动位置）
    if (m_table) {
        int restoreRow = (savedSelectedId > 0) ? findRowByRecordId(savedSelectedId) : -1;
        if (restoreRow >= 0) {
            m_table->selectRow(restoreRow);
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
        m_table->setItem(i, col++, new QTableWidgetItem(formatDateTime(r.processStartAt)));
        m_table->setItem(i, col++, new QTableWidgetItem(formatDateTime(r.processEndAt)));
        m_table->setItem(i, col++, new QTableWidgetItem(formatDateTime(r.inspectStartAt)));
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
// 行内展开详情（带动画）
// -----------------------------------------------------------------

void HistoryDialog::onRowClicked(int row, int /*column*/)
{
    if (!m_table || row < 0 || row >= m_table->rowCount()) {
        return;
    }
    if (m_animating) {
        return; // 动画进行中不响应
    }
    // 忽略点击到内嵌 detail 行（那行没有 id item）
    auto *item = m_table->item(row, 0);
    if (!item) {
        return;
    }
    const int recordId = item->data(Qt::UserRole).toInt();
    if (recordId <= 0) {
        return;
    }
    toggleDetailRow(row);
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

void HistoryDialog::scrollRowIntoView(int row)
{
    if (!m_table || row < 0 || row >= m_table->rowCount()) {
        return;
    }
    // 让展开行自身也进入视口：把 detailRow 也一并 scrollTo
    m_table->scrollTo(m_table->model()->index(row, 0),
                      QAbstractItemView::EnsureVisible);
    if (m_expandedRow >= 0 && m_expandedRow < m_table->rowCount()
        && m_expandedRow != row) {
        m_table->scrollTo(m_table->model()->index(m_expandedRow, 0),
                          QAbstractItemView::EnsureVisible);
    }
}

// 递归把事件过滤器装到 root 及其所有子控件上（含子控件的 viewport 等），
// 目的是"抢走"所有 Wheel 事件，转发给主表统一做平滑滚动，
// 同时顺便屏蔽 QTabBar 因滚轮切换 tab 的默认行为。
void HistoryDialog::installWheelForwarderRecursive(QWidget *root)
{
    if (!root) return;
    root->installEventFilter(this);
    // 有 viewport 的 QAbstractScrollArea（QTableWidget/QScrollArea 等）
    if (auto *sa = qobject_cast<QAbstractScrollArea *>(root)) {
        if (auto *vp = sa->viewport()) {
            vp->installEventFilter(this);
        }
    }
    const auto children = root->findChildren<QWidget *>();
    for (QWidget *w : children) {
        w->installEventFilter(this);
        if (auto *sa = qobject_cast<QAbstractScrollArea *>(w)) {
            if (auto *vp = sa->viewport()) {
                vp->installEventFilter(this);
            }
        }
    }
}

void HistoryDialog::toggleDetailRow(int row)
{
    if (!m_table || !m_expandAnim) {
        return;
    }

    auto *item = m_table->item(row, 0);
    if (!item) {
        return;
    }
    const int recordId = item->data(Qt::UserRole).toInt();

    // 点击的正是当前展开的记录 → 收起（带动画）
    if (m_expandedRow >= 0 && recordId == m_expandedRecordId) {
        collapseDetail();
        return;
    }

    // 已有展开行且是别的记录 → 先带动画收起，收完再展开新行。
    // 用 recordId 定位新行，避免 row 因为旧展开行被移除而错位。
    if (m_expandedRow >= 0) {
        collapseDetail([this, recordId]() {
            const int newRow = findRowByRecordId(recordId);
            if (newRow >= 0) {
                expandDetailRow(newRow, recordId);
            }
        });
        return;
    }

    expandDetailRow(row, recordId);
}

void HistoryDialog::expandDetailRow(int row, int recordId)
{
    if (!m_table || !m_expandAnim || row < 0 || row >= m_table->rowCount()) {
        return;
    }

    // 展开新行：插入一个 span 单元格，起始高度 0，动画到"内容自然高度"
    QWidget *detail = createDetailWidget(recordId);
    if (!detail) {
        return;
    }

    // 让详情按内容自然撑开：设置合理宽度后再取 sizeHint().height()
    const int detailWidth = m_table->viewport()->width();
    detail->setFixedWidth(detailWidth);
    detail->adjustSize();
    const int naturalHeight = qMax(kExpandedRowMinHeight,
                                   detail->sizeHint().height());

    const int detailRow = row + 1;
    m_table->insertRow(detailRow);
    m_table->setRowHeight(detailRow, 0);                     // 起始高度 0
    m_table->setSpan(detailRow, 0, 1, m_table->columnCount());
    m_table->setCellWidget(detailRow, 0, detail);

    // 详情区里所有子控件（QTabBar / 内嵌表 / 其 viewport 等）都要把 wheel
    // 转发给主表，否则鼠标停在详情上滚动会：(1) 主表不滚，(2) QTabBar 会切页。
    installWheelForwarderRecursive(detail);

    m_expandedRow      = detailRow;
    m_expandedRecordId = recordId;
    m_expandedWidget   = detail;

    // 让用户明确知道展开的是哪一行
    m_table->selectRow(row);

    m_animating = true;
    if (m_expandAnimFinishedConn) {
        QObject::disconnect(m_expandAnimFinishedConn);
        m_expandAnimFinishedConn = {};
    }
    const int expectedRecordId = recordId;
    m_expandAnimFinishedConn = connect(m_expandAnim, &QVariantAnimation::finished, this,
        [this, expectedRecordId]() {
            // 动画结束后再断掉自己
            if (m_expandAnimFinishedConn) {
                QObject::disconnect(m_expandAnimFinishedConn);
                m_expandAnimFinishedConn = {};
            }
            // 若中途数据被刷新或换行，就不要再改状态了
            if (m_expandedRecordId != expectedRecordId) {
                m_animating = false;
                return;
            }
            m_animating = false;
            scrollRowIntoView(m_expandedRow);
        }, Qt::QueuedConnection);
    m_expandAnim->stop();
    m_expandAnim->setStartValue(0);
    m_expandAnim->setEndValue(naturalHeight);
    m_expandAnim->start();
}

void HistoryDialog::collapseDetail(std::function<void()> after)
{
    if (!m_table || m_expandedRow < 0) {
        m_expandedRow      = -1;
        m_expandedRecordId = -1;
        m_expandedWidget   = nullptr;
        if (after) after();
        return;
    }
    if (!m_expandAnim) {
        // 无动画对象兜底
        if (m_expandedRow < m_table->rowCount()) {
            m_table->removeCellWidget(m_expandedRow, 0);
            m_table->removeRow(m_expandedRow);
        }
        m_table->clearSpans();
        m_expandedRow      = -1;
        m_expandedRecordId = -1;
        m_expandedWidget   = nullptr;
        if (after) after();
        return;
    }

    m_animating = true;
    if (m_expandAnimFinishedConn) {
        QObject::disconnect(m_expandAnimFinishedConn);
        m_expandAnimFinishedConn = {};
    }
    const int collapsingRecordId = m_expandedRecordId;
    m_expandAnimFinishedConn = connect(m_expandAnim, &QVariantAnimation::finished, this,
        [this, collapsingRecordId, after = std::move(after)]() {
            if (m_expandAnimFinishedConn) {
                QObject::disconnect(m_expandAnimFinishedConn);
                m_expandAnimFinishedConn = {};
            }
            // 中途状态已被别人重置（例如 reload），就只清动画标志即可
            if (m_expandedRecordId != collapsingRecordId) {
                m_animating = false;
                if (after) after();
                return;
            }
            if (m_expandedRow >= 0 && m_expandedRow < m_table->rowCount()) {
                m_table->removeCellWidget(m_expandedRow, 0);
                m_table->removeRow(m_expandedRow);
            }
            m_table->clearSpans();
            m_expandedRow      = -1;
            m_expandedRecordId = -1;
            m_expandedWidget   = nullptr;
            m_animating        = false;
            if (after) after();
        }, Qt::QueuedConnection);

    m_expandAnim->stop();
    m_expandAnim->setStartValue(m_table->rowHeight(m_expandedRow));
    m_expandAnim->setEndValue(0);
    m_expandAnim->start();
}

QWidget *HistoryDialog::createDetailWidget(int recordId)
{
    QString err;
    const InspectionDetail detail = InspectionRepository::loadDetail(recordId, &err);

    auto *host = new QWidget();
    host->setAutoFillBackground(true);
    host->setStyleSheet("background:#131E28;");

    auto *hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(12, 10, 12, 12);
    hostLayout->setSpacing(6);

    if (!detail.loaded) {
        auto *msg = makeLabel(err.isEmpty() ? QStringLiteral("详情加载失败")
                                            : QStringLiteral("详情加载失败：%1").arg(err),
                              "sectionHint");
        hostLayout->addWidget(msg);
        hostLayout->addStretch();
        return host;
    }

    const auto &h = detail.head;

    auto *title = makeLabel(QStringLiteral("任务 #%1 · %2")
                                .arg(h.id)
                                .arg(h.recipeName.isEmpty() ? QStringLiteral("未命名配方")
                                                            : h.recipeName),
                            "panelTitle");
    hostLayout->addWidget(title);

    auto *meta = makeLabel(QStringLiteral("创建时间：%1  |  加工：%2 — %3  |  检测：%4 — %5")
                               .arg(formatDateTime(h.createdAt))
                               .arg(formatDateTime(h.processStartAt))
                               .arg(formatDateTime(h.processEndAt))
                               .arg(formatDateTime(h.inspectStartAt))
                               .arg(formatDateTime(h.inspectEndAt)),
                           "sectionHint");
    meta->setWordWrap(true);
    hostLayout->addWidget(meta);

    const double rate = (h.particleCount > 0)
                            ? 100.0 * double(h.clearedCount) / double(h.particleCount)
                            : 0.0;
    auto *kpi = makeLabel(QStringLiteral("颗粒数 %1  ·  已清除 %2  ·  清除率 %3%%  ·  峰值高度 %4 mm")
                              .arg(h.particleCount)
                              .arg(h.clearedCount)
                              .arg(QString::number(rate, 'f', 1))
                              .arg(QString::number(h.maxParticleHeight, 'f', 3)),
                          "metricValue");
    hostLayout->addWidget(kpi);

    QStringList overviewParts;
    if (!h.remark.isEmpty()) {
        overviewParts << QStringLiteral("备注：%1").arg(h.remark);
    }
    overviewParts << QStringLiteral("总览图：%1")
                         .arg(h.overviewImagePath.isEmpty() ? QStringLiteral("无")
                                                            : h.overviewImagePath);
    auto *overview = makeLabel(overviewParts.join(QStringLiteral("    ")), "sectionHint");
    overview->setWordWrap(true);
    hostLayout->addWidget(overview);

    auto *tabs = new QTabWidget(host);
    tabs->setDocumentMode(true);

    // 颗粒明细
    auto *particleTable = new QTableWidget(tabs);
    particleTable->setColumnCount(8);
    particleTable->setHorizontalHeaderLabels({
        "序号", "X", "Y", "Z", "高度(mm)", "已清除", "检测时间", "清除时间"
    });
    setupNoScrollTable(particleTable);
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
    particleTable->setFixedHeight(tableContentHeight(particleTable));

    // 工艺快照
    auto *processTable = new QTableWidget(tabs);
    processTable->setColumnCount(11);
    processTable->setHorizontalHeaderLabels({
        "路径", "峰高(mm)", "TargetX", "TargetY", "TargetZ",
        "进给速度", "进给量", "主轴转速", "切次", "单切量", "快照时间"
    });
    setupNoScrollTable(processTable);
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
    processTable->setFixedHeight(tableContentHeight(processTable));

    // Tab 栏本身也不允许滚动：按内容自然撑开
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    tabs->addTab(particleTable, QStringLiteral("颗粒明细 (%1)").arg(detail.particles.size()));
    tabs->addTab(processTable,  QStringLiteral("工艺快照 (%1)").arg(detail.processHistory.size()));
    hostLayout->addWidget(tabs, 1);

    return host;
}
