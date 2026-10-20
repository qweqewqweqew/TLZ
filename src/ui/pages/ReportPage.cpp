#include "ui/pages/ReportPage.h"

#include "ElaComboBox.h"
#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "data/InspectionRepository.h"
#include "qcustomplot.h"

#include <QDateEdit>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPrinter>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>

class HeightDistributionWidget : public QWidget
{
public:
    explicit HeightDistributionWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(190);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }

    void setValues(const QList<int> &values)
    {
        m_values = values;
        while (m_values.size() < 4) {
            m_values.append(0);
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(Theme::kPanelBg));

        const QStringList labels{QStringLiteral("≤ 1 mm"), QStringLiteral("1 - 3 mm"),
                                 QStringLiteral("3 - 5 mm"), QStringLiteral("> 5 mm")};
        const QList<QColor> colors{QColor(Theme::kStateRunning), QColor(Theme::kStateSuccess),
                                   QColor(Theme::kStateWarning), QColor(Theme::kStateError)};
        int total = 0;
        for (int value : m_values) {
            total += qMax(0, value);
        }

        const int legendWidth = qMin(190, qMax(155, width() / 3));
        const int side = qMax(72, qMin(height() - 26, width() - legendWidth - 24));
        const QRectF pieRect(18, (height() - side) / 2.0, side, side);
        if (total <= 0) {
            painter.setPen(QPen(QColor(Theme::kDivider), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(pieRect);
            painter.setPen(QColor(Theme::kTextMuted));
            painter.drawText(pieRect, Qt::AlignCenter, QStringLiteral("暂无数据"));
        } else {
            int start = 90 * 16;
            for (int i = 0; i < 4; ++i) {
                const int value = qMax(0, m_values.at(i));
                if (value <= 0) {
                    continue;
                }
                const int span = qRound(360.0 * 16.0 * value / total);
                painter.setPen(QPen(QColor(Theme::kPanelBg), 2));
                painter.setBrush(colors.at(i));
                painter.drawPie(pieRect, start, -span);
                start -= span;
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(Theme::kPanelBg));
            painter.drawEllipse(pieRect.center(), side * 0.22, side * 0.22);
            painter.setPen(QColor(Theme::kTextPrimary));
            painter.drawText(QRectF(pieRect.center().x() - side * 0.2,
                                    pieRect.center().y() - 10,
                                    side * 0.4,
                                    20),
                            Qt::AlignCenter,
                            QString::number(total));
        }

        QFont legendFont = painter.font();
        legendFont.setPointSizeF(9.0);
        painter.setFont(legendFont);
        const int legendX = int(pieRect.right()) + 20;
        const int rowHeight = qMax(27, height() / 5);
        for (int i = 0; i < 4; ++i) {
            const int y = 18 + i * rowHeight;
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.at(i));
            painter.drawRoundedRect(QRectF(legendX, y + 4, 9, 9), 2, 2);
            painter.setPen(QColor(Theme::kTextSecondary));
            painter.drawText(QRectF(legendX + 17, y, legendWidth - 72, 18), labels.at(i));
            const int value = i < m_values.size() ? qMax(0, m_values.at(i)) : 0;
            const QString percentage = total > 0 ? QStringLiteral("%1%").arg(100.0 * value / total, 0, 'f', 1)
                                                  : QStringLiteral("--");
            painter.setPen(QColor(Theme::kTextPrimary));
            painter.drawText(QRectF(width() - 62, y, 56, 18), Qt::AlignRight, percentage);
            painter.setPen(QColor(Theme::kTextMuted));
            painter.drawText(QRectF(legendX + 17, y + 16, legendWidth - 72, 16),
                             QStringLiteral("%1 个").arg(value));
        }
    }

private:
    QList<int> m_values{0, 0, 0, 0};
};

namespace {

enum RangeIndex {
    RangeToday = 0,
    Range7Days,
    Range30Days,
    RangeAll,
    RangeCustom,
};

constexpr int kMaxRows = 200000;
constexpr int kDailyPageSize = 8;
constexpr int kDemoMinimumRows = 36;

const char *chartBarColor()
{
    if (Theme::current() == Theme::Id::Dark) {
        return Theme::kAccentFill;
    }
    return Theme::kTitle;
}
constexpr double kTargetCycleSeconds = 20.0; // 招标节拍目标 20 s/片

QString percentText(int numerator, int denominator)
{
    if (denominator <= 0) {
        return QStringLiteral("--");
    }
    return QString::number(100.0 * numerator / denominator, 'f', 1);
}

bool isDemoRow(const InspectionRecordRow &row)
{
    return row.id <= -100000;
}

QList<InspectionRecordRow> makeDemoRows(int count, const QDate &from, const QDate &to)
{
    QList<InspectionRecordRow> demo;
    if (count <= 0) {
        return demo;
    }

    const QDate rangeEnd = to.isValid() ? to : QDate::currentDate();
    QDate rangeStart = from.isValid() ? from : rangeEnd.addDays(-13);
    if (rangeStart > rangeEnd) {
        rangeStart = rangeEnd;
    }
    if (rangeStart.daysTo(rangeEnd) > 13) {
        rangeStart = rangeEnd.addDays(-13);
    }
    const int dayCount = qMax(1, static_cast<int>(rangeStart.daysTo(rangeEnd) + 1));
    demo.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int pattern = i % 12;
        const QDate date = rangeStart.addDays(i % dayCount);
        const QTime createdTime(7 + (i % 10), (i * 11) % 60, (i * 7) % 60);
        const QDateTime created(date, createdTime, Qt::LocalTime);

        InspectionRecordRow row;
        row.id = -100000 - i;
        row.backendTaskId = 900000 + static_cast<quint64>(i);
        row.recipeName = QStringLiteral("演示报表数据");
        row.createdAt = created;
        row.inspectStartAt = created.addSecs(18);
        row.inspectEndAt = created.addSecs(26);
        row.processStartAt = created.addSecs(45);
        row.processEndAt = row.processStartAt.addSecs(12 + (i * 7) % 17);
        row.particleCount = pattern == 4 || pattern == 9 ? 0 : 2 + (i % 6);
        if (row.particleCount == 0) {
            row.clearedCount = 0;
            row.maxParticleHeight = 0.0;
        } else if (pattern == 0 || pattern == 1 || pattern == 2 || pattern == 7) {
            row.clearedCount = qMax(0, row.particleCount - 1 - (i % 2));
            row.maxParticleHeight = pattern == 0 || pattern == 1 ? 5.8 : 3.5;
        } else {
            row.clearedCount = row.particleCount;
            row.maxParticleHeight = pattern % 4 == 0 ? 0.6 : pattern % 4 == 1 ? 1.8 : pattern % 4 == 2 ? 3.8 : 5.8;
        }
        row.remark = QStringLiteral("界面预览用演示数据");
        demo.append(row);
    }
    return demo;
}

} // namespace

