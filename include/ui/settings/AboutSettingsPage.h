#ifndef ABOUTSETTINGSPAGE_H
#define ABOUTSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class QPlainTextEdit;

class AboutSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit AboutSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;

protected:
    void reloadFromSource() override;

private:
    QPlainTextEdit *m_paths{nullptr};
};

#endif // ABOUTSETTINGSPAGE_H
