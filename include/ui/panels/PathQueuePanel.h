#ifndef PATHQUEUEPANEL_H
#define PATHQUEUEPANEL_H

#include "bridge/MillingPathVM.h"
#include "vision/PointCloudFrame.h"

#include <QFrame>
#include <QVector>

class QEvent;
class QObject;

class DataStatisticsPanel;
class ElaPushButton;
class QLabel;
class QStackedWidget;
class QTableWidget;

// 首页底部卡：「修磨队列」路径表 与 「动态曲线」主轴转速/扭矩，两页切换。
class PathQueuePanel : public QFrame
{
    Q_OBJECT

public:
    explicit PathQueuePanel(QWidget *parent = nullptr);

    DataStatisticsPanel *telemetry() const { return m_telemetry; }

    void setMillingPaths(const QVector<MillingPathVM> &paths);
    void setMetricFrame(const PointCloudFrame &frame);
    // PLC 进度：currentPath 1-based
    void setPathProgress(int completedPath, int currentPath);
    // 高亮并滚动到某条路径（0-based）
    void selectPath(int pathIndex);
    void clear();

signals:
    void pathSelected(int pathIndex);

private:
    void rebuildRows();
    void refreshStatusColumn();
    QString metricPoint(int u, int v) const;
    void showQueue(bool queue);
    void layoutQueueColumns();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

    ElaPushButton *m_queueButton{nullptr};
    ElaPushButton *m_curveButton{nullptr};
    QLabel *m_summary{nullptr};
    QStackedWidget *m_stack{nullptr};
    QTableWidget *m_table{nullptr};
    DataStatisticsPanel *m_telemetry{nullptr};

    QVector<MillingPathVM> m_paths;
    PointCloudFrame m_frame;
    int m_completed{0};
    int m_current{0};
    bool m_updatingSelection{false};
    bool m_layingOutColumns{false};
};

#endif // PATHQUEUEPANEL_H
