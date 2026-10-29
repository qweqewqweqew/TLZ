#include "auth/AuthService.h"

#include "app/Logger.h"
#include "auth/AccountSchema.h"
#include "auth/PasswordHasher.h"
#include "auth/PasswordPolicy.h"

#include <QCoreApplication>
#include <QEvent>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVariant>

namespace {

constexpr int kIdleCheckIntervalMs = 15000;
constexpr int kActivityPersistIntervalSecs = 60;
constexpr const char *kSessionTimeoutKey = "Auth.SessionTimeoutMinutes";

QString invalidCredentialsMessage()
{
    return QStringLiteral("用户名或密码错误。");
}

QString toDbTime(const QDateTime &time)
{
    return time.isValid() ? time.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QVariant toDbTimeOrNull(const QDateTime &time)
{
    return time.isValid() ? QVariant(toDbTime(time)) : QVariant(QVariant::String);
}

QDateTime fromDbTime(const QVariant &value)
{
    if (value.isNull()) {
        return QDateTime();
    }
    const QString text = value.toString();
    QDateTime time = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!time.isValid()) {
        time = QDateTime::fromString(text, Qt::ISODate);
    }
    if (time.isValid() && time.timeSpec() == Qt::LocalTime) {
        time.setTimeSpec(Qt::UTC);
    }
    return time;
}

const PasswordHasher::Digest &timingDummyDigest()
{
    static const PasswordHasher::Digest digest =
        PasswordHasher::hashPassword(QStringLiteral("timing-equalizer-not-a-real-account"));
    return digest;
}

const char *kUserColumns =
    "Id, Username, Role, PasswordHash, PasswordSalt, PasswordIterations, Enabled,"
    " MustChangePassword, FailedLoginCount, LockedUntil, LastLoginAt, CreatedAt, CreatedBy";

} // namespace

AuthService &AuthService::instance()
{
    static AuthService *service = new AuthService(QCoreApplication::instance());
    return *service;
}

AuthService::AuthService(QObject *parent)
    : QObject(parent)
    , m_clock([]() { return QDateTime::currentDateTimeUtc(); })
{
    m_idleTimer = new QTimer(this);
    m_idleTimer->setInterval(kIdleCheckIntervalMs);
    connect(m_idleTimer, &QTimer::timeout, this, &AuthService::checkIdleTimeout);
    m_idleTimer->start();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installEventFilter(this);
    }
}

AuthService::~AuthService()
{
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeEventFilter(this);
    }
}

bool AuthService::attachDatabase(const QString &connectionName, QString *errorMessage)
{
    m_connectionName = connectionName;
    QSqlDatabase db = database();
    if (!db.isOpen()) {
        m_available = false;
        if (errorMessage) {
            *errorMessage = QStringLiteral("数据库未打开，账户功能不可用。");
        }
        return false;
    }
    QString schemaError;
    if (!AccountSchema::ensure(db, &schemaError)) {
        m_available = false;
        if (errorMessage) {
            *errorMessage = schemaError;
        }
        return false;
    }

    m_available = true;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT Value FROM AppSetting WHERE Key = :key"));
    query.bindValue(QStringLiteral(":key"), QString::fromLatin1(kSessionTimeoutKey));
    if (query.exec() && query.next()) {
        bool ok = false;
        const int minutes = query.value(0).toInt(&ok);
        if (ok && minutes >= kMinSessionTimeoutMinutes && minutes <= kMaxSessionTimeoutMinutes) {
            m_sessionTimeoutMinutes = minutes;
        }
    }
    emit accountsChanged();
    emit sessionChanged();
    return true;
}

void AuthService::shutdown()
{
    endSession(QStringLiteral("app_exit"));
    m_available = false;
    m_connectionName.clear();
}

bool AuthService::isAvailable() const
{
    return m_available;
}

bool AuthService::hasAccounts() const
{
    if (!m_available) {
        return false;
    }
    QSqlQuery query(database());
    return query.exec(QStringLiteral("SELECT 1 FROM UserAccount LIMIT 1")) && query.next();
}

bool AuthService::isLoggedIn() const
{
    return m_userId != 0;
}

QString AuthService::currentUsername() const
{
    return m_username;
}

Role AuthService::currentRole() const
{
    return m_role;
}

bool AuthService::can(Permission permission) const
{
    return m_available && isLoggedIn() && Auth::roleAllows(m_role, permission);
}

QString AuthService::identityText() const
{
    if (!isLoggedIn()) {
        return QStringLiteral("未登录");
    }
    return QStringLiteral("%1（%2）").arg(m_username, Auth::roleDisplayName(m_role));
}

