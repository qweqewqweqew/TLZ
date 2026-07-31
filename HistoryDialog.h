#ifndef HISTORYDIALOG_H
#define HISTORYDIALOG_H

#include <QDialog>
#include <QPoint>

class ElaComboBox;
class ElaIconButton;
class ElaPushButton;
class QDateEdit;
class QLabel;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTableWidget;
class QVBoxLayout;
class QEvent;

class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void reloadRecords();
    void onRowClicked(int row, int column);

private:
    QWidget *buildFilterBar();
    QWidget *buildSummaryRow();
    QWidget *buildTableCard();
    QWidget *buildTitleBar();

    QLabel *makeTileValue(QWidget *tile);
    QWidget *makeTile(const QString &title, const QString &suffix, QLabel *&valueLabelOut);

    void populateRecipeCombo();
    void updateSummary(const struct HistorySummary &s);
    void populateTable(const class QList<struct InspectionRecordRow> &rows);
    void updateMainTableColumnWidths();

    void updateCustomRangeEnabled();
    void showDetailPlaceholder(const QString &message);
    void showDetailForRecord(int recordId);
    QWidget *createDetailWidget(int recordId);
    int findRowByRecordId(int recordId) const;
    void toggleMaximized();
    void updateMaximizeButtonIcon();

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
    QWidget        *m_titleBar{nullptr};
    ElaIconButton  *m_maximizeButton{nullptr};
    QSplitter      *m_splitter{nullptr};
    QStackedWidget *m_tableStack{nullptr};     // 0 = table, 1 = empty state
    QTableWidget   *m_table{nullptr};

    // Right-side detail panel
    QStackedWidget *m_detailStack{nullptr};
    QScrollArea    *m_detailScroll{nullptr};
    QWidget        *m_detailContent{nullptr};
    QLabel         *m_detailPlaceholderLabel{nullptr};
    QLabel         *m_detailTitle{nullptr};
    QLabel         *m_detailMeta{nullptr};
    QLabel         *m_detailOverview{nullptr};
    QLabel         *m_detailKpi{nullptr};
    QTabWidget     *m_detailTabs{nullptr};
    QTableWidget   *m_particleTable{nullptr};
    QTableWidget   *m_processTable{nullptr};
    int             m_selectedRecordId{-1};

    bool m_reloading{false};
    bool m_dragging{false};
    QPoint m_dragPosition;
};

#endif // HISTORYDIALOG_H
