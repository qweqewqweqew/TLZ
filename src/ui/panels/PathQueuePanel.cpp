#include "ui/panels/PathQueuePanel.h"

#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "ui/panels/DataStatisticsPanel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

enum Column {
    ColIndex = 0,
    ColStart,
    ColEnd,
    ColStatus,
    ColCount,
};

} // namespace

PathQueuePanel::PathQueuePanel(QWidget *parent)
    : QFrame(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    auto *toggleRow = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(toggleRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);
    m_summary = new QLabel(toggleRow);
    m_summary->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;border:none;")
                                 .arg(Theme::kTextSecondary));
    headerLayout->addWidget(m_summary, 1);
    m_queueButton = new ElaPushButton(QStringLiteral("队列"), toggleRow);
    m_curveButton = new ElaPushButton(QStringLiteral("曲线"), toggleRow);
    for (ElaPushButton *button : {m_queueButton, m_curveButton}) {
        button->setFixedSize(56, 26);
        button->setCheckable(true);
        headerLayout->addWidget(button);
    }

    auto *panel = createPanel(QStringLiteral("修磨队列"), nullptr, this);
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);
    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    panelLayout->addWidget(toggleRow);

    m_stack = new QStackedWidget(panel);
    panelLayout->addWidget(m_stack, 1);

    m_table = new QTableWidget(0, ColCount, m_stack);
    m_table->setObjectName(QStringLiteral("queueTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("编号"),
                                        QStringLiteral("起点"),
                                        QStringLiteral("终点"),
                                        QStringLiteral("状态")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_table->verticalHeader()->setDefaultSectionSize(26);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_table->horizontalHeader()->setTextElideMode(Qt::ElideNone);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->viewport()->installEventFilter(this);
    layoutQueueColumns();
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setStyleSheet(QStringLiteral(
                               "QTableWidget#queueTable {"
                               "  background:%1; color:%2; gridline-color:%3; border:none;"
                               "  alternate-background-color:%4; selection-background-color:%5; selection-color:%7;"
                               "}"
                               "QTableWidget#queueTable::item { padding:2px 8px; }"
                               "QHeaderView::section {"
                               "  background:%1; color:%6; padding:6px 0px; border:none;"
                               "  border-bottom:1px solid %3; font-weight:600;"
                               "}")
                               .arg(Theme::kPanelBg,
                                    Theme::kTextPrimary,
                                    Theme::kBorder,
                                    Theme::kControlBg,
                                    Theme::kSelectionBg,
                                    Theme::kTextSecondary,
                                    Theme::kSelectionText));
    m_stack->addWidget(m_table);

    m_telemetry = new DataStatisticsPanel(m_stack, /*framed=*/false);
    m_stack->addWidget(m_telemetry);

    connect(m_queueButton, &ElaPushButton::clicked, this, [this]() { showQueue(true); });
    connect(m_curveButton, &ElaPushButton::clicked, this, [this]() { showQueue(false); });
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (m_updatingSelection || row < 0 || row >= m_paths.size()) {
            return;
        }
        emit pathSelected(row);
    });

    showQueue(true);
    clear();
    Theme::watch(this, [this]() {
        if (m_summary) {
            m_summary->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;border:none;")
                                         .arg(Theme::kTextSecondary));
        }
        if (m_table) {
            m_table->setStyleSheet(QStringLiteral(
                                       "QTableWidget#queueTable {"
                                       "  background:%1; color:%2; gridline-color:%3; border:none;"
                                       "  alternate-background-color:%4; selection-background-color:%5; selection-color:%7;"
                                       "}"
                                       "QTableWidget#queueTable::item { padding:2px 8px; }"
                                       "QHeaderView::section {"
                                       "  background:%1; color:%6; padding:6px 0px; border:none;"
                                       "  border-bottom:1px solid %3; font-weight:600;"
                                       "}")
                                       .arg(Theme::kPanelBg,
                                            Theme::kTextPrimary,
                                            Theme::kBorder,
                                            Theme::kControlBg,
                                            Theme::kSelectionBg,
                                            Theme::kTextSecondary,
                                            Theme::kSelectionText));
        }
        showQueue(m_stack && m_stack->currentWidget() == m_table);
    });
}

void PathQueuePanel::layoutQueueColumns()
{
    if (!m_table || m_layingOutColumns) {
        return;
    }
    m_layingOutColumns = true;

    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(ColIndex, QHeaderView::Fixed);
    header->setSectionResizeMode(ColStatus, QHeaderView::Fixed);
    header->setSectionResizeMode(ColStart, QHeaderView::Stretch);
    header->setSectionResizeMode(ColEnd, QHeaderView::Stretch);
    m_table->setColumnWidth(ColIndex, 48);
    m_table->setColumnWidth(ColStatus, 68);
    m_layingOutColumns = false;
}