AuthResult AuthService::createInitialAdministrator(const QString &usernameInput,
                                                   const QString &password)
{
    if (!m_available) {
        return {false, QStringLiteral("数据库未就绪，无法创建管理员。")};
    }
    if (hasAccounts()) {
        return {false, QStringLiteral("系统中已经有账户，不能重复初始化。请用已有管理员登录。")};
    }
    const QString username = usernameInput.trimmed();
    QString problem = PasswordPolicy::validateUsername(username);
    if (problem.isEmpty()) {
        problem = PasswordPolicy::validatePassword(username, password);
    }
    if (!problem.isEmpty()) {
        return {false, problem};
    }

    const PasswordHasher::Digest digest = PasswordHasher::hashPassword(password);
    const QString stamp = toDbTime(now());
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "INSERT INTO UserAccount (Username, Role, PasswordHash, PasswordSalt, PasswordIterations,"
        " Enabled, MustChangePassword, FailedLoginCount, PasswordChangedAt, CreatedAt, CreatedBy,"
        " UpdatedAt)"
        " SELECT :username, 'administrator', :hash, :salt, :iterations, 1, 0, 0, :stamp, :stamp,"
        " '', :stamp WHERE NOT EXISTS (SELECT 1 FROM UserAccount)"));
    query.bindValue(QStringLiteral(":username"), username);
    query.bindValue(QStringLiteral(":hash"), QString::fromLatin1(digest.hash.toBase64()));
    query.bindValue(QStringLiteral(":salt"), QString::fromLatin1(digest.salt.toBase64()));
    query.bindValue(QStringLiteral(":iterations"), digest.iterations);
    query.bindValue(QStringLiteral(":stamp"), stamp);
    if (!query.exec()) {
        return {false, QStringLiteral("写入账户失败: %1").arg(query.lastError().text())};
    }
    if (query.numRowsAffected() != 1) {
        return {false, QStringLiteral("系统中已经有账户，不能重复初始化。")};
    }

    writeAudit(username, Role::Administrator, QStringLiteral("account.bootstrap"), username,
               QStringLiteral("首次启动创建管理员"), true);
    StoredUser user;
    QString error;
    if (findUser(username, &user, &error)) {
        startSession(user);
    }
    emit accountsChanged();
    return {true, QStringLiteral("管理员 %1 已创建并登录。").arg(username)};
}

LoginResult AuthService::login(const QString &usernameInput, const QString &password)
{
    if (!m_available) {
        return {LoginStatus::Unavailable, QStringLiteral("数据库未就绪，暂时无法登录。")};
    }
    const QString username = usernameInput.trimmed();
    if (username.isEmpty() || password.isEmpty()) {
        return {LoginStatus::InvalidCredentials, QStringLiteral("请输入用户名和密码。")};
    }
    const QDateTime current = now();

    StoredUser user;
    QString error;
    if (!findUser(username, &user, &error)) {
        if (!error.isEmpty()) {
            return {LoginStatus::Unavailable, QStringLiteral("读取账户失败: %1").arg(error)};
        }
        // 不存在的用户名同样计数和锁定，避免通过提示差异枚举账户。
        if (m_unknownUsers.size() >= kMaxUnknownUserEntries) {
            for (auto it = m_unknownUsers.begin(); it != m_unknownUsers.end();) {
                if (!it->lockedUntil.isValid() || it->lockedUntil <= current) {
                    it = m_unknownUsers.erase(it);
                } else {
                    ++it;
                }
            }
        }
        UnknownUserThrottle &throttle = m_unknownUsers[username.toLower()];
        if (throttle.lockedUntil.isValid() && throttle.lockedUntil > current) {
            writeAudit(username, Role::None, QStringLiteral("login"), username,
                       QStringLiteral("锁定期内再次尝试"), false);
            return {LoginStatus::Locked, lockedMessage(throttle.lockedUntil)};
        }
        PasswordHasher::verifyPassword(password, timingDummyDigest());
        ++throttle.failures;
        const bool nowLocked = throttle.failures >= kMaxFailedAttempts;
        if (nowLocked) {
            throttle.failures = 0;
            throttle.lockedUntil = current.addSecs(qint64(kLockoutMinutes) * 60);
        }
        writeAudit(username, Role::None, QStringLiteral("login"), username,
                   nowLocked ? QStringLiteral("用户名不存在，连续失败已锁定")
                             : QStringLiteral("用户名不存在"),
                   false);
        if (nowLocked) {
            return {LoginStatus::Locked, lockedMessage(throttle.lockedUntil)};
        }
        return {LoginStatus::InvalidCredentials, invalidCredentialsMessage()};
    }

    const UserRecord &record = user.record;
    if (record.lockedUntil.isValid() && record.lockedUntil > current) {
        writeAudit(record.username, record.role, QStringLiteral("login"), record.username,
                   QStringLiteral("锁定期内再次尝试"), false);
        return {LoginStatus::Locked, lockedMessage(record.lockedUntil)};
    }

    if (!verify(user, password)) {
        bool nowLocked = false;
        recordFailure(user, &nowLocked);
        writeAudit(record.username, record.role, QStringLiteral("login"), record.username,
                   nowLocked ? QStringLiteral("密码错误，连续 %1 次已锁定 %2 分钟")
                                   .arg(kMaxFailedAttempts)
                                   .arg(kLockoutMinutes)
                             : QStringLiteral("密码错误"),
                   false);
        if (nowLocked) {
            return {LoginStatus::Locked,
                    lockedMessage(current.addSecs(qint64(kLockoutMinutes) * 60))};
        }
        return {LoginStatus::InvalidCredentials, invalidCredentialsMessage()};
    }

    clearFailures(record.id);
    if (!record.enabled) {
        writeAudit(record.username, record.role, QStringLiteral("login"), record.username,
                   QStringLiteral("账户已停用"), false);
        return {LoginStatus::InvalidCredentials, QStringLiteral("该账户已停用，请联系管理员。")};
    }
    if (user.iterations < PasswordHasher::kDefaultIterations) {
        storePassword(record.id, password, record.mustChangePassword, nullptr);
    }
    if (record.mustChangePassword) {
        writeAudit(record.username, record.role, QStringLiteral("login"), record.username,
                   QStringLiteral("密码正确，需先修改密码"), true);
        return {LoginStatus::PasswordChangeRequired,
                QStringLiteral("首次登录或密码已被管理员重置，请先设置新密码。")};
    }

    startSession(user);
    return {LoginStatus::Success, QStringLiteral("已登录：%1").arg(identityText())};
}

