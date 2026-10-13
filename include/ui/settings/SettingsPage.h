#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>

class EventLogPanel;
class NodeLifecycleMonitor;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class Ros2Bridge;
class SettingsSubPage;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(Ros2Bridge *bridge,
                          NodeLifecycleMonitor *nodes,
                          EventLogPanel *eventLog,
                          QWidget *parent = nullptr);

    // 离开设置或切换子页前调用。保存失败或用户取消时返回 false。
    bool confirmLeave();

signals:
    void historyRequested();
    void logRequested(const QString &level, const QString &message);

private:
    void showPage(int row);
    void updateActions();
    void setStatus(const QString &text, bool problem);
    bool saveCurrent();
    SettingsSubPage *currentPage() const;

    QListWidget *m_nav{nullptr};
    QStackedWidget *m_stack{nullptr};
    QLabel *m_pageTitle{nullptr};
    QLabel *m_status{nullptr};
    QPushButton *m_saveButton{nullptr};
    QPushButton *m_cancelButton{nullptr};
    QPushButton *m_defaultButton{nullptr};
    int m_currentRow{0};
    bool m_updatingNav{false};
};

#endif // SETTINGSPAGE_H
