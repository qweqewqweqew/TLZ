#ifndef TITLEBAR_H
#define TITLEBAR_H

#include "auth/AuthTypes.h"

#include <QFrame>
#include <QPoint>

class QLabel;
class QPushButton;
class QTimer;

// 顶栏：左 Logo，居中系统名，右侧设备状态 / 时间 / 通知 / 用户。
class TitleBar : public QFrame
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setMaximizedState(bool maximized);
    // 节点进程是否在跑，决定「设备在线 / 设备离线」
    void setNodesRunning(bool running);
    void setStartNodesBusy(bool busy);
    void setUserIdentity(const QString &username, Role role);
    // 铃铛上的未读报警数；0 隐藏角标
    void setNotificationCount(int count);
    void applyTheme();

signals:
    void userRequested(const QPoint &menuAnchor);
    void notificationsRequested();
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();
    void dragRequested(const QPoint &globalTopLeft);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPushButton *createUserChip();
    QWidget *createBellButton();
    void updateDevicePill();
    void updateClock();
    void centerTitle();

    QLabel *m_systemTitle{nullptr};
    QLabel *m_brand{nullptr};
    QLabel *m_brandSub{nullptr};
    QLabel *m_userArrow{nullptr};
    QLabel *m_devicePill{nullptr};
    QLabel *m_clock{nullptr};
    QPushButton *m_bellButton{nullptr};
    QLabel *m_bellBadge{nullptr};
    QPushButton *m_userButton{nullptr};
    QLabel *m_userAvatar{nullptr};
    QLabel *m_userName{nullptr};
    QLabel *m_userRole{nullptr};
    QTimer *m_clockTimer{nullptr};

    bool m_nodesRunning{false};
    bool m_startNodesBusy{false};
    QString m_username;
    Role m_role{Role::None};
    QPoint m_dragPosition;
    bool m_dragging{false};
    bool m_maximized{false};
};

#endif // TITLEBAR_H
