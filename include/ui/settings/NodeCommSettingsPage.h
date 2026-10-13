#ifndef NODECOMMSETTINGSPAGE_H
#define NODECOMMSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class QLabel;
class QPlainTextEdit;

class NodeCommSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit NodeCommSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;

protected:
    void reloadFromSource() override;

private:
    QLabel *m_launcherPath{nullptr};
    QLabel *m_startScript{nullptr};
    QLabel *m_stopScript{nullptr};
    QPlainTextEdit *m_databaseSummary{nullptr};
};

#endif // NODECOMMSETTINGSPAGE_H
