#ifndef REPORTPAGE_H
#define REPORTPAGE_H

#include <QDate>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QWidget>

class ElaComboBox;
class ElaPushButton;
class QCPBars;
class QCPGraph;
class QCustomPlot;
class QDateEdit;
class QLabel;
class QLineEdit;
class QTableWidget;
class HeightDistributionWidget;
struct InspectionRecordRow;

// 报表统计：按时间范围汇总检测记录，给出 KPI、趋势、任务结果与每日汇总表，可导出 PDF。
// 班次、板号和单颗粒高度明细先保留前端占位，数据口径见 README。
class ReportPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReportPage(QWidget *parent = nullptr);

    // Ctrl+T 调试演示模式开关：只有开启时才补演示记录，正式查询只展示真实数据。
    void setDemoMode(bool active);

public slots:
    void reload();

signals:
    void logRequested(const QString &level, const QString &message);

private:
    struct DailyStat
    {
        int boards{0};
        int boardsWithParticles{0};
        int boardsCompleted{0};
        int particles{0};
        int cleared{0};
        double cycleSecondsTotal{0.0};
        int cycleSamples{0};
        QMap<QString, int> shiftBoards;
        QMap<QString, int> shiftCompleted;
    };

    // 班次名称来自 backend_params.ini 的 [Report] Shifts，可自定义，不再固定甲乙丙。
    QStringList shiftNames() const;

    QWidget *buildFilterBar();
    QWidget *buildKpiRow();
    QWidget *buildChartsRow();
    QWidget *buildDailyTable();
    QWidget *makeKpiTile(const QString &title, const QString &unit, const QString &iconResource,
                         const QString &accent,
                         QLabel *&valueOut);
    void styleAxes(QCustomPlot *plot);
    void applyRange(QDate &from, QDate &to) const;
    void updateKpis(const QList<InspectionRecordRow> &rows);
    void updateDailyChart(const QMap<QDate, DailyStat> &daily);
    void updateHeightChart(const QList<InspectionRecordRow> &rows);
    void updateResultChart(const QList<InspectionRecordRow> &rows);
    void updateDailyTable(const QMap<QDate, DailyStat> &daily);
    void renderDailyPage();
    void applyTheme();
    void exportPdf();
    void exportExcel();

    ElaComboBox *m_rangeCombo{nullptr};
    ElaComboBox *m_shiftCombo{nullptr};
    QLineEdit *m_plateEdit{nullptr};
    QDateEdit *m_from{nullptr};
    QDateEdit *m_to{nullptr};
    ElaPushButton *m_queryButton{nullptr};
    ElaPushButton *m_resetButton{nullptr};
    ElaPushButton *m_excelButton{nullptr};
    ElaPushButton *m_pdfButton{nullptr};
    QLabel *m_status{nullptr};

    QLabel *m_kpiBoards{nullptr};
    QLabel *m_kpiParticleBoards{nullptr};
    QLabel *m_kpiCompletion{nullptr};
    QLabel *m_kpiCycle{nullptr};

    QCustomPlot *m_dailyPlot{nullptr};
    QCPBars *m_dailyBars{nullptr};
    QCPGraph *m_dailyLine{nullptr};
    QLabel *m_dailyCaption{nullptr};
    HeightDistributionWidget *m_heightPie{nullptr};
    QLabel *m_heightCaption{nullptr};
    QCustomPlot *m_resultPlot{nullptr};
    QCPBars *m_resultBars{nullptr};
    QList<QCPBars *> m_resultCategoryBars;
    QLabel *m_resultCaption{nullptr};
    QTableWidget *m_dailyTable{nullptr};
    QLabel *m_tableCaption{nullptr};
    struct DailyRow
    {
        QDate date;
        DailyStat stat;
    };
    QList<DailyRow> m_dailyRows;
    int m_dailyPage{0};
    ElaPushButton *m_dailyPrev{nullptr};
    ElaPushButton *m_dailyNext{nullptr};
    QLabel *m_dailyPageLabel{nullptr};

    QList<int> m_lastKpis;
    QList<int> m_lastHeightBuckets;
    QMap<QDate, DailyStat> m_lastDaily;
    QString m_lastRangeText;
    bool m_demoDataActive{false};
    bool m_demoModeActive{false};
};

#endif // REPORTPAGE_H
