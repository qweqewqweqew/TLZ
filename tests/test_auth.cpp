#include "auth/AccountSchema.h"
#include "auth/AuthService.h"
#include "auth/AuthTypes.h"
#include "auth/PasswordHasher.h"
#include "auth/PasswordPolicy.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

namespace {

constexpr const char *kAdminPassword = "Admin2026x";
constexpr const char *kTechPassword = "Tech2026yz";

} // namespace

class AuthTest : public QObject
{
    Q_OBJECT

private slots:
    void pbkdf2MatchesReferenceVectors_data();
    void pbkdf2MatchesReferenceVectors();
    void hashRoundTripUsesRandomSalt();

    void usernamePolicy_data();
    void usernamePolicy();
    void passwordPolicy_data();
    void passwordPolicy();

    void rolesIncludeLowerLevels();

    void init();
    void cleanup();

    void bootstrapCreatesOnlyOneAdministrator();
    void loginAndLogout();
    void wrongPasswordLocksAfterLimit();
    void unknownUsernameIsThrottledLikeRealOne();
    void newAccountMustChangePasswordFirst();
    void onlyAdministratorManagesAccounts();
    void onlyOneAdministratorExists();
    void registerNeedsAdministratorApproval();
    void disabledAccountCannotLogin();
    void idleSessionTimesOut();
    void auditNeverContainsPasswords();

private:
    void bootstrapAdmin();
    void advance(int seconds);

    QString m_connection;
    QDateTime m_now;
    AuthService *m_auth{nullptr};
};

