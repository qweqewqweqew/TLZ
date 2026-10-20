#ifndef KPISTRIPPANEL_H
#define KPISTRIPPANEL_H

#include <QWidget>

class QLabel;

// 首页顶部四个指标卡：检测粒子 / 待修磨 / 已完成 / 板级判定。
// 粒子按算法下发的切削路径计数；板级判定显示 milling/paths 的 particle_board_grade。
class KpiStripPanel : public QWidget
{
    Q_OBJECT

public:
    explicit KpiStripPanel(QWidget *parent = nullptr);

    void setCounts(int detected, int pending, int completed);
    void setBoardGrade(const QString &grade);
    void clear();

private:
    QWidget *makeTile(const QString &title, const QString &unit, const QString &iconResource,
                      const QString &accent, QLabel *&valueOut);
    void setValue(QLabel *label, int value, const QString &accent);

    QLabel *m_detected{nullptr};
    QLabel *m_pending{nullptr};
    QLabel *m_completed{nullptr};
    QLabel *m_review{nullptr};
    int m_detectedCount{-1};
    int m_pendingCount{-1};
    int m_completedCount{-1};
    QString m_grade;
};

#endif // KPISTRIPPANEL_H