AuthResult AuthService::completeRequiredPasswordChange(const QString &usernameInput,
                                                       const QString &currentPassword,
                                                       const QString &newPassword)
{
    if (!m_available) {
        return {false, QStringLiteral("数据库未就绪。")};
    }
    const QString username = usernameInput.trimmed();
    StoredUser user;
    QString error;
    if (!findUser(username, &user, &error)) {
        return {false, invalidCredentialsMessage()};
    }
    const QDateTime current = now();
    if (user.record.lockedUntil.isValid() && user.record.lockedUntil > current) {
        return {false, lockedMessage(user.record.lockedUntil)};
    }
    if (!verify(user, currentPassword)) {
        bool nowLocked = false;
        recordFailure(user, &nowLocked);
        writeAudit(user.record.username, user.record.role, QStringLiteral("password.change"),
                   user.record.username, QStringLiteral("当前密码错误"), false);
        return {false, invalidCredentialsMessage()};
    }
    if (!user.record.enabled) {
        return {false, QStringLiteral("该账户已停用，请联系管理员。")};
    }
    if (!user.record.mustChangePassword) {
        return {false, QStringLiteral("该账户不需要强制改密，请直接登录。")};
    }
    const QString problem = PasswordPolicy::validatePassword(user.record.username, newPassword);
    if (!problem.isEmpty()) {
        return {false, problem};
    }
    if (newPassword == currentPassword) {
        return {false, QStringLiteral("新密码不能与当前密码相同。")};
    }
    if (!storePassword(user.record.id, newPassword, false, &error)) {
        return {false, error};
    }
    writeAudit(user.record.username, user.record.role, QStringLiteral("password.change"),
               user.record.username, QStringLiteral("按要求设置了新密码"), true);
    if (findUserById(user.record.id, &user, &error)) {
        startSession(user);
    }
    return {true, QStringLiteral("新密码已生效。")};
}

AuthResult AuthService::changeOwnPassword(const QString &currentPassword, const QString &newPassword)
{
    if (!m_available || !isLoggedIn()) {
        return {false, QStringLiteral("请先登录。")};
    }
    StoredUser user;
    QString error;
    if (!findUserById(m_userId, &user, &error)) {
        return {false, error.isEmpty() ? QStringLiteral("当前账户不存在。") : error};
    }
    if (!verify(user, currentPassword)) {
        bool nowLocked = false;
        recordFailure(user, &nowLocked);
        writeAudit(m_username, m_role, QStringLiteral("password.change"), m_username,
                   nowLocked ? QStringLiteral("当前密码错误，账户已锁定")
                             : QStringLiteral("当前密码错误"),
                   false);
        if (nowLocked) {
            endSession(QStringLiteral("locked"));
            return {false, QStringLiteral("当前密码连续输错，账户已锁定 %1 分钟并退出登录。")
                               .arg(kLockoutMinutes)};
        }
        return {false, QStringLiteral("当前密码不正确。")};
    }
    const QString problem = PasswordPolicy::validatePassword(m_username, newPassword);
    if (!problem.isEmpty()) {
        return {false, problem};
    }
    if (newPassword == currentPassword) {
        return {false, QStringLiteral("新密码不能与当前密码相同。")};
    }
    clearFailures(m_userId);
    if (!storePassword(m_userId, newPassword, false, &error)) {
        return {false, error};
    }
    writeAudit(m_username, m_role, QStringLiteral("password.change"), m_username,
               QStringLiteral("用户修改了自己的密码"), true);
    return {true, QStringLiteral("密码已修改。")};
}

void AuthService::logout()
{
    endSession(QStringLiteral("logout"));
}

void AuthService::noteActivity()
{
    if (!isLoggedIn()) {
        return;
    }
    m_lastActivity = now();
    if (m_sessionId == 0 || !m_available) {
        return;
    }
    if (m_lastActivityPersisted.isValid()
        && m_lastActivityPersisted.secsTo(m_lastActivity) < kActivityPersistIntervalSecs) {
        return;
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral("UPDATE UserSession SET LastActivityAt = :t WHERE Id = :id"));
    query.bindValue(QStringLiteral(":t"), toDbTime(m_lastActivity));
    query.bindValue(QStringLiteral(":id"), m_sessionId);
    if (query.exec()) {
        m_lastActivityPersisted = m_lastActivity;
    }
}

int AuthService::sessionTimeoutMinutes() const
{
    return m_sessionTimeoutMinutes;
}

