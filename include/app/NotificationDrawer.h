#ifndef NOTIFICATIONDRAWER_H
#define NOTIFICATIONDRAWER_H

#include <QFrame>

class EventLogPanel;
class QLabel;
class QPropertyAnimation;

// 右侧滑出的通知抽屉：承载报警与事件日志。覆盖在内容区上方，不占布局。
class NotificationDrawer : public QFrame
{
    Q_OBJECT

public:
    explicit NotificationDrawer(QWidget *parent = nullptr);

    EventLogPanel *eventLog() const { return m_log; }
    bool isOpen() const { return m_open; }

    void appendEvent(const QString &level, const QString &message);
    // 未读数：抽屉关闭时累计的 WARN / ERROR 条数
    int unreadCount() const { return m_unread; }

    // 由父窗口在 resize / move 时调用，给出抽屉应占据的区域。
    // 抽屉是顶层浮层窗口，area 使用屏幕全局坐标。
    void setDockArea(const QRect &area);

public slots:
    void open();
    void close();
    void toggle();

signals:
    void unreadChanged(int count);
    void openChanged(bool open);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void moveTo(bool open, bool animated);
    void updateRoundedMask();

    EventLogPanel *m_log{nullptr};
    QLabel *m_subtitle{nullptr};
    QPropertyAnimation *m_animation{nullptr};
    QRect m_dockArea;
    int m_unread{0};
    int m_warnCount{0};
    int m_errorCount{0};
    bool m_open{false};
};

#endif // NOTIFICATIONDRAWER_H
