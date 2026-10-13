#ifndef PROCESSSETTINGSPAGE_H
#define PROCESSSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class BackendParamsDialog;
class Ros2Bridge;

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

protected:
    void reloadFromSource() override;

private:
    Ros2Bridge *m_bridge{nullptr};
    BackendParamsDialog *m_params{nullptr};
};

#endif // PROCESSSETTINGSPAGE_H