AuthResult AuthService::setSessionTimeoutMinutes(int minutes)
{
    const QString target = QStringLiteral("会话超时");
    if (!can(Permission::EditSecurityPolicy)) {
        writeAudit(m_username, m_role, QStringLiteral("security.session_timeout"), target,
                   QStringLiteral("权限不足"), false);
        return {false, QStringLiteral("需要管理员权限才能修改会话超时。")};
    }
    if (minutes < kMinSessionTimeoutMinutes || minutes > kMaxSessionTimeoutMinutes) {
        return {false, QStringLiteral("会话超时须在 %1~%2 分钟之间。")
                           .arg(kMinSessionTimeoutMinutes)
                           .arg(kMaxSessionTimeoutMinutes)};
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "INSERT INTO AppSetting (Key, Value, UpdatedAt) VALUES (:key, :value, :t)"
        " ON CONFLICT(Key) DO UPDATE SET Value = excluded.Value, UpdatedAt = excluded.UpdatedAt"));
    query.bindValue(QStringLiteral(":key"), QString::fromLatin1(kSessionTimeoutKey));
    query.bindValue(QStringLiteral(":value"), QString::number(minutes));
    query.bindValue(QStringLiteral(":t"), toDbTime(now()));
    if (!query.exec()) {
        return {false, QStringLiteral("写入会话超时失败: %1").arg(query.lastError().text())};
    }
    const int previous = m_sessionTimeoutMinutes;
    m_sessionTimeoutMinutes = minutes;
    writeAudit(m_username, m_role, QStringLiteral("security.session_timeout"), target,
               QStringLiteral("%1 → %2 分钟").arg(previous).arg(minutes), true);
    return {true, QStringLiteral("会话超时已改为 %1 分钟。").arg(minutes)};
}

QVector<UserRecord> AuthService::listUsers(QString *errorMessage) const
{
    // 用户列表用于登录前的只读展示；所有写操作仍分别由 requireAdmin 保护。
    QVector<UserRecord> users;
    QSqlQuery query(database());
    if (!query.exec(QStringLiteral("SELECT %1 FROM UserAccount ORDER BY Username COLLATE NOCASE")
                        .arg(QLatin1String(kUserColumns)))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return users;
    }
    while (query.next()) {
        UserRecord record;
        record.id = query.value(0).toInt();
        record.username = query.value(1).toString();
        record.role = Auth::roleFromKey(query.value(2).toString());
        record.enabled = query.value(6).toInt() != 0;
        record.mustChangePassword = query.value(7).toInt() != 0;
        record.failedLoginCount = query.value(8).toInt();
        record.lockedUntil = fromDbTime(query.value(9));
        record.lastLoginAt = fromDbTime(query.value(10));
        record.createdAt = fromDbTime(query.value(11));
        record.createdBy = query.value(12).toString();
        users.append(record);
    }
    return users;
}

AuthResult AuthService::createUser(const QString &usernameInput,
                                   Role role,
                                   const QString &initialPassword)
{
    const QString username = usernameInput.trimmed();
    const AuthResult allowed = requireAdmin(QStringLiteral("account.create"), username);
    if (!allowed.ok) {
        return allowed;
    }
    if (role == Role::None) {
        return {false, QStringLiteral("请选择角色。")};
    }
    if (role == Role::Administrator) {
        return {false, QStringLiteral("系统只允许一个管理员，不能再新建管理员账户。")};
    }
    QString problem = PasswordPolicy::validateUsername(username);
    if (problem.isEmpty()) {
        problem = PasswordPolicy::validatePassword(username, initialPassword);
    }
    if (!problem.isEmpty()) {
        return {false, problem};
    }
    StoredUser existing;
    QString error;
    if (findUser(username, &existing, &error)) {
        return {false, QStringLiteral("用户名已存在。")};
    }
    if (!insertUser(username, role, initialPassword, true, m_username, &error)) {
        return {false, error};
    }
    writeAudit(m_username, m_role, QStringLiteral("account.create"), username,
               QStringLiteral("角色=%1，首次登录须改密").arg(Auth::roleDisplayName(role)), true);
    emit accountsChanged();
    return {true, QStringLiteral("已创建账户 %1（%2）。对方首次登录时必须修改密码。")
                      .arg(username, Auth::roleDisplayName(role))};
}

