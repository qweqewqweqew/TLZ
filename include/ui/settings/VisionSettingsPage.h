#ifndef VISIONSETTINGSPAGE_H
#define VISIONSETTINGSPAGE_H

#include "camera/HikCameraConfig.h"
#include "ui/settings/SettingsSubPage.h"

class ElaCheckBox;
class QLineEdit;
class QSpinBox;

class VisionSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit VisionSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;
    bool isEditable() const override { return true; }
    bool isDirty() const override;
    SettingsSaveOutcome save() override;
    void revert() override;
    void restoreDefaults() override;
    std::optional<Permission> writePermission() const override
    {
        return Permission::EditCameraConfig;
    }
    void setWriteAllowed(bool allowed) override;
    QString pendingChangeSummary() const override;

protected:
    void reloadFromSource() override;

private:
    void setConfig(const HikCameraConfig &config);
    HikCameraConfig currentConfig() const;
    void notifyDirty();

    HikCameraConfig m_loaded;
    QWidget *m_form{nullptr};
    ElaCheckBox *m_enabled{nullptr};
    QLineEdit *m_address{nullptr};
    QSpinBox *m_port{nullptr};
    QLineEdit *m_username{nullptr};
    QLineEdit *m_password{nullptr};
    QSpinBox *m_channel{nullptr};
    QSpinBox *m_streamType{nullptr};
    QSpinBox *m_linkMode{nullptr};
    QSpinBox *m_loginTimeout{nullptr};
    QSpinBox *m_reconnect{nullptr};
};

#endif // VISIONSETTINGSPAGE_H
