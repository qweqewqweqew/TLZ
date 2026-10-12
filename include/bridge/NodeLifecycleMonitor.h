#ifndef NODELIFECYCLEMONITOR_H
#define NODELIFECYCLEMONITOR_H

#include <QObject>

class QTimer;

// 轮询本机进程，判断 start_all.bat 拉起的三个节点是否都还活着。
class NodeLifecycleMonitor : public QObject
{
    Q_OBJECT

public:
    explicit NodeLifecycleMonitor(QObject *parent = nullptr);

    bool nodesRunning() const { return m_nodesRunning; }
    void pollNow();

signals:
    void nodesRunningChanged(bool running);

private:
    void poll();
    static bool allTargetNodesRunning();

    QTimer *m_timer{nullptr};
    bool m_nodesRunning{false};
};

#endif // NODELIFECYCLEMONITOR_H