AuthResult AuthService::registerWithApproval(const QString &adminUsernameInput,
                                             const QString &adminPassword,
                                             const QString &usernameInput,
                                             Role role,
                                             const QString &password)
{
    if (!m_available) {
        return {false, QStringLiteral("数据库未就绪，暂时无法注册。")};
    }
    const QString username = usernameInput.trimmed();
    if (role != Role::Operator && role != Role::Technician) {
        return {false, QStringLiteral("注册只能选择操作员或技术员。")};
    }
    QString problem = PasswordPolicy::validateUsername(username);
    if (problem.isEmpty()) {
        problem = PasswordPolicy::validatePassword(username, password);
    }
    if (!problem.isEmpty()) {
        return {false, problem};
    }
    StoredUser existing;
    QString error;
    if (findUser(username, &existing, &error)) {
        return {false, QStringLiteral("用户名已存在。")};
    }
    if (!error.isEmpty()) {
        return {false, QStringLiteral("读取账户失败: %1").arg(error)};
    }

    // 授权只校验管理员口令，不切换当前会话；输错计入该管理员的失败次数。
    const QString adminName = adminUsernameInput.trimmed();
    const QString denied = QStringLiteral("管理员账号或密码错误，未创建账户。");
    StoredUser admin;
    if (!findUser(adminName, &admin, &error) || admin.record.role != Role::Administrator) {
        PasswordHasher::verifyPassword(adminPassword, timingDummyDigest());
        writeAudit(adminName, Role::None, QStringLiteral("account.register"), username,
                   QStringLiteral("授权失败：不是管理员账号"), false);
        return {false, denied};
    }
    const QDateTime current = now();
    if (admin.record.lockedUntil.isValid() && admin.record.lockedUntil > current) {
        return {false, lockedMessage(admin.record.lockedUntil)};
    }
    if (!verify(admin, adminPassword)) {
        bool nowLocked = false;
        recordFailure(admin, &nowLocked);
        writeAudit(admin.record.username, Role::Administrator, QStringLiteral("account.register"),
                   username, QStringLiteral("授权失败：管理员密码错误"), false);
        if (nowLocked) {
            return {false, lockedMessage(current.addSecs(qint64(kLockoutMinutes) * 60))};
        }
        return {false, denied};
    }
    clearFailures(admin.record.id);
    if (!admin.record.enabled) {
        return {false, denied};
    }

    if (!insertUser(username, role, password, false, admin.record.username, &error)) {
        return {false, error};
    }
    writeAudit(admin.record.username, Role::Administrator, QStringLiteral("account.register"), username,
               QStringLiteral("现场注册，角色=%1，管理员当场授权").arg(Auth::roleDisplayName(role)), true);
    emit accountsChanged();
    return {true, QStringLiteral("账户 %1（%2）已创建，请用新账户登录。")
                      .arg(username, Auth::roleDisplayName(role))};
}

bool AuthService::insertUser(const QString &username,
                             Role role,
                             const QString &password,
                             bool mustChange,
                             const QString &createdBy,
                             QString *errorMessage)
{
    const PasswordHasher::Digest digest = PasswordHasher::hashPassword(password);
    const QString stamp = toDbTime(now());
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "INSERT INTO UserAccount (Username, Role, PasswordHash, PasswordSalt, PasswordIterations,"
        " Enabled, MustChangePassword, FailedLoginCount, PasswordChangedAt, CreatedAt, CreatedBy,"
        " UpdatedAt)"
        " VALUES (:username, :role, :hash, :salt, :iterations, 1, :must, 0, :stamp, :stamp, :by, :stamp)"));
    query.bindValue(QStringLiteral(":username"), username);
    query.bindValue(QStringLiteral(":role"), Auth::roleKey(role));
    query.bindValue(QStringLiteral(":hash"), QString::fromLatin1(digest.hash.toBase64()));
    query.bindValue(QStringLiteral(":salt"), QString::fromLatin1(digest.salt.toBase64()));
    query.bindValue(QStringLiteral(":iterations"), digest.iterations);
    query.bindValue(QStringLiteral(":must"), mustChange ? 1 : 0);
    query.bindValue(QStringLiteral(":stamp"), stamp);
    query.bindValue(QStringLiteral(":by"), createdBy);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写入账户失败: %1").arg(query.lastError().text());
        }
        return false;
    }
    return true;
}

AuthResult AuthService::resetPassword(int userId, const QString &temporaryPassword)
{
    StoredUser user;
    QString error;
    const bool found = findUserById(userId, &user, &error);
    const QString target = found ? user.record.username : QString::number(userId);
    const AuthResult allowed = requireAdmin(QStringLiteral("account.reset_password"), target);
    if (!allowed.ok) {
        return allowed;
    }
    if (!found) {
        return {false, QStringLiteral("账户不存在。")};
    }
    if (userId == m_userId) {
        return {false, QStringLiteral("不能在这里重置自己的密码，请使用「修改密码」。")};
    }
    const QString problem = PasswordPolicy::validatePassword(user.record.username, temporaryPassword);
    if (!problem.isEmpty()) {
        return {false, problem};
    }
    if (!storePassword(userId, temporaryPassword, true, &error)) {
        return {false, error};
    }
    clearFailures(userId);
    writeAudit(m_username, m_role, QStringLiteral("account.reset_password"), target,
               QStringLiteral("已重置为临时密码，下次登录须改密"), true);
    emit accountsChanged();
    return {true, QStringLiteral("已重置 %1 的密码。对方下次登录时必须修改密码。").arg(target)};
}

AuthResult AuthService::setRole(int userId, Role role)
{
    StoredUser user;
    QString error;
    const bool found = findUserById(userId, &user, &error);
    const QString target = found ? user.record.username : QString::number(userId);
    const AuthResult allowed = requireAdmin(QStringLiteral("account.set_role"), target);
    if (!allowed.ok) {
        return allowed;
    }
    if (!found) {
        return {false, QStringLiteral("账户不存在。")};
    }
    if (role == Role::None) {
        return {false, QStringLiteral("请选择角色。")};
    }
    if (role == Role::Administrator) {
        return {false, QStringLiteral("系统只允许一个管理员，不能把其他账户提升为管理员。")};
    }
    if (userId == m_userId) {
        return {false, QStringLiteral("不能修改管理员自己的角色。")};
    }
    if (user.record.role == role) {
        return {true, QStringLiteral("角色没有变化。")};
    }
    if (user.record.role == Role::Administrator && user.record.enabled
        && countEnabledAdministrators(userId) == 0) {
        return {false, QStringLiteral("至少要保留一个启用的管理员。")};
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral("UPDATE UserAccount SET Role = :role, UpdatedAt = :t WHERE Id = :id"));
    query.bindValue(QStringLiteral(":role"), Auth::roleKey(role));
    query.bindValue(QStringLiteral(":t"), toDbTime(now()));
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        return {false, QStringLiteral("修改角色失败: %1").arg(query.lastError().text())};
    }
    writeAudit(m_username, m_role, QStringLiteral("account.set_role"), target,
               QStringLiteral("%1 → %2").arg(Auth::roleDisplayName(user.record.role),
                                             Auth::roleDisplayName(role)),
               true);
    emit accountsChanged();
    return {true, QStringLiteral("%1 的角色已改为%2。").arg(target, Auth::roleDisplayName(role))};
}

