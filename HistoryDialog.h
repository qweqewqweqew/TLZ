#ifndef HISTORYDIALOG_H
#define HISTORYDIALOG_H

#include <QDialog>

class ElaComboBox;
class ElaPushButton;
class QDateEdit;
class QLabel;
class QPropertyAnimation;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTableWidget;
class QTimer;
class QVariantAnimation;
class QVBoxLayout;

#include <QMetaObject>
#include <functional>

class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void reloadRecords();
    void onRowClicked(int row, int column);

private:
    QWidget *buildFilterBar();
    QWidget *buildSummaryRow();
    QWidget *buildTableCard();

    QLabel *makeTileValue(QWidget *tile);
    QWidget *makeTile(const QString &title, const QString &suffix, QLabel *&valueLabelOut);

    void populateRecipeCombo();
    void updateSummary(const struct HistorySummary &s);
    void populateTable(const class QList<struct InspectionRecordRow> &rows);

    void updateCustomRangeEnabled();
    void toggleDetailRow(int row);
    void expandDetailRow(int row, int recordId);
    void collapseDetail(std::function<void()> after = {});
    QWidget *createDetailWidget(int recordId);
    int findRowByRecordId(int recordId) const;
    void scrollRowIntoView(int row);

    // 触发一次去抖 reload（180ms 内合并连续过滤器变更）
    void scheduleReload();

    // 把 wheel 事件过滤器递归装到 widget 及其所有子控件上，
    // 用来阻止 QTabBar / 内嵌 QTableWidget 抢走滚轮
    void installWheelForwarderRecursive(QWidget *widget);

    // Filters
    ElaComboBox   *m_rangeCombo{nullptr};
    ElaComboBox   *m_recipeCombo{nullptr};
    QDateEdit     *m_customFrom{nullptr};
    QDateEdit     *m_customTo{nullptr};
    QLabel        *m_customDash{nullptr};
    ElaPushButton *m_refreshButton{nullptr};
    ElaPushButton *m_closeButton{nullptr};   // 保留但不再挂到界面上

    // Summary tiles
    QLabel *m_tileTaskCount{nullptr};
    QLabel *m_tileParticles{nullptr};
    QLabel *m_tileClearRate{nullptr};
    QLabel *m_tileMaxHeight{nullptr};

    // Body
    QLabel         *m_statusLabel{nullptr};
    QSplitter      *m_splitter{nullptr};       // 未使用，保留以兼容旧引用
    QStackedWidget *m_tableStack{nullptr};     // 0 = table, 1 = empty state
    QTableWidget   *m_table{nullptr};

    // Legacy detail members (未使用，保留成员名以兼容旧代码；点击展开使用 m_expanded* )
    QStackedWidget *m_detailStack{nullptr};
    QLabel         *m_detailTitle{nullptr};
    QLabel         *m_detailMeta{nullptr};
    QLabel         *m_detailOverview{nullptr};
    QLabel         *m_detailKpi{nullptr};
    QTabWidget     *m_detailTabs{nullptr};
    QTableWidget   *m_particleTable{nullptr};
    QTableWidget   *m_processTable{nullptr};

    // Expand-in-row detail state
    int      m_expandedRow{-1};      // 被展开的记录行（未展开为 -1）
    int      m_expandedRecordId{-1}; // 展开的记录 Id
    QWidget *m_expandedWidget{nullptr};
    QVariantAnimation *m_expandAnim{nullptr};
    // 展开动画一次性 finished 连接；每次启动动画前重连，避免误伤其他连接
    QMetaObject::Connection m_expandAnimFinishedConn;

    // 平滑滚轮
    QVariantAnimation *m_scrollAnim{nullptr};

    // 过滤器变化去抖
    QTimer *m_reloadTimer{nullptr};

    bool m_reloading{false};
    bool m_animating{false};
};

#endif // HISTORYDIALOG_H