ReportPage::ReportPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("reportPage"));
    setStyleSheet(QStringLiteral(
                      "QWidget#reportPage { background:%1; }"
                      "QFrame#controlBar { background:%2; border:1px solid %4; border-radius:6px; }"
                      "QTableWidget#reportTable {"
                      "  background:%2; color:%3; gridline-color:%4; border:none;"
                      "  alternate-background-color:%5; selection-background-color:%6; selection-color:%3;"
                      "}"
                      "QTableWidget#reportTable::item { padding:4px 8px; }"
                      "QHeaderView::section {"
                      "  background:%1; color:%7; padding:6px 10px; border:none;"
                      "  border-bottom:1px solid %4; font-weight:600;"
                      "}"
                      "QDateEdit, QComboBox, QLineEdit { background:%2; color:%3; border:1px solid %4; border-radius:5px;"
                      "  padding:2px 6px; min-height:26px; }"
                      "QDateEdit:focus, QComboBox:focus, QLineEdit:focus { border:1px solid %6; }"
                      "QDateEdit:disabled, QComboBox:disabled, QLineEdit:disabled { color:%8; }"
                      "QDateEdit::drop-down { subcontrol-origin:padding; subcontrol-position:right; width:18px;"
                      "  border-left:1px solid %4; }"
                      "QLineEdit { padding-left:8px; padding-right:8px; }")
                      .arg(Theme::kWindowBg,
                           Theme::kPanelBg,
                           Theme::kTextPrimary,
                           Theme::kBorder,
                           Theme::kHoverBg,
                           Theme::kSelectionBg,
                           Theme::kTextSecondary,
                           Theme::kTextMuted));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);
    root->addWidget(buildFilterBar());
    root->addWidget(buildKpiRow());
    root->addWidget(buildChartsRow(), 5);
    root->addWidget(buildDailyTable(), 4);
    applyTheme();
    Theme::watch(this, [this]() { applyTheme(); });
}

QWidget *ReportPage::buildFilterBar()
{
    auto *bar = new QFrame(this);
    bar->setObjectName("controlBar");
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);

    layout->addWidget(makeLabel(QStringLiteral("时间范围"), QStringLiteral("sectionHint")));
    m_rangeCombo = new ElaComboBox(bar);
    m_rangeCombo->addItems({QStringLiteral("今天"),
                            QStringLiteral("最近 7 天"),
                            QStringLiteral("最近 30 天"),
                            QStringLiteral("全部"),
                            QStringLiteral("自定义")});
    m_rangeCombo->setCurrentIndex(Range7Days);
    m_rangeCombo->setMinimumWidth(130);
    layout->addWidget(m_rangeCombo);

    layout->addWidget(makeLabel(QStringLiteral("班次"), QStringLiteral("sectionHint")));
    m_shiftCombo = new ElaComboBox(bar);
    m_shiftCombo->addItems({QStringLiteral("全部班次"), QStringLiteral("甲班"), QStringLiteral("乙班"), QStringLiteral("丙班")});
    m_shiftCombo->setMinimumWidth(106);
    m_shiftCombo->setToolTip(QStringLiteral("班次字段待后端补齐，当前仅保留界面入口"));
    layout->addWidget(m_shiftCombo);

    layout->addWidget(makeLabel(QStringLiteral("板号"), QStringLiteral("sectionHint")));
    m_plateEdit = new QLineEdit(bar);
    m_plateEdit->setPlaceholderText(QStringLiteral("输入铜板编号"));
    m_plateEdit->setFixedWidth(132);
    m_plateEdit->setToolTip(QStringLiteral("板号字段待后端补齐，当前不参与查询"));
    layout->addWidget(m_plateEdit);

    const QDate today = QDate::currentDate();
    m_from = new QDateEdit(today.addDays(-6), bar);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy/MM/dd"));
    m_from->setFixedWidth(120);
    layout->addWidget(m_from);
    layout->addWidget(makeLabel(QStringLiteral("至"), QStringLiteral("sectionHint")));
    m_to = new QDateEdit(today, bar);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy/MM/dd"));
    m_to->setFixedWidth(120);
    layout->addWidget(m_to);

    m_queryButton = new ElaPushButton(QStringLiteral("查询"), bar);
    m_queryButton->setFixedSize(84, 32);
    applyPrimaryButtonStyle(m_queryButton);
    layout->addWidget(m_queryButton);
    m_resetButton = new ElaPushButton(QStringLiteral("重置"), bar);
    m_resetButton->setFixedSize(72, 32);
    applySecondaryButtonStyle(m_resetButton);
    layout->addWidget(m_resetButton);

    m_status = makeLabel(QString(), QStringLiteral("sectionHint"));
    layout->addSpacing(8);
    layout->addWidget(m_status, 1);

    m_filterHint = makeLabel(QStringLiteral("班次、板号暂为前端占位"), QStringLiteral("sectionHint"));
    m_filterHint->setToolTip(QStringLiteral("当前数据库没有班次和板号字段，查询暂按时间范围执行"));
    layout->addWidget(m_filterHint);

    m_excelButton = new ElaPushButton(QStringLiteral("导出 Excel"), bar);
    m_excelButton->setFixedSize(104, 32);
    applySecondaryButtonStyle(m_excelButton);
    m_excelButton->setEnabled(false);
    m_excelButton->setToolTip(QStringLiteral("等待接入 QXlsx 库后开放"));
    layout->addWidget(m_excelButton);
    m_pdfButton = new ElaPushButton(QStringLiteral("导出 PDF"), bar);
    m_pdfButton->setFixedSize(96, 32);
    applySecondaryButtonStyle(m_pdfButton);
    layout->addWidget(m_pdfButton);

    const auto syncCustom = [this]() {
        const bool custom = m_rangeCombo->currentIndex() == RangeCustom;
        m_from->setEnabled(custom);
        m_to->setEnabled(custom);
    };
    connect(m_rangeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [syncCustom](int) {
        syncCustom();
    });
    connect(m_queryButton, &ElaPushButton::clicked, this, &ReportPage::reload);
    connect(m_resetButton, &ElaPushButton::clicked, this, [this, today]() {
        m_rangeCombo->setCurrentIndex(Range7Days);
        m_from->setDate(today.addDays(-6));
        m_to->setDate(today);
        reload();
    });
    connect(m_excelButton, &ElaPushButton::clicked, this, &ReportPage::exportExcel);
    connect(m_pdfButton, &ElaPushButton::clicked, this, &ReportPage::exportPdf);
    syncCustom();
    return bar;
}

