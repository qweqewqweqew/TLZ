#ifndef DISPLAYSETTINGSPAGE_H
#define DISPLAYSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class ElaCheckBox;
class QSpinBox;

class DisplaySettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit DisplaySettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;
    bool isEditable() const override { return true; }
    bool isDirty() const override;
    SettingsSaveOutcome save() override;
    void revert() override;
    void restoreDefaults() override;
    std::optional<Permission> writePermission() const override
    {
        return Permission::EditDisplaySettings;
    }
    void setWriteAllowed(bool allowed) override;
    QString pendingChangeSummary() const override;

protected:
    void reloadFromSource() override;

private:
    struct Values
    {
        bool flipX{false};
        int cropLeft{120};
        int cropRight{120};
    };

    void setValues(const Values &values);
    Values currentValues() const;
    QString targetPath() const;
    void notifyDirty();

    Values m_loaded;
    QWidget *m_form{nullptr};
    ElaCheckBox *m_flipX{nullptr};
    QSpinBox *m_cropLeft{nullptr};
    QSpinBox *m_cropRight{nullptr};
};

#endif // DISPLAYSETTINGSPAGE_H
