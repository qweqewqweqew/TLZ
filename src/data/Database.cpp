#include "data/Database.h"

#include "app/Logger.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

namespace {

constexpr const char *kConnName = "MzTLZ_Default";
constexpr const char *kDriverName = "QSQLITE";

bool configureSqliteSession(QSqlDatabase &db, QString *errorText)
{
    static const char *kStatements[] = {
        "PRAGMA foreign_keys = ON",
        "PRAGMA journal_mode = WAL",
        "PRAGMA synchronous = NORMAL",
    };

    for (const char *sql : kStatements) {
        QSqlQuery query(db);
        if (!query.exec(QString::fromLatin1(sql))) {
            if (errorText) {
                *errorText = query.lastError().text();
            }
            return false;
        }
    }
    return true;
}

} // namespace

Database &Database::instance()
{
    static Database s;
    return s;
}

const char *Database::connectionName()
{
    return kConnName;
}

bool Database::isOpen() const
{
    if (!QSqlDatabase::contains(kConnName)) {
        return false;
    }
    return QSqlDatabase::database(kConnName, /*open=*/false).isOpen();
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(kConnName, /*open=*/false);
}

bool Database::open(QString *errorMessage)
{
    if (isOpen()) {
        return true;
    }

    if (!QSqlDatabase::isDriverAvailable(kDriverName)) {
        const QString msg = QStringLiteral(
            "Qt SQL 驱动不可用: QSQLITE。请检查部署目录中的 sqldrivers/qsqlite.dll");
        LOG("[DB] %s", msg.toUtf8().constData());
        if (errorMessage) {
            *errorMessage = msg;
        }
        return false;
    }

    const QString dbPath = qEnvironmentVariable("MZTLZ_DB_PATH").trimmed();
    if (dbPath.isEmpty()) {
        const QString msg = QStringLiteral(
            "未设置 MZTLZ_DB_PATH，请先完成 DatabaseBootstrap::ensureDatabase");
        LOG("[DB] %s", msg.toUtf8().constData());
        if (errorMessage) {
            *errorMessage = msg;
        }
        return false;
    }

    QSqlDatabase db = QSqlDatabase::contains(kConnName)
                          ? QSqlDatabase::database(kConnName, /*open=*/false)
                          : QSqlDatabase::addDatabase(kDriverName, kConnName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        const QString msg = QStringLiteral("无法打开 SQLite 数据库 %1: %2")
                                .arg(dbPath, db.lastError().text());
        LOG("[DB] %s", msg.toUtf8().constData());
        if (errorMessage) {
            *errorMessage = msg;
        }
        return false;
    }

    QString sessionError;
    if (!configureSqliteSession(db, &sessionError)) {
        const QString msg = QStringLiteral("SQLite 会话初始化失败: %1").arg(sessionError);
        LOG("[DB] %s", msg.toUtf8().constData());
        db.close();
        if (errorMessage) {
            *errorMessage = msg;
        }
        return false;
    }

    LOG("[DB] 连接成功: sqlite=%s", dbPath.toUtf8().constData());
    return true;
}

bool Database::healthCheck(QString *errorMessage) const
{
    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("数据库未连接");
        }
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT 1 AS HealthValue")) ||
        !query.next() || query.value(0).toInt() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("数据库健康检查失败: %1")
                                .arg(query.lastError().text());
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void Database::close()
{
    if (QSqlDatabase::contains(kConnName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(kConnName, /*open=*/false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(kConnName);
        LOG("[DB] 连接已关闭");
    }
}