QWidget *ReportPage::makeKpiTile(const QString &title, const QString &unit, const QString &iconResource,
                                 const QString &accent,
                                 QLabel *&valueOut)
{
    auto *tile = new QFrame(this);
    tile->setObjectName("panel");
    tile->setMinimumHeight(94);
    auto *layout = new QHBoxLayout(tile);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto *texts = new QVBoxLayout();
    texts->setContentsMargins(0, 0, 0, 0);
    texts->setSpacing(3);
    auto *caption = makeLabel(title);
    caption->setStyleSheet(QStringLiteral("color:%1;font-size:13px;border:none;background:transparent;")
                               .arg(Theme::kTextSecondary));
    texts->addWidget(caption);

    auto *valueRow = new QHBoxLayout();
    valueRow->setContentsMargins(0, 0, 0, 0);
    valueRow->setSpacing(6);
    valueOut = makeLabel(QStringLiteral("--"));
    valueOut->setStyleSheet(QStringLiteral("color:%1;font-size:30px;font-weight:700;border:none;background:transparent;")
                                .arg(accent));
    valueRow->addWidget(valueOut, 0, Qt::AlignBottom);
    auto *unitLabel = makeLabel(unit);
    unitLabel->setStyleSheet(QStringLiteral("color:%1;font-size:13px;border:none;background:transparent;padding-bottom:6px;")
                                 .arg(Theme::kTextMuted));
    valueRow->addWidget(unitLabel, 0, Qt::AlignBottom);
    valueRow->addStretch(1);
    texts->addLayout(valueRow);
    layout->addLayout(texts, 1);

    auto *icon = new QLabel(tile);
    icon->setFixedSize(40, 40);
    icon->setAlignment(Qt::AlignCenter);
    icon->setProperty("iconResource", iconResource);
    icon->setProperty("kpiAccent", accent);
    icon->setPixmap(tintedIcon(iconResource, QColor(accent), 22, devicePixelRatioF()));
    Theme::bindStyle(icon, []() {
        return QStringLiteral("background:%1;border:none;border-radius:20px;").arg(Theme::kControlBg);
    });
    layout->addWidget(icon, 0, Qt::AlignVCenter);
    return tile;
}

QWidget *ReportPage::buildKpiRow()
{
    auto *host = new QWidget(this);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(makeKpiTile(QStringLiteral("检测铜板"), QStringLiteral("张"),
                                  QStringLiteral(":/img/kpi_target.svg"), Theme::kAccent, m_kpiBoards));
    layout->addWidget(makeKpiTile(QStringLiteral("有粒子修磨铜板"), QStringLiteral("张"),
                                  QStringLiteral(":/img/kpi_alert.svg"), Theme::kStateWarning, m_kpiParticleBoards));
    layout->addWidget(makeKpiTile(QStringLiteral("修磨完成率"), QStringLiteral("%"),
                                  QStringLiteral(":/img/kpi_check.svg"), Theme::kStateSuccess, m_kpiCompletion));
    layout->addWidget(makeKpiTile(QStringLiteral("平均修磨节拍"), QStringLiteral("s/张"),
                                  QStringLiteral(":/img/kpi_clock.svg"), Theme::kStateError, m_kpiCycle));
    return host;
}