AuthResult AuthService::setEnabled(int userId, bool enabled)
{
    StoredUser user;
    QString error;
    const bool found = findUserById(userId, &user, &error);
    const QString target = found ? user.record.username : QString::number(userId);
    const AuthResult allowed = requireAdmin(
        enabled ? QStringLiteral("account.enable") : QStringLiteral("account.disable"), target);
    if (!allowed.ok) {
        return allowed;
    }
    if (!found) {
        return {false, QStringLiteral("账户不存在。")};
    }
    if (!enabled && userId == m_userId) {
        return {false, QStringLiteral("不能停用当前登录的账户。")};
    }
    if (user.record.enabled == enabled) {
        return {true, enabled ? QStringLiteral("账户已经是启用状态。")
                              : QStringLiteral("账户已经是停用状态。")};
    }
    if (!enabled && user.record.role == Role::Administrator
        && countEnabledAdministrators(userId) == 0) {
        return {false, QStringLiteral("至少要保留一个启用的管理员。")};
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral("UPDATE UserAccount SET Enabled = :e, UpdatedAt = :t WHERE Id = :id"));
    query.bindValue(QStringLiteral(":e"), enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":t"), toDbTime(now()));
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        return {false, QStringLiteral("修改账户状态失败: %1").arg(query.lastError().text())};
    }
    writeAudit(m_username, m_role,
               enabled ? QStringLiteral("account.enable") : QStringLiteral("account.disable"),
               target, enabled ? QStringLiteral("启用") : QStringLiteral("停用"), true);
    emit accountsChanged();
    return {true, enabled ? QStringLiteral("已启用 %1。").arg(target)
                          : QStringLiteral("已停用 %1。").arg(target)};
}

AuthResult AuthService::unlock(int userId)
{
    StoredUser user;
    QString error;
    const bool found = findUserById(userId, &user, &error);
    const QString target = found ? user.record.username : QString::number(userId);
    const AuthResult allowed = requireAdmin(QStringLiteral("account.unlock"), target);
    if (!allowed.ok) {
        return allowed;
    }
    if (!found) {
        return {false, QStringLiteral("账户不存在。")};
    }
    clearFailures(userId);
    writeAudit(m_username, m_role, QStringLiteral("account.unlock"), target,
               QStringLiteral("清除失败次数与锁定"), true);
    emit accountsChanged();
    return {true, QStringLiteral("已解锁 %1。").arg(target)};
}

AuthResult AuthService::deleteUser(int userId)
{
    StoredUser user;
    QString error;
    const bool found = findUserById(userId, &user, &error);
    const QString target = found ? user.record.username : QString::number(userId);
    const AuthResult allowed = requireAdmin(QStringLiteral("account.delete"), target);
    if (!allowed.ok) {
        return allowed;
    }
    if (!found) {
        return {false, QStringLiteral("账户不存在。")};
    }
    if (userId == m_userId) {
        return {false, QStringLiteral("不能删除当前登录的账户。")};
    }
    if (user.record.role == Role::Administrator) {
        return {false, QStringLiteral("不能删除管理员账户。")};
    }

    QSqlQuery sessionQuery(database());
    sessionQuery.prepare(QStringLiteral("DELETE FROM UserSession WHERE UserId = :id"));
    sessionQuery.bindValue(QStringLiteral(":id"), userId);
    sessionQuery.exec();

    QSqlQuery query(database());
    query.prepare(QStringLiteral("DELETE FROM UserAccount WHERE Id = :id"));
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        return {false, QStringLiteral("删除账户失败: %1").arg(query.lastError().text())};
    }
    writeAudit(m_username, m_role, QStringLiteral("account.delete"), target,
               QStringLiteral("删除账户"), true);
    emit accountsChanged();
    return {true, QStringLiteral("已删除账户 %1。").arg(target)};
}

QVector<AuditEntry> AuthService::recentAudit(int limit, QString *errorMessage) const
{
    QVector<AuditEntry> entries;
    if (!can(Permission::ViewAuditLog)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("需要管理员权限。");
        }
        return entries;
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "SELECT OccurredAt, Username, Role, Action, Target, Detail, Success"
        " FROM AuditLog ORDER BY Id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), qBound(1, limit, 5000));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return entries;
    }
    while (query.next()) {
        AuditEntry entry;
        entry.occurredAt = fromDbTime(query.value(0));
        entry.username = query.value(1).toString();
        entry.role = Auth::roleFromKey(query.value(2).toString());
        entry.action = query.value(3).toString();
        entry.target = query.value(4).toString();
        entry.detail = query.value(5).toString();
        entry.success = query.value(6).toInt() != 0;
        entries.append(entry);
    }
    return entries;
}

