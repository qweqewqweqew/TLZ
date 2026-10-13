#ifndef ACCOUNTSETTINGSPAGE_H
#define ACCOUNTSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class AccountSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit AccountSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;
};

#endif // ACCOUNTSETTINGSPAGE_H