void ReportPage::styleAxes(QCustomPlot *plot)
{
    plot->setBackground(QColor(Theme::kPanelBg));
    plot->axisRect()->setBackground(QColor(Theme::kPanelBg));
    const QPen base{QColor(Theme::kBorder)};
    const QPen grid{QColor(Theme::kDivider), 1, Qt::DotLine};
    for (QCPAxis *axis : {plot->xAxis, plot->yAxis}) {
        axis->setBasePen(base);
        axis->setTickPen(Qt::NoPen);
        axis->setSubTickPen(Qt::NoPen);
        axis->setTickLabelColor(QColor(Theme::kTextSecondary));
        axis->setLabelColor(QColor(Theme::kTextSecondary));
        axis->grid()->setPen(grid);
        axis->grid()->setSubGridVisible(false);
        axis->grid()->setZeroLinePen(Qt::NoPen);
    }
    plot->xAxis->grid()->setVisible(false);
    plot->setInteractions(QCP::Interactions());
}

QWidget *ReportPage::buildChartsRow()
{
    auto *host = new QWidget(this);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_dailyCaption = makeLabel(QString(), QStringLiteral("sectionHint"));
    auto *dailyPanel = createPanel(QStringLiteral("每日铜板检测与修磨"), m_dailyCaption, host);
    m_dailyPlot = new QCustomPlot(dailyPanel);
    m_dailyPlot->setMinimumHeight(200);
    styleAxes(m_dailyPlot);
    m_dailyBars = new QCPBars(m_dailyPlot->xAxis, m_dailyPlot->yAxis);
    m_dailyBars->setPen(Qt::NoPen);
    m_dailyBars->setBrush(QColor(chartBarColor()));
    m_dailyBars->setWidth(0.6);
    m_dailyLine = m_dailyPlot->addGraph();
    m_dailyLine->setPen(QPen(QColor(Theme::kStateSuccess), 2));
    m_dailyLine->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,
                                                 QPen(QColor(Theme::kStateSuccess), 2),
                                                 QBrush(QColor(Theme::kPanelBg)),
                                                 7));
    m_dailyPlot->legend->setVisible(true);
    m_dailyPlot->legend->setBrush(QBrush(Qt::NoBrush));
    m_dailyPlot->legend->setBorderPen(Qt::NoPen);
    m_dailyPlot->legend->setTextColor(QColor(Theme::kTextSecondary));
    m_dailyPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
    m_dailyBars->setName(QStringLiteral("检测铜板"));
    m_dailyLine->setName(QStringLiteral("修磨完成"));
    qobject_cast<QVBoxLayout *>(dailyPanel->layout())->addWidget(m_dailyPlot, 1);
    layout->addWidget(dailyPanel, 3);

    m_heightCaption = makeLabel(QString(), QStringLiteral("sectionHint"));
    auto *heightPanel = createPanel(QStringLiteral("粒子高度占比"), m_heightCaption, host);
    m_heightPie = new HeightDistributionWidget(heightPanel);
    qobject_cast<QVBoxLayout *>(heightPanel->layout())->addWidget(m_heightPie, 1);
    layout->addWidget(heightPanel, 1);
    return host;
}

QWidget *ReportPage::buildDailyTable()
{
    auto *host = new QWidget(this);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_resultCaption = makeLabel(QString(), QStringLiteral("sectionHint"));
    auto *resultPanel = createPanel(QStringLiteral("修磨任务结果"), m_resultCaption, host);
    m_resultPlot = new QCustomPlot(resultPanel);
    m_resultPlot->setMinimumHeight(190);
    styleAxes(m_resultPlot);
    const QList<QColor> resultColors{QColor(Theme::kStateSuccess), QColor(Theme::kStateWarning),
                                     QColor(Theme::kStateError), QColor(Theme::kAccent),
                                     QColor(Theme::kStateRunning)};
    for (const QColor &color : resultColors) {
        auto *bars = new QCPBars(m_resultPlot->yAxis, m_resultPlot->xAxis);
        bars->setPen(Qt::NoPen);
        bars->setBrush(color);
        bars->setWidth(0.55);
        m_resultCategoryBars.append(bars);
    }
    m_resultBars = m_resultCategoryBars.value(0, nullptr);
    m_resultPlot->xAxis->grid()->setVisible(true);
    m_resultPlot->yAxis->grid()->setVisible(false);
    qobject_cast<QVBoxLayout *>(resultPanel->layout())->addWidget(m_resultPlot, 1);
    layout->addWidget(resultPanel, 2);

    m_tableCaption = makeLabel(QString(), QStringLiteral("sectionHint"));
    auto *tablePanel = createPanel(QStringLiteral("每日班次汇总"), m_tableCaption, host);
    m_dailyTable = new QTableWidget(tablePanel);
    m_dailyTable->setObjectName(QStringLiteral("reportTable"));
    m_dailyTable->setColumnCount(6);
    m_dailyTable->setHorizontalHeaderLabels({QStringLiteral("日期"),
                                             QStringLiteral("甲班"),
                                             QStringLiteral("乙班"),
                                             QStringLiteral("丙班"),
                                             QStringLiteral("检测合计"),
                                             QStringLiteral("修磨合计")});
    m_dailyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dailyTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_dailyTable->setAlternatingRowColors(true);
    m_dailyTable->setShowGrid(false);
    m_dailyTable->setFrameShape(QFrame::NoFrame);
    m_dailyTable->verticalHeader()->setVisible(false);
    m_dailyTable->verticalHeader()->setDefaultSectionSize(32);
    m_dailyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_dailyTable->horizontalHeader()->setHighlightSections(false);
    qobject_cast<QVBoxLayout *>(tablePanel->layout())->addWidget(m_dailyTable, 1);

    auto *pager = new QHBoxLayout();
    pager->setContentsMargins(0, 0, 0, 0);
    pager->setSpacing(8);
    m_dailyPageLabel = makeLabel(QStringLiteral("共 0 条 · 每页 8 条"), QStringLiteral("sectionHint"));
    m_dailyPrev = new ElaPushButton(QStringLiteral("上一页"), tablePanel);
    m_dailyNext = new ElaPushButton(QStringLiteral("下一页"), tablePanel);
    m_dailyPrev->setFixedHeight(28);
    m_dailyNext->setFixedHeight(28);
    applySecondaryButtonStyle(m_dailyPrev);
    applySecondaryButtonStyle(m_dailyNext);
    pager->addWidget(m_dailyPageLabel);
    pager->addStretch(1);
    pager->addWidget(m_dailyPrev);
    pager->addWidget(m_dailyNext);
    qobject_cast<QVBoxLayout *>(tablePanel->layout())->addLayout(pager);
    connect(m_dailyPrev, &ElaPushButton::clicked, this, [this]() {
        if (m_dailyPage > 0) {
            --m_dailyPage;
            renderDailyPage();
        }
    });
    connect(m_dailyNext, &ElaPushButton::clicked, this, [this]() {
        const int pages = qMax(1, (m_dailyRows.size() + kDailyPageSize - 1) / kDailyPageSize);
        if (m_dailyPage + 1 < pages) {
            ++m_dailyPage;
            renderDailyPage();
        }
    });
    layout->addWidget(tablePanel, 3);
    return host;
}