void AuthService::audit(const QString &action,
                        const QString &target,
                        const QString &detail,
                        bool success)
{
    writeAudit(m_username, m_role, action, target, detail, success);
}

void AuthService::setClockForTesting(std::function<QDateTime()> clock)
{
    m_clock = std::move(clock);
}

void AuthService::checkIdleTimeout()
{
    if (!isLoggedIn() || !m_lastActivity.isValid()) {
        return;
    }
    if (m_lastActivity.secsTo(now()) >= qint64(m_sessionTimeoutMinutes) * 60) {
        endSession(QStringLiteral("timeout"));
        emit sessionExpired();
    }
}

bool AuthService::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        noteActivity();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

QSqlDatabase AuthService::database() const
{
    if (m_connectionName.isEmpty() || !QSqlDatabase::contains(m_connectionName)) {
        return QSqlDatabase();
    }
    return QSqlDatabase::database(m_connectionName, false);
}

QDateTime AuthService::now() const
{
    return m_clock().toUTC();
}

bool AuthService::findUser(const QString &username, StoredUser *user, QString *errorMessage) const
{
    if (!m_available) {
        return false;
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT %1 FROM UserAccount WHERE Username = :username")
                      .arg(QLatin1String(kUserColumns)));
    query.bindValue(QStringLiteral(":username"), username);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    if (!query.next()) {
        return false;
    }
    user->record.id = query.value(0).toInt();
    user->record.username = query.value(1).toString();
    user->record.role = Auth::roleFromKey(query.value(2).toString());
    user->hash = QByteArray::fromBase64(query.value(3).toString().toLatin1());
    user->salt = QByteArray::fromBase64(query.value(4).toString().toLatin1());
    user->iterations = query.value(5).toInt();
    user->record.enabled = query.value(6).toInt() != 0;
    user->record.mustChangePassword = query.value(7).toInt() != 0;
    user->record.failedLoginCount = query.value(8).toInt();
    user->record.lockedUntil = fromDbTime(query.value(9));
    user->record.lastLoginAt = fromDbTime(query.value(10));
    user->record.createdAt = fromDbTime(query.value(11));
    user->record.createdBy = query.value(12).toString();
    return true;
}

bool AuthService::findUserById(int userId, StoredUser *user, QString *errorMessage) const
{
    if (!m_available) {
        return false;
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT Username FROM UserAccount WHERE Id = :id"));
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    if (!query.next()) {
        return false;
    }
    return findUser(query.value(0).toString(), user, errorMessage);
}

bool AuthService::verify(const StoredUser &user, const QString &password) const
{
    PasswordHasher::Digest stored;
    stored.hash = user.hash;
    stored.salt = user.salt;
    stored.iterations = user.iterations;
    return PasswordHasher::verifyPassword(password, stored);
}

bool AuthService::storePassword(int userId,
                                const QString &password,
                                bool mustChange,
                                QString *errorMessage)
{
    const PasswordHasher::Digest digest = PasswordHasher::hashPassword(password);
    const QString stamp = toDbTime(now());
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "UPDATE UserAccount SET PasswordHash = :hash, PasswordSalt = :salt,"
        " PasswordIterations = :iterations, MustChangePassword = :must,"
        " PasswordChangedAt = :t, UpdatedAt = :t WHERE Id = :id"));
    query.bindValue(QStringLiteral(":hash"), QString::fromLatin1(digest.hash.toBase64()));
    query.bindValue(QStringLiteral(":salt"), QString::fromLatin1(digest.salt.toBase64()));
    query.bindValue(QStringLiteral(":iterations"), digest.iterations);
    query.bindValue(QStringLiteral(":must"), mustChange ? 1 : 0);
    query.bindValue(QStringLiteral(":t"), stamp);
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写入密码失败: %1").arg(query.lastError().text());
        }
        return false;
    }
    return true;
}

bool AuthService::recordFailure(const StoredUser &user, bool *nowLocked)
{
    int failures = user.record.failedLoginCount + 1;
    QDateTime lockedUntil;
    const bool locked = failures >= kMaxFailedAttempts;
    if (locked) {
        failures = 0;
        lockedUntil = now().addSecs(qint64(kLockoutMinutes) * 60);
    }
    if (nowLocked) {
        *nowLocked = locked;
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "UPDATE UserAccount SET FailedLoginCount = :f, LockedUntil = :l, UpdatedAt = :t"
        " WHERE Id = :id"));
    query.bindValue(QStringLiteral(":f"), failures);
    query.bindValue(QStringLiteral(":l"), toDbTimeOrNull(lockedUntil));
    query.bindValue(QStringLiteral(":t"), toDbTime(now()));
    query.bindValue(QStringLiteral(":id"), user.record.id);
    if (!query.exec()) {
        LOG("[账户] 记录登录失败次数失败: %s", query.lastError().text().toUtf8().constData());
        return false;
    }
    if (locked) {
        emit accountsChanged();
    }
    return true;
}

void AuthService::clearFailures(int userId)
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "UPDATE UserAccount SET FailedLoginCount = 0, LockedUntil = NULL"
        " WHERE Id = :id AND (FailedLoginCount <> 0 OR LockedUntil IS NOT NULL)"));
    query.bindValue(QStringLiteral(":id"), userId);
    query.exec();
}

