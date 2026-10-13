#include "ui/settings/SettingsSubPage.h"

#include <QShowEvent>

SettingsSubPage::SettingsSubPage(QWidget *parent)
    : QWidget(parent)
{
}

SettingsSaveOutcome SettingsSubPage::save()
{
    return {SettingsSaveStatus::Rejected,
            QStringLiteral("本页没有可写项，未写入任何配置。")};
}

void SettingsSubPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!isDirty()) {
        reloadFromSource();
    }
}
