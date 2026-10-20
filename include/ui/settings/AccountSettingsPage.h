#ifndef ACCOUNTSETTINGSPAGE_H
#define ACCOUNTSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class ElaPushButton;
class QLabel;
class QSpinBox;
class QTableWidget;

class AccountSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit AccountSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;
    // 保存区控制本页会话超时策略。
    bool isEditable() const override;
    bool isDirty() const override;
    SettingsSaveOutcome save() override;
    void revert() override;
    void restoreDefaults() override;
    std::optional<Permission> writePermission() const override
    {
        return Permission::EditSecurityPolicy;
    }
    void setWriteAllowed(bool allowed) override;
    QString pendingChangeSummary() const override;

protected:
    void reloadFromSource() override;

private:
    void refreshIdentity();
    void refreshUsers();
    void refreshAudit();
    void updateUserActions();
    int selectedUserId() const;
    QString selectedUsername() const;
    void showResult(bool ok, const QString &message);

    void createUser();
    void resetPassword();
    void changeRole();
    void toggleEnabled();
    void unlockUser();

    QLabel *m_identity{nullptr};
    QLabel *m_identityScope{nullptr};
    ElaPushButton *m_loginButton{nullptr};
    ElaPushButton *m_logoutButton{nullptr};
    ElaPushButton *m_changePasswordButton{nullptr};

    QSpinBox *m_sessionTimeout{nullptr};
    int m_loadedTimeout{0};

    QLabel *m_usersLocked{nullptr};
    QTableWidget *m_users{nullptr};
    ElaPushButton *m_createButton{nullptr};
    ElaPushButton *m_resetButton{nullptr};
    ElaPushButton *m_roleButton{nullptr};
    ElaPushButton *m_enableButton{nullptr};
    ElaPushButton *m_unlockButton{nullptr};

    QLabel *m_auditLocked{nullptr};
    QLabel *m_auditCaption{nullptr};
    ElaPushButton *m_auditRefreshButton{nullptr};
    QTableWidget *m_audit{nullptr};
};

#endif // ACCOUNTSETTINGSPAGE_H
