#ifndef MACHINESTATUSPANEL_H
#define MACHINESTATUSPANEL_H

#include <QFrame>

class QPushButton;
class QLabel;
class QProgressBar;
struct PlcFeedbackVM;

// 「修磨实时状态」卡：PLC 状态、任务进度、主轴 / 进给 / 坐标实时值。
// 启动 / 暂停 / 停止按钮已配色并带图标，控制指令仍等待后端接口。
class MachineStatusPanel : public QFrame
{
    Q_OBJECT

public:
    explicit MachineStatusPanel(QWidget *parent = nullptr);

    void setPlcFeedback(const PlcFeedbackVM &feedback);
    void setTask(quint64 taskId, int pathTotal);
    void setTaskProgress(float progressPercent);
    void setTaskFinished(bool success);
    void clear();
    QWidget *metricColumn() const;

signals:
    // 演示模式下由 MainWindow 接管：开始/继续、暂停在当前、停止并复位。
    void startMillingRequested();
    void pauseMillingRequested();
    void stopMillingRequested();

private:
    void setMetric(const QString &key, double value, int decimals, bool hasData = true);
    void refreshTaskLine();
    void applyTheme();
    void applyPlcPill();

    QWidget *m_metricColumn{nullptr};
    QLabel *m_plcPill{nullptr};
    QLabel *m_taskLine{nullptr};
    QProgressBar *m_progress{nullptr};
    QLabel *m_percent{nullptr};
    QPushButton *m_startButton{nullptr};
    QPushButton *m_pauseButton{nullptr};
    QPushButton *m_stopButton{nullptr};

    quint64 m_taskId{0};
    int m_pathTotal{0};
    int m_completedPath{0};
    int m_currentPath{0};
    bool m_finished{false};
    bool m_success{false};
    bool m_plcLinked{false};
    QString m_plcColor;
};

#endif // MACHINESTATUSPANEL_H
