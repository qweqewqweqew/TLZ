#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QFrame>
#include <QPoint>

class QPushButton;

class TitleBar : public QFrame
{
    Q_OBJECT

public:
    enum class WorkspacePage {
        Home,
        History,
        Settings,
    };

    explicit TitleBar(QWidget *parent = nullptr);

    void setMaximizedState(bool maximized);
    void setWorkspacePage(WorkspacePage page);
    void setNodesRunning(bool running);
    void setStartNodesBusy(bool busy);

signals:
    void homeRequested();
    void startNodesRequested();
    void stopNodesRequested();
    void settingsRequested();
    void historyRequested();
    void emergencyStopRequested();
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();
    void dragRequested(const QPoint &globalTopLeft);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPushButton *m_emergencyStopButton{nullptr};
    QPushButton *m_homeButton{nullptr};
    QPushButton *m_historyButton{nullptr};
    QPushButton *m_settingsButton{nullptr};
    QPushButton *m_startNodesButton{nullptr};
    void updateStartNodesButton();

    bool m_nodesRunning{false};
    bool m_startNodesBusy{false};
    QPoint m_dragPosition;
    bool m_dragging{false};
    bool m_maximized{false};
};

#endif // TITLEBAR_H
