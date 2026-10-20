#ifndef HISTORYDIALOG_H
#define HISTORYDIALOG_H

#include "data/InspectionRepository.h"

#include <QDialog>

class ElaComboBox;
class ElaPushButton;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
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

public slots:
    void reloadRecords();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onRowClicked(int row, int column);

private:
    QWidget *buildFilterBar();
    QWidget *buildSummaryRow();
    QWidget *buildTableCard();

    QLabel *makeTileValue(QWidget *tile);
    QWidget *makeTile(const QString &title, const QString &suffix, const QString &valueColor,
                      QLabel *&valueLabelOut);

    void updateSummary(const struct HistorySummary &s);
    void populateTable(const QList<InspectionRecordRow> &rows);
    void updateMainTableColumnWidths();
    void renderPage();
    void exportCurrentList();

    void showDetailPlaceholder(const QString &message);
    void showDetailForRecord(int recordId);
    void openReviewPage(int recordId);
    QWidget *createDetailWidget(int recordId);
    int findRowByRecordId(int recordId) const;

    // Filters
    QDateEdit     *m_from{nullptr};
    QDateEdit     *m_to{nullptr};
    QLineEdit     *m_plateEdit{nullptr};
    ElaComboBox   *m_shiftCombo{nullptr};
    ElaComboBox   *m_statusCombo{nullptr};
    ElaPushButton *m_refreshButton{nullptr};
    ElaPushButton *m_resetButton{nullptr};
    QPushButton *m_exportButton{nullptr};
    bool m_splitterSized{false};

    // Summary tiles
    QLabel *m_tileTaskCount{nullptr};
    QLabel *m_tileParticles{nullptr};
    QLabel *m_tileClearRate{nullptr};
    QLabel *m_tilePending{nullptr};

    QList<InspectionRecordRow> m_filtered;
    int m_page{0};
    ElaPushButton *m_prevPage{nullptr};
    ElaPushButton *m_nextPage{nullptr};
    QLabel *m_pageLabel{nullptr};
    QLabel *m_totalLabel{nullptr};

    // Body
    QLabel         *m_statusLabel{nullptr};
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
};

#endif // HISTORYDIALOG_H