void AuthTest::pbkdf2MatchesReferenceVectors_data()
{
    QTest::addColumn<QByteArray>("password");
    QTest::addColumn<QByteArray>("salt");
    QTest::addColumn<int>("iterations");
    QTest::addColumn<int>("length");
    QTest::addColumn<QByteArray>("expected");

    QTest::newRow("rfc7914-c1") << QByteArray("password") << QByteArray("salt") << 1 << 32
                                << QByteArray("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    QTest::newRow("c2") << QByteArray("password") << QByteArray("salt") << 2 << 32
                        << QByteArray("ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    QTest::newRow("c4096") << QByteArray("password") << QByteArray("salt") << 4096 << 32
                           << QByteArray("c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    QTest::newRow("multi-block") << QByteArray("passwordPASSWORDpassword")
                                 << QByteArray("saltSALTsaltSALTsaltSALTsaltSALTsalt") << 4096 << 40
                                 << QByteArray("348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9");
    QTest::newRow("long-key") << QByteArray(100, 'x') << QByteArray("salt") << 10 << 32
                              << QByteArray("8f2c68b9a0fdc54b1f118d8b998b0ccaee2697047fc972f3b0887a410acc197c");
    QTest::newRow("utf8") << QStringLiteral("密码abc123").toUtf8() << QByteArray("0123456789abcdef")
                          << 1000 << 32
                          << QByteArray("4fbca36d9ef6cec0e42012ebc6fe3e9d64b40dad245f064291434986ff93b92f");
}

void AuthTest::pbkdf2MatchesReferenceVectors()
{
    QFETCH(QByteArray, password);
    QFETCH(QByteArray, salt);
    QFETCH(int, iterations);
    QFETCH(int, length);
    QFETCH(QByteArray, expected);
    QCOMPARE(PasswordHasher::pbkdf2HmacSha256(password, salt, iterations, length).toHex(), expected);
}

void AuthTest::hashRoundTripUsesRandomSalt()
{
    const PasswordHasher::Digest a = PasswordHasher::hashPassword(QStringLiteral("Secret123"), 1000);
    const PasswordHasher::Digest b = PasswordHasher::hashPassword(QStringLiteral("Secret123"), 1000);
    QCOMPARE(a.salt.size(), PasswordHasher::kSaltBytes);
    QCOMPARE(a.hash.size(), PasswordHasher::kDigestBytes);
    QVERIFY(a.salt != b.salt);
    QVERIFY(a.hash != b.hash);
    QVERIFY(PasswordHasher::verifyPassword(QStringLiteral("Secret123"), a));
    QVERIFY(!PasswordHasher::verifyPassword(QStringLiteral("Secret124"), a));
    QVERIFY(!PasswordHasher::verifyPassword(QStringLiteral("secret123"), a));
    QVERIFY(!PasswordHasher::verifyPassword(QString(), a));
}

void AuthTest::usernamePolicy_data()
{
    QTest::addColumn<QString>("username");
    QTest::addColumn<bool>("valid");
    QTest::newRow("ascii") << QStringLiteral("zhang.san") << true;
    QTest::newRow("chinese") << QStringLiteral("张三工") << true;
    QTest::newRow("too-short") << QStringLiteral("ab") << false;
    QTest::newRow("too-long") << QString(33, QLatin1Char('a')) << false;
    QTest::newRow("space-inside") << QStringLiteral("zhang san") << false;
    QTest::newRow("leading-space") << QStringLiteral(" zhang") << false;
    QTest::newRow("sql-quote") << QStringLiteral("a'; DROP TABLE") << false;
    QTest::newRow("empty") << QString() << false;
}

void AuthTest::usernamePolicy()
{
    QFETCH(QString, username);
    QFETCH(bool, valid);
    QCOMPARE(PasswordPolicy::validateUsername(username).isEmpty(), valid);
}

void AuthTest::passwordPolicy_data()
{
    QTest::addColumn<QString>("username");
    QTest::addColumn<QString>("password");
    QTest::addColumn<bool>("valid");
    QTest::newRow("ok") << QStringLiteral("zhangsan") << QStringLiteral("Grind2026") << true;
    QTest::newRow("too-short") << QStringLiteral("zhangsan") << QStringLiteral("Ab12") << false;
    QTest::newRow("no-digit") << QStringLiteral("zhangsan") << QStringLiteral("abcdefghij") << false;
    QTest::newRow("no-letter") << QStringLiteral("zhangsan") << QStringLiteral("1234567890") << false;
    QTest::newRow("contains-username") << QStringLiteral("zhangsan") << QStringLiteral("ZhangSan99")
                                       << false;
    QTest::newRow("common") << QStringLiteral("zhangsan") << QStringLiteral("Password123") << false;
    QTest::newRow("trailing-space") << QStringLiteral("zhangsan") << QStringLiteral("Grind2026 ")
                                    << false;
    QTest::newRow("too-long") << QStringLiteral("zhangsan") << (QString(64, QLatin1Char('a')) + "1")
                              << false;
}

void AuthTest::passwordPolicy()
{
    QFETCH(QString, username);
    QFETCH(QString, password);
    QFETCH(bool, valid);
    QCOMPARE(PasswordPolicy::validatePassword(username, password).isEmpty(), valid);
}

void AuthTest::rolesIncludeLowerLevels()
{
    for (Permission permission : Auth::allPermissions()) {
        QVERIFY(!Auth::roleAllows(Role::None, permission));
        QVERIFY(Auth::roleAllows(Role::Administrator, permission));
        if (Auth::roleAllows(Role::Operator, permission)) {
            QVERIFY(Auth::roleAllows(Role::Technician, permission));
        }
    }
    QVERIFY(Auth::roleAllows(Role::Operator, Permission::AcknowledgeAlarm));
    QVERIFY(!Auth::roleAllows(Role::Operator, Permission::EditProcessParams));
    QVERIFY(Auth::roleAllows(Role::Technician, Permission::EditProcessParams));
    QVERIFY(Auth::roleAllows(Role::Technician, Permission::EditDisplaySettings));
    QVERIFY(!Auth::roleAllows(Role::Technician, Permission::EditCameraConfig));
    QVERIFY(!Auth::roleAllows(Role::Technician, Permission::ManageAccounts));
    for (Role role : Auth::assignableRoles()) {
        QCOMPARE(Auth::roleFromKey(Auth::roleKey(role)), role);
    }
    QCOMPARE(Auth::roleFromKey(QStringLiteral("root")), Role::None);
}

void AuthTest::init()
{
    m_connection = QStringLiteral("auth_test_%1").arg(QTest::currentTestFunction());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());
    m_now = QDateTime(QDate(2026, 10, 14), QTime(8, 0), Qt::UTC);
    m_auth = new AuthService();
    m_auth->setClockForTesting([this]() { return m_now; });
    QString error;
    QVERIFY2(m_auth->attachDatabase(m_connection, &error), qPrintable(error));
}

void AuthTest::cleanup()
{
    delete m_auth;
    m_auth = nullptr;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

void AuthTest::bootstrapAdmin()
{
    const AuthResult result =
        m_auth->createInitialAdministrator(QStringLiteral("admin01"), QString::fromLatin1(kAdminPassword));
    QVERIFY2(result.ok, qPrintable(result.message));
}

void AuthTest::advance(int seconds)
{
    m_now = m_now.addSecs(seconds);
}

void AuthTest::bootstrapCreatesOnlyOneAdministrator()
{
    QVERIFY(!m_auth->hasAccounts());
    QVERIFY(!m_auth->createInitialAdministrator(QStringLiteral("admin01"), QStringLiteral("weak")).ok);
    QVERIFY(!m_auth->hasAccounts());

    bootstrapAdmin();
    QVERIFY(m_auth->hasAccounts());
    QVERIFY(m_auth->isLoggedIn());
    QCOMPARE(m_auth->currentRole(), Role::Administrator);

    m_auth->logout();
    const AuthResult second =
        m_auth->createInitialAdministrator(QStringLiteral("intruder"), QStringLiteral("Intrude2026"));
    QVERIFY(!second.ok);
    QCOMPARE(m_auth->login(QStringLiteral("intruder"), QStringLiteral("Intrude2026")).status,
             LoginStatus::InvalidCredentials);
}

void AuthTest::loginAndLogout()
{
    bootstrapAdmin();
    m_auth->logout();
    QVERIFY(!m_auth->isLoggedIn());
    QVERIFY(!m_auth->can(Permission::EditDisplaySettings));

    QCOMPARE(m_auth->login(QStringLiteral("ADMIN01"), QString::fromLatin1(kAdminPassword)).status,
             LoginStatus::Success);
    QCOMPARE(m_auth->currentUsername(), QStringLiteral("admin01"));
    QVERIFY(m_auth->can(Permission::ManageAccounts));
    m_auth->logout();
    QVERIFY(!m_auth->can(Permission::ManageAccounts));
}

void AuthTest::wrongPasswordLocksAfterLimit()
{
    bootstrapAdmin();
    m_auth->logout();
    for (int i = 1; i < AuthService::kMaxFailedAttempts; ++i) {
        QCOMPARE(m_auth->login(QStringLiteral("admin01"), QStringLiteral("Wrong2026x")).status,
                 LoginStatus::InvalidCredentials);
    }
    QCOMPARE(m_auth->login(QStringLiteral("admin01"), QStringLiteral("Wrong2026x")).status,
             LoginStatus::Locked);
    // 锁定期内正确密码也不能登录。
    QCOMPARE(m_auth->login(QStringLiteral("admin01"), QString::fromLatin1(kAdminPassword)).status,
             LoginStatus::Locked);

    advance(AuthService::kLockoutMinutes * 60 + 1);
    QCOMPARE(m_auth->login(QStringLiteral("admin01"), QString::fromLatin1(kAdminPassword)).status,
             LoginStatus::Success);
}

void AuthTest::unknownUsernameIsThrottledLikeRealOne()
{
    bootstrapAdmin();
    m_auth->logout();
    const LoginResult first = m_auth->login(QStringLiteral("nobody"), QStringLiteral("Wrong2026x"));
    const LoginResult real = m_auth->login(QStringLiteral("admin01"), QStringLiteral("Wrong2026x"));
    QCOMPARE(first.status, LoginStatus::InvalidCredentials);
    QCOMPARE(first.message, real.message);
    for (int i = 2; i < AuthService::kMaxFailedAttempts; ++i) {
        m_auth->login(QStringLiteral("nobody"), QStringLiteral("Wrong2026x"));
    }
    QCOMPARE(m_auth->login(QStringLiteral("nobody"), QStringLiteral("Wrong2026x")).status,
             LoginStatus::Locked);
}

void AuthTest::newAccountMustChangePasswordFirst()
{
    bootstrapAdmin();
    const AuthResult created = m_auth->createUser(QStringLiteral("tech01"), Role::Technician,
                                                  QString::fromLatin1(kTechPassword));
    QVERIFY2(created.ok, qPrintable(created.message));
    QVERIFY(!m_auth->createUser(QStringLiteral("TECH01"), Role::Operator,
                                QStringLiteral("Other2026x")).ok);
    m_auth->logout();

    QCOMPARE(m_auth->login(QStringLiteral("tech01"), QString::fromLatin1(kTechPassword)).status,
             LoginStatus::PasswordChangeRequired);
    QVERIFY(!m_auth->isLoggedIn());

    QVERIFY(!m_auth->completeRequiredPasswordChange(QStringLiteral("tech01"),
                                                    QString::fromLatin1(kTechPassword),
                                                    QString::fromLatin1(kTechPassword)).ok);
    QVERIFY(!m_auth->completeRequiredPasswordChange(QStringLiteral("tech01"),
                                                    QStringLiteral("Guess2026x"),
                                                    QStringLiteral("Mine2026ab")).ok);
    const AuthResult changed = m_auth->completeRequiredPasswordChange(
        QStringLiteral("tech01"), QString::fromLatin1(kTechPassword), QStringLiteral("Mine2026ab"));
    QVERIFY2(changed.ok, qPrintable(changed.message));
    QVERIFY(m_auth->isLoggedIn());
    QCOMPARE(m_auth->currentRole(), Role::Technician);
    QVERIFY(m_auth->can(Permission::EditProcessParams));
    QVERIFY(!m_auth->can(Permission::EditCameraConfig));

    m_auth->logout();
    QCOMPARE(m_auth->login(QStringLiteral("tech01"), QString::fromLatin1(kTechPassword)).status,
             LoginStatus::InvalidCredentials);
    QCOMPARE(m_auth->login(QStringLiteral("tech01"), QStringLiteral("Mine2026ab")).status,
             LoginStatus::Success);
}

void AuthTest::onlyAdministratorManagesAccounts()
{
    bootstrapAdmin();
    QVERIFY(m_auth->createUser(QStringLiteral("tech01"), Role::Technician,
                               QString::fromLatin1(kTechPassword)).ok);
    m_auth->logout();
    m_auth->login(QStringLiteral("tech01"), QString::fromLatin1(kTechPassword));
    QVERIFY(m_auth->completeRequiredPasswordChange(QStringLiteral("tech01"),
                                                   QString::fromLatin1(kTechPassword),
                                                   QStringLiteral("Mine2026ab")).ok);

    QVERIFY(!m_auth->createUser(QStringLiteral("sneaky"), Role::Administrator,
                                QStringLiteral("Sneaky2026")).ok);
    QVERIFY(m_auth->listUsers().isEmpty());
    QVERIFY(m_auth->recentAudit(10).isEmpty());
    QVERIFY(!m_auth->setSessionTimeoutMinutes(60).ok);

    m_auth->logout();
    QVERIFY(!m_auth->createUser(QStringLiteral("sneaky"), Role::Operator,
                                QStringLiteral("Sneaky2026")).ok);
}

void AuthTest::onlyOneAdministratorExists()
{
    bootstrapAdmin();
    QVERIFY(!m_auth->createUser(QStringLiteral("admin02"), Role::Administrator,
                                QStringLiteral("Second2026")).ok);
    QVERIFY(m_auth->createUser(QStringLiteral("tech02"), Role::Technician,
                               QStringLiteral("Second2026")).ok);
    int adminId = 0;
    int techId = 0;
    for (const UserRecord &user : m_auth->listUsers()) {
        if (user.username == QLatin1String("admin01")) {
            adminId = user.id;
        } else if (user.username == QLatin1String("tech02")) {
            techId = user.id;
        }
    }
    QVERIFY(adminId > 0 && techId > 0);

    QVERIFY(!m_auth->setRole(techId, Role::Administrator).ok);
    QVERIFY(m_auth->setRole(techId, Role::Operator).ok);

    QVERIFY(!m_auth->setEnabled(adminId, false).ok);
    QVERIFY(!m_auth->setRole(adminId, Role::Operator).ok);
    QVERIFY(!m_auth->resetPassword(adminId, QStringLiteral("Reset2026x")).ok);

    int admins = 0;
    for (const UserRecord &user : m_auth->listUsers()) {
        admins += user.role == Role::Administrator ? 1 : 0;
    }
    QCOMPARE(admins, 1);
}

void AuthTest::registerNeedsAdministratorApproval()
{
    bootstrapAdmin();
    QVERIFY(m_auth->createUser(QStringLiteral("tech01"), Role::Technician,
                               QString::fromLatin1(kTechPassword)).ok);
    m_auth->logout();

    const QString admin = QStringLiteral("admin01");
    const QString adminPassword = QString::fromLatin1(kAdminPassword);
    QVERIFY(!m_auth->registerWithApproval(admin, QStringLiteral("Wrong2026x"), QStringLiteral("op01"),
                                          Role::Operator, QStringLiteral("Oper2026ab")).ok);
    QVERIFY(!m_auth->registerWithApproval(QStringLiteral("tech01"), QString::fromLatin1(kTechPassword),
                                          QStringLiteral("op01"), Role::Operator,
                                          QStringLiteral("Oper2026ab")).ok);
    QVERIFY(!m_auth->registerWithApproval(admin, adminPassword, QStringLiteral("boss"),
                                          Role::Administrator, QStringLiteral("Boss2026ab")).ok);
    QVERIFY(!m_auth->registerWithApproval(admin, adminPassword, QStringLiteral("op01"),
                                          Role::Operator, QStringLiteral("123456")).ok);
    QCOMPARE(m_auth->login(QStringLiteral("op01"), QStringLiteral("Oper2026ab")).status,
             LoginStatus::InvalidCredentials);

    const AuthResult registered = m_auth->registerWithApproval(
        admin, adminPassword, QStringLiteral("op01"), Role::Operator, QStringLiteral("Oper2026ab"));
    QVERIFY2(registered.ok, qPrintable(registered.message));
    QVERIFY(!m_auth->isLoggedIn());
    QVERIFY(!m_auth->registerWithApproval(admin, adminPassword, QStringLiteral("OP01"),
                                          Role::Operator, QStringLiteral("Oper2026cd")).ok);

    QCOMPARE(m_auth->login(QStringLiteral("op01"), QStringLiteral("Oper2026ab")).status,
             LoginStatus::Success);
    QCOMPARE(m_auth->currentRole(), Role::Operator);
    m_auth->logout();

    for (int i = 0; i < AuthService::kMaxFailedAttempts; ++i) {
        m_auth->registerWithApproval(admin, QStringLiteral("Wrong2026x"), QStringLiteral("op02"),
                                     Role::Operator, QStringLiteral("Oper2026ab"));
    }
    QCOMPARE(m_auth->login(admin, adminPassword).status, LoginStatus::Locked);
}

void AuthTest::disabledAccountCannotLogin()
{
    bootstrapAdmin();
    QVERIFY(m_auth->createUser(QStringLiteral("op01"), Role::Operator,
                               QStringLiteral("Oper2026ab")).ok);
    int id = 0;
    for (const UserRecord &user : m_auth->listUsers()) {
        if (user.username == QLatin1String("op01")) {
            id = user.id;
        }
    }
    QVERIFY(m_auth->setEnabled(id, false).ok);
    m_auth->logout();
    const LoginResult result = m_auth->login(QStringLiteral("op01"), QStringLiteral("Oper2026ab"));
    QCOMPARE(result.status, LoginStatus::InvalidCredentials);
    QVERIFY(!m_auth->isLoggedIn());
}

void AuthTest::idleSessionTimesOut()
{
    bootstrapAdmin();
    QVERIFY(m_auth->setSessionTimeoutMinutes(5).ok);
    QSignalSpy expired(m_auth, &AuthService::sessionExpired);

    advance(4 * 60);
    m_auth->checkIdleTimeout();
    QVERIFY(m_auth->isLoggedIn());

    m_auth->noteActivity();
    advance(4 * 60);
    m_auth->checkIdleTimeout();
    QVERIFY(m_auth->isLoggedIn());

    advance(60);
    m_auth->checkIdleTimeout();
    QVERIFY(!m_auth->isLoggedIn());
    QCOMPARE(expired.count(), 1);

    QVERIFY(!m_auth->setSessionTimeoutMinutes(0).ok);
}

void AuthTest::auditNeverContainsPasswords()
{
    bootstrapAdmin();
    QVERIFY(m_auth->createUser(QStringLiteral("tech01"), Role::Technician,
                               QString::fromLatin1(kTechPassword)).ok);
    m_auth->logout();
    m_auth->login(QStringLiteral("admin01"), QStringLiteral("Wrong2026x"));
    m_auth->login(QStringLiteral("admin01"), QString::fromLatin1(kAdminPassword));

    QSqlQuery query(QSqlDatabase::database(m_connection));
    QVERIFY(query.exec(QStringLiteral(
        "SELECT Username || Target || Detail || Action FROM AuditLog")));
    int rows = 0;
    while (query.next()) {
        ++rows;
        const QString text = query.value(0).toString();
        QVERIFY2(!text.contains(QString::fromLatin1(kAdminPassword)), qPrintable(text));
        QVERIFY2(!text.contains(QString::fromLatin1(kTechPassword)), qPrintable(text));
        QVERIFY2(!text.contains(QStringLiteral("Wrong2026x")), qPrintable(text));
    }
    QVERIFY(rows >= 4);

    QVERIFY(query.exec(QStringLiteral("SELECT PasswordHash, PasswordSalt FROM UserAccount")));
    while (query.next()) {
        QVERIFY(!query.value(0).toString().contains(QString::fromLatin1(kAdminPassword)));
        QCOMPARE(QByteArray::fromBase64(query.value(1).toString().toLatin1()).size(),
                 PasswordHasher::kSaltBytes);
    }
}

QTEST_GUILESS_MAIN(AuthTest)
#include "test_auth.moc"