void ReportPage::applyTheme()
{
    setStyleSheet(QStringLiteral(
                      "QWidget#reportPage { background:%1; }"
                      "QFrame#controlBar { background:%2; border:1px solid %4; border-radius:6px; }"
                      "QTableWidget#reportTable {"
                      "  background:%2; color:%3; gridline-color:%4; border:none;"
                      "  alternate-background-color:%5; selection-background-color:%6; selection-color:%3;"
                      "}"
                      "QTableWidget#reportTable::item { padding:4px 8px; }"
                      "QHeaderView::section {"
                      "  background:%1; color:%7; padding:6px 10px; border:none;"
                      "  border-bottom:1px solid %4; font-weight:600;"
                      "}"
                      "QDateEdit, QComboBox, QLineEdit { background:%2; color:%3; border:1px solid %4; border-radius:5px;"
                      "  padding:2px 6px; min-height:26px; }"
                      "QDateEdit:focus, QComboBox:focus, QLineEdit:focus { border:1px solid %6; }"
                      "QDateEdit:disabled, QComboBox:disabled, QLineEdit:disabled { color:%8; }"
                      "QDateEdit::drop-down { subcontrol-origin:padding; subcontrol-position:right; width:18px;"
                      "  border-left:1px solid %4; }"
                      "QLineEdit { padding-left:8px; padding-right:8px; }")
                      .arg(Theme::kWindowBg,
                           Theme::kPanelBg,
                           Theme::kTextPrimary,
                           Theme::kBorder,
                           Theme::kHoverBg,
                           Theme::kSelectionBg,
                           Theme::kTextSecondary,
                           Theme::kTextMuted));
    if (m_dailyPlot) {
        styleAxes(m_dailyPlot);
        if (m_dailyBars) {
            m_dailyBars->setBrush(QColor(chartBarColor()));
        }
        if (m_dailyLine) {
            m_dailyLine->setPen(QPen(QColor(Theme::kStateSuccess), 2));
            m_dailyLine->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,
                                                         QPen(QColor(Theme::kStateSuccess), 2),
                                                         QBrush(QColor(Theme::kPanelBg)),
                                                         7));
        }
        m_dailyPlot->legend->setTextColor(QColor(Theme::kTextSecondary));
        m_dailyPlot->replot();
    }
    if (m_heightPie) {
        m_heightPie->update();
    }
    if (m_resultPlot) {
        styleAxes(m_resultPlot);
        const QList<QColor> resultColors{QColor(Theme::kStateSuccess), QColor(Theme::kStateWarning),
                                         QColor(Theme::kStateError), QColor(Theme::kAccent),
                                         QColor(Theme::kStateRunning)};
        for (int i = 0; i < m_resultCategoryBars.size() && i < resultColors.size(); ++i) {
            m_resultCategoryBars.at(i)->setBrush(resultColors.at(i));
        }
        m_resultPlot->replot();
    }
    const bool singleHighlight = Theme::current() == Theme::Id::Light;
    const char *rest = singleHighlight ? Theme::kTitle : Theme::kAccent;
    const struct {
        QLabel *label;
        const char *color;
    } kpis[] = {
        {m_kpiBoards, Theme::kAccent},
        {m_kpiParticleBoards, rest},
        {m_kpiCompletion, rest},
        {m_kpiCycle, rest},
    };
    for (const auto &kpi : kpis) {
        if (!kpi.label) {
            continue;
        }
        kpi.label->setStyleSheet(QStringLiteral("color:%1;font-size:30px;font-weight:700;border:none;background:transparent;")
                                     .arg(kpi.color));
    }
    for (QLabel *icon : findChildren<QLabel *>()) {
        const QString resource = icon->property("iconResource").toString();
        if (resource.isEmpty()) {
            continue;
        }
        const QString accent = icon->property("kpiAccent").toString();
        icon->setPixmap(tintedIcon(resource, QColor(accent), 22, devicePixelRatioF()));
    }
}

void ReportPage::applyRange(QDate &from, QDate &to) const
{
    const QDate today = QDate::currentDate();
    switch (m_rangeCombo->currentIndex()) {
    case RangeToday:
        from = today;
        to = today;
        break;
    case Range7Days:
        from = today.addDays(-6);
        to = today;
        break;
    case Range30Days:
        from = today.addDays(-29);
        to = today;
        break;
    case RangeCustom:
        from = m_from->date();
        to = m_to->date();
        if (from > to) {
            std::swap(from, to);
        }
        break;
    case RangeAll:
    default:
        from = QDate();
        to = QDate();
        break;
    }
}