int AuthService::countEnabledAdministrators(int excludingUserId) const
{
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM UserAccount WHERE Role = 'administrator' AND Enabled = 1"
        " AND Id <> :id"));
    query.bindValue(QStringLiteral(":id"), excludingUserId);
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

void AuthService::startSession(const StoredUser &user)
{
    if (isLoggedIn()) {
        endSession(QStringLiteral("switch_user"));
    }
    const QDateTime current = now();
    QSqlDatabase db = database();

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO UserSession (UserId, StartedAt, LastActivityAt) VALUES (:id, :t, :t)"));
    insert.bindValue(QStringLiteral(":id"), user.record.id);
    insert.bindValue(QStringLiteral(":t"), toDbTime(current));
    m_sessionId = insert.exec() ? insert.lastInsertId().toInt() : 0;

    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE UserAccount SET LastLoginAt = :t WHERE Id = :id"));
    update.bindValue(QStringLiteral(":t"), toDbTime(current));
    update.bindValue(QStringLiteral(":id"), user.record.id);
    update.exec();

    m_userId = user.record.id;
    m_username = user.record.username;
    m_role = user.record.role;
    m_lastActivity = current;
    m_lastActivityPersisted = current;
    writeAudit(m_username, m_role, QStringLiteral("login"), m_username,
               QStringLiteral("登录成功"), true);
    LOG("[账户] 登录: user=%s role=%s",
        m_username.toUtf8().constData(),
        Auth::roleKey(m_role).toUtf8().constData());
    emit sessionChanged();
}

void AuthService::endSession(const QString &reason)
{
    if (!isLoggedIn()) {
        return;
    }
    const QDateTime current = now();
    if (m_sessionId != 0 && m_available) {
        QSqlQuery query(database());
        query.prepare(QStringLiteral(
            "UPDATE UserSession SET EndedAt = :t, EndReason = :reason, LastActivityAt = :last"
            " WHERE Id = :id"));
        query.bindValue(QStringLiteral(":t"), toDbTime(current));
        query.bindValue(QStringLiteral(":reason"), reason);
        query.bindValue(QStringLiteral(":last"), toDbTime(m_lastActivity));
        query.bindValue(QStringLiteral(":id"), m_sessionId);
        query.exec();
    }

    QString detail = QStringLiteral("主动退出");
    QString action = QStringLiteral("logout");
    if (reason == QLatin1String("timeout")) {
        action = QStringLiteral("session.timeout");
        detail = QStringLiteral("空闲超过 %1 分钟自动退出").arg(m_sessionTimeoutMinutes);
    } else if (reason == QLatin1String("switch_user")) {
        detail = QStringLiteral("切换账户");
    } else if (reason == QLatin1String("app_exit")) {
        detail = QStringLiteral("程序退出");
    } else if (reason == QLatin1String("locked")) {
        detail = QStringLiteral("账户被锁定");
    }
    writeAudit(m_username, m_role, action, m_username, detail, true);
    LOG("[账户] 退出: user=%s reason=%s",
        m_username.toUtf8().constData(),
        reason.toUtf8().constData());

    m_userId = 0;
    m_username.clear();
    m_role = Role::None;
    m_sessionId = 0;
    m_lastActivity = QDateTime();
    m_lastActivityPersisted = QDateTime();
    emit sessionChanged();
}

AuthResult AuthService::requireAdmin(const QString &action, const QString &target)
{
    if (!m_available) {
        return {false, QStringLiteral("数据库未就绪。")};
    }
    if (!can(Permission::ManageAccounts)) {
        writeAudit(m_username, m_role, action, target, QStringLiteral("权限不足"), false);
        return {false, QStringLiteral("需要管理员权限。")};
    }
    return {true, QString()};
}

QString AuthService::lockedMessage(const QDateTime &until) const
{
    const qint64 seconds = qMax<qint64>(0, now().secsTo(until));
    const qint64 minutes = qMax<qint64>(1, (seconds + 59) / 60);
    return QStringLiteral("登录失败次数过多，已临时锁定，请 %1 分钟后再试。").arg(minutes);
}

void AuthService::writeAudit(const QString &username,
                             Role role,
                             const QString &action,
                             const QString &target,
                             const QString &detail,
                             bool success)
{
    if (!m_available) {
        return;
    }
    QSqlQuery query(database());
    query.prepare(QStringLiteral(
        "INSERT INTO AuditLog (OccurredAt, Username, Role, Action, Target, Detail, Success)"
        " VALUES (:t, :u, :r, :a, :target, :d, :s)"));
    query.bindValue(QStringLiteral(":t"), toDbTime(now()));
    query.bindValue(QStringLiteral(":u"), username);
    query.bindValue(QStringLiteral(":r"), Auth::roleKey(role));
    query.bindValue(QStringLiteral(":a"), action);
    query.bindValue(QStringLiteral(":target"), target);
    query.bindValue(QStringLiteral(":d"), detail);
    query.bindValue(QStringLiteral(":s"), success ? 1 : 0);
    if (!query.exec()) {
        LOG("[审计] 写入失败: action=%s error=%s",
            action.toUtf8().constData(),
            query.lastError().text().toUtf8().constData());
    }
}
