#include "Database.h"

#include "Logger.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

namespace {
constexpr const char *kConnName = "MzTLZ_Default";
constexpr const char *kDriverName = "QODBC";

// SQL Server ODBC 驱动候选（优先使用最新的）
const QStringList &odbcDriverCandidates()
{
    static const QStringList kList = {
        "ODBC Driver 18 for SQL Server",
        "ODBC Driver 17 for SQL Server",
        "SQL Server Native Client 11.0",
        "SQL Server",
    };
    return kList;
}

QString buildConnectionString(const QString &driver)
{
    const QString server = qEnvironmentVariable(
        "MZTLZ_DB_SERVER", QStringLiteral("localhost\\SQLEXPRESS"));
    const QString databaseName = qEnvironmentVariable(
        "MZTLZ_DB_NAME", QStringLiteral("MzTLZ"));
    // 用 Windows 集成认证；实例和数据库由启动引导模块确定
    // Encrypt/TrustServerCertificate 是给 Driver 18 用的，其他驱动会忽略
    return QString(
               "DRIVER={%1};"
               "SERVER=%2;"
               "DATABASE=%3;"
               "Trusted_Connection=Yes;"
               "Encrypt=No;"
               "TrustServerCertificate=Yes;")
        .arg(driver, server, databaseName);
}

bool configureSqlServerSession(QSqlDatabase &db, QString *errorText)
{
    static const char *kStatements[] = {
        "SET QUOTED_IDENTIFIER ON",
        "SET ANSI_NULLS ON",
        "SET ANSI_PADDING ON",
        "SET ANSI_WARNINGS ON",
        "SET CONCAT_NULL_YIELDS_NULL ON",
        "SET ARITHABORT ON",
        "SET NUMERIC_ROUNDABORT OFF",
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
                                "Qt SQL 驱动不可用: %1。请确保 Qt 编译时启用了 QODBC 插件。")
                                .arg(kDriverName);
        LOG("[DB] %s", msg.toUtf8().constData());
        if (errorMessage) *errorMessage = msg;
        return false;
    }

    QSqlDatabase db = QSqlDatabase::contains(kConnName)
                          ? QSqlDatabase::database(kConnName, /*open=*/false)
                          : QSqlDatabase::addDatabase(kDriverName, kConnName);

    QStringList triedDrivers;
    QString lastError;
    for (const QString &drv : odbcDriverCandidates()) {
        triedDrivers << drv;
        db.setDatabaseName(buildConnectionString(drv));
        if (db.open()) {
            QString sessionError;
            if (!configureSqlServerSession(db, &sessionError)) {
                lastError = QStringLiteral("SQL Server session setup failed: %1").arg(sessionError);
                LOG("[DB] session setup failed for %s: %s",
                    drv.toUtf8().constData(),
                    sessionError.toUtf8().constData());
                db.close();
                continue;
            }
            LOG("[DB] 连接成功: driver=%s", drv.toUtf8().constData());
            return true;
        }
        lastError = db.lastError().text();
        LOG("[DB] 尝试 %s 失败: %s",
            drv.toUtf8().constData(),
            lastError.toUtf8().constData());
    }

    const QString msg = QStringLiteral(
                            "无法连接到 SQL Server (MzTLZ)。已尝试驱动: %1。最后错误: %2")
                            .arg(triedDrivers.join(", "), lastError);
    LOG("[DB] %s", msg.toUtf8().constData());
    if (errorMessage) *errorMessage = msg;
    return false;
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
