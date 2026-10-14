#ifndef PROCESSSETTINGSPAGE_H
#define PROCESSSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class BackendParamsDialog;
class Ros2Bridge;
class TeachingDialog;

class ProcessSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit ProcessSettingsPage(Ros2Bridge *bridge, QWidget *parent = nullptr);

    QString bindingSummary() const override;
    bool isEditable() const override { return true; }
    bool isDirty() const override;
    SettingsSaveOutcome save() override;
    void revert() override;
    void restoreDefaults() override;
    std::optional<Permission> writePermission() const override
    {
        return Permission::EditProcessParams;
    }
    void setWriteAllowed(bool allowed) override;
    QString pendingChangeSummary() const override;

protected:
    void reloadFromSource() override;

private:
    Ros2Bridge *m_bridge{nullptr};
    BackendParamsDialog *m_params{nullptr};
    TeachingDialog *m_teaching{nullptr};
};

#endif // PROCESSSETTINGSPAGE_H
