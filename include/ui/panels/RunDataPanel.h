#ifndef RUNDATAPANEL_H
#define RUNDATAPANEL_H

#include <QFrame>
#include <QHash>

class QIcon;
class QLabel;
class QTimer;

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

    // 来自 MillingPaths：切削条数 / 空移条数，显示为 "23/24"。
    void setCutEmptyPathCounts(int cutCount, int emptyCount);

    // 复位所有指标为 "--"。
    void clearMetrics();

    void setMillingTask(quint64 taskId, int pathTotal);
    // 算法 JSON particle_board_grade，任务级原样显示。
    void setParticleBoardGrade(const QString &grade);
    void setMillingStatus(bool finished, bool success);
    void clearMillingTask();

private:
    enum class TaskStatus { Idle, Running, Success, Failed };

    QWidget *createTaskHeader(QWidget *parent);
    QWidget *createMetricCard(const QString &key,
                              const QString &title,
                              const QString &value,
                              const QString &unit,
                              const QIcon &icon,
                              QWidget *parent);
    void applyStatus(TaskStatus status);
    void pulseStatusText();

    QHash<QString, QLabel *> m_metricLabels;
    QLabel *m_pathTotalLabel{nullptr};
    QLabel *m_statusDot{nullptr};
    QLabel *m_statusText{nullptr};
    QTimer *m_pulseTimer{nullptr};
    QString m_statusColor;
    int m_pulseStep{0};
};

#endif // RUNDATAPANEL_H
