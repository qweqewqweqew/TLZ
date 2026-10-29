#ifndef NAVIGATIONBAR_H
#define NAVIGATIONBAR_H

#include "auth/AuthTypes.h"

#include <QAbstractButton>
#include <QFrame>
#include <QPoint>
#include <QString>
#include <QVector>

class QLabel;
class QPushButton;

// 左侧导航的单个条目：图标在左、文字在右，选中时实底高亮。
class NavItemButton : public QAbstractButton
{
    Q_OBJECT

public:
    NavItemButton(const QString &iconResource, const QString &text, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_iconResource;
    bool m_hovered{false};
};

// 主窗口左侧导航：检测与修磨 / 历史查询 / 报表统计 / 系统设置，底部为当前用户。
class NavigationBar : public QFrame
{
    Q_OBJECT

public:
    enum Page {
        Detection = 0,
        History,
        Report,
        Settings,
        PageCount,
    };

    explicit NavigationBar(QWidget *parent = nullptr);

    void setCurrentPage(int page);
    int currentPage() const { return m_current; }
    void setUserIdentity(const QString &username, Role role);
    void applyTheme();
    // true 时开始按钮显示为暂停。真实模式由 PLC 状态驱动，演示模式由本地动画驱动。
    void setMillingRunning(bool running);

signals:
    // 用户点击导航；MainWindow 决定是否真的切换（设置页可能有未保存修改）。
    void pageRequested(int page);
    // 点击底部用户块；参数为弹出菜单的全局锚点。
    void userRequested(const QPoint &menuAnchor);
    void millingStartRequested();
    void millingPauseRequested();
    void millingStopRequested();

private:
    QPushButton *createThemeButton();

    QVector<NavItemButton *> m_items;
    QPushButton *m_themeButton{nullptr};
    QPushButton *m_startButton{nullptr};
    QPushButton *m_stopButton{nullptr};
    QPushButton *m_userButton{nullptr};
    QLabel *m_userAvatar{nullptr};
    QLabel *m_userName{nullptr};
    QLabel *m_userRole{nullptr};
    int m_current{0};
    QString m_username;
    Role m_role{Role::None};
    bool m_millingRunning{false};
};

#endif // NAVIGATIONBAR_H