void ReportPage::reload()
{
    QDate from;
    QDate to;
    applyRange(from, to);

    InspectionQuery query;
    query.limit = kMaxRows;
    if (from.isValid()) {
        query.createdFrom = QDateTime(from, QTime(0, 0)).toUTC();
    }
    if (to.isValid()) {
        query.createdTo = QDateTime(to.addDays(1), QTime(0, 0)).toUTC();
    }

    QString error;
    QList<InspectionRecordRow> rows = InspectionRepository::loadFiltered(query, &error);
    if (!error.isEmpty()) {
        m_demoDataActive = false;
        m_status->setText(QStringLiteral("查询失败：%1").arg(error));
        m_status->setStyleSheet(QStringLiteral("color:%1;font-size:12px;").arg(Theme::kStateError));
        emit logRequested(QStringLiteral("ERROR"), QStringLiteral("报表查询失败：%1").arg(error));
        return;
    }

    m_demoDataActive = rows.size() < kDemoMinimumRows;
    if (m_demoDataActive) {
        rows.append(makeDemoRows(kDemoMinimumRows - rows.size(), from, to));
    }

    m_lastRangeText = from.isValid()
                          ? QStringLiteral("%1 至 %2").arg(from.toString(QStringLiteral("yyyy/MM/dd")),
                                                           to.toString(QStringLiteral("yyyy/MM/dd")))
                          : QStringLiteral("全部记录");
    m_status->setStyleSheet(QStringLiteral("color:%1;font-size:12px;").arg(Theme::kTextMuted));
    m_status->setText(QStringLiteral("%1 · 共 %2 条检测记录%3")
                          .arg(m_lastRangeText)
                          .arg(rows.size())
                          .arg(m_demoDataActive ? QStringLiteral(" · 含演示数据，仅供界面预览") : QString()));
    if (m_filterHint) {
        m_filterHint->setText((m_shiftCombo && m_shiftCombo->currentIndex() > 0) ||
                              (m_plateEdit && !m_plateEdit->text().trimmed().isEmpty())
                                  ? QStringLiteral("占位筛选未参与查询")
                                  : QStringLiteral("班次、板号暂为前端占位"));
    }

    QMap<QDate, DailyStat> daily;
    for (const InspectionRecordRow &row : rows) {
        QDateTime created = row.createdAt;
        if (created.timeSpec() == Qt::UTC) {
            created = created.toLocalTime();
        }
        DailyStat &stat = daily[created.date()];
        ++stat.boards;
        stat.particles += row.particleCount;
        stat.cleared += row.clearedCount;
        if (row.particleCount > 0) {
            ++stat.boardsWithParticles;
            if (row.clearedCount >= row.particleCount) {
                ++stat.boardsCompleted;
            }
        }
        if (isDemoRow(row)) {
            const int demoIndex = -row.id - 100000;
            const int shift = demoIndex % 3;
            ++stat.shiftBoards[shift];
            if (row.particleCount > 0 && row.clearedCount >= row.particleCount) {
                ++stat.shiftCompleted[shift];
            }
        }
        if (row.processStartAt.isValid() && row.processEndAt.isValid()) {
            const double seconds = row.processStartAt.secsTo(row.processEndAt);
            if (seconds > 0) {
                stat.cycleSecondsTotal += seconds;
                ++stat.cycleSamples;
            }
        }
    }
    m_lastDaily = daily;

    updateKpis(rows);
    updateDailyChart(daily);
    updateHeightChart(rows);
    updateResultChart(rows);
    updateDailyTable(daily);
}

void ReportPage::updateKpis(const QList<InspectionRecordRow> &rows)
{
    int boards = rows.size();
    int particleBoards = 0;
    int completed = 0;
    double cycleTotal = 0.0;
    int cycleSamples = 0;
    for (const InspectionRecordRow &row : rows) {
        if (row.particleCount > 0) {
            ++particleBoards;
            if (row.clearedCount >= row.particleCount) {
                ++completed;
            }
        }
        if (row.processStartAt.isValid() && row.processEndAt.isValid()) {
            const double seconds = row.processStartAt.secsTo(row.processEndAt);
            if (seconds > 0) {
                cycleTotal += seconds;
                ++cycleSamples;
            }
        }
    }
    m_kpiBoards->setText(QString::number(boards));
    m_kpiParticleBoards->setText(QString::number(particleBoards));
    m_kpiCompletion->setText(percentText(completed, particleBoards));
    if (cycleSamples > 0) {
        const double average = cycleTotal / cycleSamples;
        m_kpiCycle->setText(QString::number(average, 'f', average >= 100 ? 0 : 1));
        m_kpiCycle->setToolTip(QStringLiteral("目标 %1 s/张（招标要求 12 小时 2160 片）").arg(kTargetCycleSeconds));
    } else {
        m_kpiCycle->setText(QStringLiteral("--"));
        m_kpiCycle->setToolTip(QStringLiteral("范围内没有带起止时间的修磨记录"));
    }
    m_lastKpis = {boards, particleBoards, completed, cycleSamples > 0 ? int(cycleTotal / cycleSamples) : -1};
}

