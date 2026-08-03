#ifndef RUNDATAPANEL_H
#define RUNDATAPANEL_H

#include <QFrame>
#include <QHash>

class QLabel;

// “运行数据”面板：展示检测指标、任务 ID、路径进度和当前任务状态。
class RunDataPanel : public QFrame
{
    Q_OBJECT

public:
    explicit RunDataPanel(QWidget *parent = nullptr);

    // 通用：按 key 更新某个指标卡的值（"--" 会走灰色 no-data 样式）。
    void setMetricValue(const QString &key, const QString &value);

    // 便捷：来自 MillingPaths 的最大颗粒高度（整数 mm）。
    // 传入负数时按 "--" 复位显示。
    void setMaxParticleHeight(int maxParticleHeight);

    void setPlcPathCounts(int completedPath, int currentPath);
    void setMillingProgress(float progress);

    // 复位所有指标为 "--"。
    void clearMetrics();

    void setMillingTask(quint64 taskId, int pathTotal, bool calibrationApplied);
    void setMillingStatus(bool finished, bool success);
    void clearMillingTask();

private:
    enum class TaskStatus { Idle, Running, Success, Failed };

    QWidget *createTaskHeader(QWidget *parent);
    QWidget *createMetricCard(const QString &key,
                              const QString &title,
                              const QString &value,
                              const QString &unit,
                              const QColor &accent,
                              QWidget *parent);
    void applyStatus(TaskStatus status);

    QHash<QString, QLabel *> m_metricLabels;
    QLabel *m_pathTotalLabel{nullptr};
    QLabel *m_statusDot{nullptr};
    QLabel *m_statusText{nullptr};
    QLabel *m_calibLabel{nullptr};
};

#endif // RUNDATAPANEL_H
