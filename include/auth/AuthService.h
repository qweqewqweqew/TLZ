#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include "auth/AuthTypes.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class QSqlDatabase;
class QTimer;

struct UserRecord
{
    int id{0};
    QString username;
    Role role{Role::None};
    bool enabled{true};
    bool mustChangePassword{false};
    int failedLoginCount{0};
    QDateTime lockedUntil;
    QDateTime lastLoginAt;
    QDateTime createdAt;
    QString createdBy;
};

struct AuditEntry
{
    QDateTime occurredAt;
    QString username;
    Role role{Role::None};
    QString action;
    QString target;
    QString detail;
    bool success{true};
};

enum class LoginStatus {
    Success,
    PasswordChangeRequired,
    InvalidCredentials,
    Locked,
    Unavailable,
};

struct LoginResult
{
    LoginStatus status{LoginStatus::Unavailable};
    QString message;
};

struct AuthResult
{
    bool ok{false};
    QString message;
};

// 本地账户与会话。只在 GUI 线程使用；数据库按连接名取，不长期持有 QSqlDatabase 句柄。
class AuthService : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxFailedAttempts = 5;
    static constexpr int kLockoutMinutes = 15;
    static constexpr int kDefaultSessionTimeoutMinutes = 15;
    static constexpr int kMinSessionTimeoutMinutes = 1;
    static constexpr int kMaxSessionTimeoutMinutes = 240;

    static AuthService &instance();

    explicit AuthService(QObject *parent = nullptr);
    ~AuthService() override;

    bool attachDatabase(const QString &connectionName, QString *errorMessage = nullptr);
    void shutdown();
    bool isAvailable() const;
    bool hasAccounts() const;

    bool isLoggedIn() const;
    QString currentUsername() const;
    Role currentRole() const;
    bool can(Permission permission) const;
    QString identityText() const;

    AuthResult createInitialAdministrator(const QString &username, const QString &password);
    LoginResult login(const QString &username, const QString &password);
    AuthResult completeRequiredPasswordChange(const QString &username,
                                              const QString &currentPassword,
                                              const QString &newPassword);
    AuthResult changeOwnPassword(const QString &currentPassword, const QString &newPassword);
    void logout();
    void noteActivity();

    int sessionTimeoutMinutes() const;
    AuthResult setSessionTimeoutMinutes(int minutes);

    QVector<UserRecord> listUsers(QString *errorMessage = nullptr) const;
    AuthResult createUser(const QString &username, Role role, const QString &initialPassword);
    // 登录界面的「注册」：管理员当场输入自己的账号密码授权，不改变当前会话。
    AuthResult registerWithApproval(const QString &adminUsername,
                                    const QString &adminPassword,
                                    const QString &username,
                                    Role role,
                                    const QString &password);
    AuthResult resetPassword(int userId, const QString &temporaryPassword);
    AuthResult setRole(int userId, Role role);
    AuthResult setEnabled(int userId, bool enabled);
    AuthResult unlock(int userId);
    QVector<AuditEntry> recentAudit(int limit, QString *errorMessage = nullptr) const;

    // 设置页等调用方写审计用。detail 里不得包含密码。
    void audit(const QString &action, const QString &target, const QString &detail, bool success);

    void setClockForTesting(std::function<QDateTime()> clock);
    void checkIdleTimeout();

signals:
    void sessionChanged();
    void sessionExpired();
    void accountsChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct StoredUser
    {
        UserRecord record;
        QByteArray hash;
        QByteArray salt;
        int iterations{0};
    };

    static constexpr int kMaxUnknownUserEntries = 256;

    struct UnknownUserThrottle
    {
        int failures{0};
        QDateTime lockedUntil;
    };

    QSqlDatabase database() const;
    QDateTime now() const;
    bool findUser(const QString &username, StoredUser *user, QString *errorMessage) const;
    bool findUserById(int userId, StoredUser *user, QString *errorMessage) const;
    bool verify(const StoredUser &user, const QString &password) const;
    bool storePassword(int userId, const QString &password, bool mustChange, QString *errorMessage);
    bool insertUser(const QString &username,
                    Role role,
                    const QString &password,
                    bool mustChange,
                    const QString &createdBy,
                    QString *errorMessage);
    bool recordFailure(const StoredUser &user, bool *nowLocked);
    void clearFailures(int userId);
    int countEnabledAdministrators(int excludingUserId) const;
    void startSession(const StoredUser &user);
    void endSession(const QString &reason);
    AuthResult requireAdmin(const QString &action, const QString &target);
    QString lockedMessage(const QDateTime &until) const;
    void writeAudit(const QString &username,
                    Role role,
                    const QString &action,
                    const QString &target,
                    const QString &detail,
                    bool success);

    QString m_connectionName;
    bool m_available{false};
    int m_userId{0};
    QString m_username;
    Role m_role{Role::None};
    int m_sessionId{0};
    QDateTime m_lastActivity;
    QDateTime m_lastActivityPersisted;
    int m_sessionTimeoutMinutes{kDefaultSessionTimeoutMinutes};
    QHash<QString, UnknownUserThrottle> m_unknownUsers;
    std::function<QDateTime()> m_clock;
    QTimer *m_idleTimer{nullptr};
};

#endif // AUTHSERVICE_H