void ReportPage::updateDailyChart(const QMap<QDate, DailyStat> &daily)
{
    QVector<double> ticks;
    QVector<QString> labels;
    QVector<double> boards;
    QVector<double> completed;
    int index = 0;
    double maxValue = 0;
    for (auto it = daily.cbegin(); it != daily.cend(); ++it, ++index) {
        ticks.append(index);
        labels.append(it.key().toString(QStringLiteral("MM-dd")));
        boards.append(it.value().boards);
        completed.append(it.value().boardsCompleted);
        maxValue = std::max(maxValue, double(it.value().boards));
    }
    QSharedPointer<QCPAxisTickerText> ticker(new QCPAxisTickerText);
    ticker->addTicks(ticks, labels);
    m_dailyPlot->xAxis->setTicker(ticker);
    m_dailyPlot->xAxis->setTickLabelRotation(labels.size() > 14 ? 45 : 0);
    m_dailyBars->setData(ticks, boards);
    m_dailyLine->setData(ticks, completed);
    m_dailyPlot->xAxis->setRange(-0.7, std::max(0.7, ticks.size() - 0.3));
    m_dailyPlot->yAxis->setRange(0, std::max(4.0, maxValue * 1.25));
    m_dailyPlot->replot();
    m_dailyCaption->setText(daily.isEmpty() ? QStringLiteral("范围内没有记录")
                                            : QStringLiteral("%1 天 · 单位：张").arg(daily.size()));
}

void ReportPage::updateHeightChart(const QList<InspectionRecordRow> &rows)
{
    // 任务记录只有最大高度，先按任务级 MaxParticleHeight 做分段；单颗粒分布等明细接口补齐后再替换口径。
    QVector<double> values(4, 0.0);
    int measured = 0;
    for (const InspectionRecordRow &row : rows) {
        if (row.maxParticleHeight <= 0.0) {
            continue;
        }
        ++measured;
        if (row.maxParticleHeight < 1.0) {
            values[0] += 1.0;
        } else if (row.maxParticleHeight < 3.0) {
            values[1] += 1.0;
        } else if (row.maxParticleHeight < 5.0) {
            values[2] += 1.0;
        } else {
            values[3] += 1.0;
        }
    }
    m_heightPie->setValues({int(values[0]), int(values[1]), int(values[2]), int(values[3])});
    m_heightCaption->setText(measured > 0
                                 ? QStringLiteral("%1 条有高度记录 · 任务级最大高度").arg(measured)
                                 : QStringLiteral("暂无高度数据 · 待后端补齐明细接口"));
    m_lastHeightBuckets = {int(values[0]), int(values[1]), int(values[2]), int(values[3])};
}

void ReportPage::updateResultChart(const QList<InspectionRecordRow> &rows)
{
    int completed = 0;
    int review = 0;
    int abnormal = 0;
    int partial = 0;
    int clean = 0;
    for (const InspectionRecordRow &row : rows) {
        if (isDemoRow(row)) {
            const int demoIndex = -row.id - 100000;
            switch (demoIndex % 12) {
            case 0:
                ++review;
                continue;
            case 1:
                ++abnormal;
                continue;
            default:
                break;
            }
        }
        if (row.particleCount <= 0) {
            ++clean;
        } else if (row.clearedCount >= row.particleCount) {
            ++completed;
        } else {
            ++partial;
        }
    }
    const QVector<double> ticks{0, 1, 2, 3, 4};
    const QVector<QString> labels{QStringLiteral("复测通过"), QStringLiteral("待复核"), QStringLiteral("异常"),
                                 QStringLiteral("修磨中"), QStringLiteral("待修磨")};
    const QVector<double> values{double(completed), double(review), double(abnormal), double(partial), double(clean)};
    QSharedPointer<QCPAxisTickerText> ticker(new QCPAxisTickerText);
    ticker->addTicks(ticks, labels);
    m_resultPlot->yAxis->setTicker(ticker);
    m_resultPlot->yAxis->setRange(-0.6, 4.6);
    m_resultPlot->yAxis->setRangeReversed(true);
    for (int i = 0; i < m_resultCategoryBars.size() && i < values.size(); ++i) {
        m_resultCategoryBars.at(i)->setData(QVector<double>{ticks.at(i)},
                                            QVector<double>{values.at(i)});
    }
    const double maxValue = *std::max_element(values.cbegin(), values.cend());
    m_resultPlot->xAxis->setRange(0, std::max(4.0, maxValue * 1.2));
    m_resultPlot->replot();
    m_resultCaption->setText(QStringLiteral("按铜板计 · %1%2")
                                 .arg(m_demoDataActive ? QStringLiteral("演示状态已填充 · ") : QStringLiteral("状态字段待后端 · "))
                                 .arg(QStringLiteral("共 %1 张").arg(rows.size())));
}

void ReportPage::updateDailyTable(const QMap<QDate, DailyStat> &daily)
{
    m_dailyRows.clear();
    m_dailyRows.reserve(daily.size());
    for (auto it = daily.cbegin(); it != daily.cend(); ++it) {
        m_dailyRows.append(DailyRow{it.key(), it.value()});
    }
    std::reverse(m_dailyRows.begin(), m_dailyRows.end());
    const int pages = qMax(1, (m_dailyRows.size() + kDailyPageSize - 1) / kDailyPageSize);
    if (m_dailyPage >= pages) {
        m_dailyPage = pages - 1;
    }
    renderDailyPage();
}

