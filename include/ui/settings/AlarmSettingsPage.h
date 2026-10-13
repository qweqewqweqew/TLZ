#ifndef ALARMSETTINGSPAGE_H
#define ALARMSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class EventLogPanel;
class NodeLifecycleMonitor;
class QPlainTextEdit;

class AlarmSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit AlarmSettingsPage(NodeLifecycleMonitor *nodes,
                               EventLogPanel *eventLog,
                               QWidget *parent = nullptr);

    QString bindingSummary() const override;

protected:
    void reloadFromSource() override;

private:
    void runSelfCheck();

    NodeLifecycleMonitor *m_nodes{nullptr};
    EventLogPanel *m_eventLog{nullptr};
    QPlainTextEdit *m_selfCheck{nullptr};
    QPlainTextEdit *m_eventSnapshot{nullptr};
};

#endif // ALARMSETTINGSPAGE_H