bool PathQueuePanel::eventFilter(QObject *watched, QEvent *event)
{
    if (m_table && watched == m_table->viewport() && event->type() == QEvent::Resize) {
        layoutQueueColumns();
    }
    return QFrame::eventFilter(watched, event);
}

void PathQueuePanel::showQueue(bool queue)
{
    fadeToStackedWidgetPage(m_stack,
                            queue ? static_cast<QWidget *>(m_table) : static_cast<QWidget *>(m_telemetry));
    const auto style = [](ElaPushButton *button, bool active) {
        button->setChecked(active);
        active ? applySelectedButtonStyle(button) : applySecondaryButtonStyle(button);
        button->update();
    };
    style(m_queueButton, queue);
    style(m_curveButton, !queue);
}

QString PathQueuePanel::metricPoint(int u, int v) const
{
    if (!m_frame.hasMetricGeometry) {
        return QStringLiteral("%1,%2").arg(u).arg(v);
    }
    const int x = qRound(m_frame.xOffsetMm + u * m_frame.xScaleMm);
    const int y = qRound(m_frame.yOffsetMm + v * m_frame.yScaleMm);
    return QStringLiteral("%1,%2").arg(x).arg(y);
}

void PathQueuePanel::setMillingPaths(const QVector<MillingPathVM> &paths)
{
    m_paths = paths;
    m_completed = 0;
    m_current = 0;
    rebuildRows();
}

void PathQueuePanel::setMetricFrame(const PointCloudFrame &frame)
{
    m_frame = frame;
    rebuildRows();
}

void PathQueuePanel::setPathProgress(int completedPath, int currentPath)
{
    if (m_completed == completedPath && m_current == currentPath) {
        return;
    }
    m_completed = completedPath;
    m_current = currentPath;
    refreshStatusColumn();
    if (m_current > 0 && m_current <= m_paths.size()) {
        m_table->scrollToItem(m_table->item(m_current - 1, ColIndex), QAbstractItemView::EnsureVisible);
    }
}

void PathQueuePanel::selectPath(int pathIndex)
{
    if (pathIndex < 0 || pathIndex >= m_paths.size()) {
        return;
    }
    m_updatingSelection = true;
    m_table->selectRow(pathIndex);
    m_table->scrollToItem(m_table->item(pathIndex, ColIndex), QAbstractItemView::PositionAtCenter);
    m_updatingSelection = false;
}

void PathQueuePanel::clear()
{
    m_paths.clear();
    m_frame = PointCloudFrame();
    m_completed = 0;
    m_current = 0;
    rebuildRows();
}

void PathQueuePanel::rebuildRows()
{
    m_updatingSelection = true;
    m_table->setRowCount(m_paths.size());
    int cut = 0;
    for (int i = 0; i < m_paths.size(); ++i) {
        const MillingPathVM &path = m_paths.at(i);
        if (path.doCut) {
            ++cut;
        }
        const QStringList cells{
            QStringLiteral("%1").arg(i + 1, 2, 10, QLatin1Char('0')),
            metricPoint(path.uStart, path.vStart),
            metricPoint(path.uEnd, path.vEnd),
            QString(),
        };
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(i, column, item);
        }
    }
    m_updatingSelection = false;
    refreshStatusColumn();
    m_summary->setText(m_paths.isEmpty()
                           ? QStringLiteral("暂无任务")
                           : QStringLiteral("共 %1 条 · 切削 %2 · 空移 %3")
                                 .arg(m_paths.size()).arg(cut).arg(m_paths.size() - cut));
}

void PathQueuePanel::refreshStatusColumn()
{
    for (int i = 0; i < m_paths.size(); ++i) {
        QTableWidgetItem *item = m_table->item(i, ColStatus);
        if (!item) {
            continue;
        }
        const bool cutting = m_paths.at(i).doCut;
        if (i < m_completed) {
            item->setText(QStringLiteral("已完成"));
            item->setForeground(QColor(Theme::kStateSuccess));
        } else if (m_current > 0 && i == m_current - 1) {
            item->setText(cutting ? QStringLiteral("修磨中") : QStringLiteral("移动中"));
            item->setForeground(QColor(Theme::kStateRunning));
        } else {
            item->setText(cutting ? QStringLiteral("待修磨") : QStringLiteral("待执行"));
            item->setForeground(QColor(Theme::kTextMuted));
        }
    }
}