void ReportPage::renderDailyPage()
{
    if (!m_dailyTable) {
        return;
    }
    const int pages = qMax(1, (m_dailyRows.size() + kDailyPageSize - 1) / kDailyPageSize);
    m_dailyPage = qBound(0, m_dailyPage, pages - 1);
    const int begin = m_dailyPage * kDailyPageSize;
    const int count = qMin(kDailyPageSize, m_dailyRows.size() - begin);
    m_dailyTable->setRowCount(qMax(0, count));
    for (int row = 0; row < count; ++row) {
        const DailyRow &entry = m_dailyRows.at(begin + row);
        const DailyStat &stat = entry.stat;
        const auto shiftText = [&stat](int shift) {
            if (stat.shiftBoards[shift] <= 0) {
                return QStringLiteral("--");
            }
            return QStringLiteral("%1 / %2")
                .arg(stat.shiftBoards[shift])
                .arg(stat.shiftCompleted[shift]);
        };
        const QStringList cells{
            entry.date.toString(QStringLiteral("yyyy-MM-dd")),
            shiftText(0),
            shiftText(1),
            shiftText(2),
            QString::number(stat.boards),
            QString::number(stat.boardsCompleted),
        };
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            item->setTextAlignment(column == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : Qt::AlignCenter);
            m_dailyTable->setItem(row, column, item);
        }
    }
    if (m_tableCaption) {
        m_tableCaption->setText(m_demoDataActive
                                    ? QStringLiteral("格式：检测 / 修磨 · 演示班次数据仅供界面预览")
                                    : QStringLiteral("班次数据待后端 · 合计来自现有检测记录"));
    }
    if (m_dailyPageLabel) {
        m_dailyPageLabel->setText(QStringLiteral("共 %1 条 · 每页 %2 条")
                                      .arg(m_dailyRows.size())
                                      .arg(kDailyPageSize));
    }
    if (m_dailyPrev) {
        m_dailyPrev->setEnabled(m_dailyPage > 0);
    }
    if (m_dailyNext) {
        m_dailyNext->setEnabled(m_dailyPage + 1 < pages && !m_dailyRows.isEmpty());
    }
}

void ReportPage::exportExcel()
{
    emit logRequested(QStringLiteral("WARN"), QStringLiteral("导出 Excel 需要 QXlsx 库，尚未接入。"));
}

void ReportPage::exportPdf()
{
    if (m_lastKpis.size() < 4) {
        reload();
    }
    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString defaultName = QStringLiteral("修磨报表_%1.pdf")
                                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("导出 PDF"),
                                                      QDir(defaultDir).filePath(defaultName),
                                                      QStringLiteral("PDF 文件 (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }

    QString html;
    html += QStringLiteral("<h2 style='margin-bottom:4px;'>阴极铜板粒子检测及修磨系统 · 统计报表</h2>");
    html += QStringLiteral("<p style='color:#666;'>统计范围：%1 &nbsp;&nbsp; 导出时间：%2%3</p>")
                .arg(m_lastRangeText,
                     QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")),
                     m_demoDataActive ? QStringLiteral(" &nbsp;&nbsp; <b>含演示数据</b>") : QString());
    html += QStringLiteral("<table cellspacing='0' cellpadding='6' border='1' style='border-collapse:collapse;'>"
                           "<tr><th>检测铜板</th><th>有粒子修磨铜板</th><th>修磨完成率</th><th>平均修磨节拍</th></tr>"
                           "<tr><td>%1 张</td><td>%2 张</td><td>%3</td><td>%4</td></tr></table><br/>")
                .arg(m_kpiBoards->text(), m_kpiParticleBoards->text(),
                     m_kpiCompletion->text() == QStringLiteral("--") ? QStringLiteral("--") : m_kpiCompletion->text() + QStringLiteral(" %"),
                     m_kpiCycle->text() == QStringLiteral("--") ? QStringLiteral("--") : m_kpiCycle->text() + QStringLiteral(" s/张"));
    html += QStringLiteral("<h3>每日班次汇总</h3><table cellspacing='0' cellpadding='5' border='1' style='border-collapse:collapse;'>"
                           "<tr><th>日期</th><th>甲班</th><th>乙班</th><th>丙班</th><th>检测合计</th><th>修磨合计</th></tr>");
    const auto shiftText = [](const DailyStat &stat, int shift) {
        if (stat.shiftBoards[shift] <= 0) {
            return QStringLiteral("--");
        }
        return QStringLiteral("%1 / %2")
            .arg(stat.shiftBoards[shift])
            .arg(stat.shiftCompleted[shift]);
    };
    for (const DailyRow &entry : m_dailyRows) {
        const DailyStat &stat = entry.stat;
        html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td></tr>")
                    .arg(entry.date.toString(QStringLiteral("yyyy-MM-dd")),
                         shiftText(stat, 0),
                         shiftText(stat, 1),
                         shiftText(stat, 2),
                         QString::number(stat.boards),
                         QString::number(stat.boardsCompleted));
    }
    html += QStringLiteral("</table>");

    html += QStringLiteral("<h3>最大粒子高度分布</h3><p>按任务级 MaxParticleHeight 汇总，单位：mm。</p>"
                           "<table cellspacing='0' cellpadding='5' border='1' style='border-collapse:collapse;'>"
                           "<tr><th>高度区间</th><th>任务数</th></tr>");
    const QStringList heightLabels{QStringLiteral("0 - 1 mm"), QStringLiteral("1 - 3 mm"),
                                   QStringLiteral("3 - 5 mm"), QStringLiteral(">= 5 mm")};
    for (int i = 0; i < heightLabels.size(); ++i) {
        const int count = i < m_lastHeightBuckets.size() ? m_lastHeightBuckets.at(i) : 0;
        html += QStringLiteral("<tr><td>%1</td><td>%2</td></tr>")
                    .arg(heightLabels.at(i), QString::number(count));
    }
    html += QStringLiteral("</table><p style='color:#666;'>该分布按任务级最大高度汇总；单颗粒明细分布待后端接口补齐。</p>");

    QTextDocument document;
    document.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 10));
    document.setHtml(html);
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    document.print(&printer);
    emit logRequested(QStringLiteral("INFO"), QStringLiteral("报表已导出：%1").arg(QDir::toNativeSeparators(path)));
}
