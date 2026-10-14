#include "auth/AccountSchema.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace AccountSchema {

bool ensure(QSqlDatabase &db, QString *errorMessage)
{
    static const char *kStatements[] = {
        "CREATE TABLE IF NOT EXISTS UserAccount ("
        " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " Username TEXT NOT NULL COLLATE NOCASE UNIQUE,"
        " Role TEXT NOT NULL CHECK (Role IN ('operator','technician','administrator')),"
        " PasswordHash TEXT NOT NULL,"
        " PasswordSalt TEXT NOT NULL,"
        " PasswordIterations INTEGER NOT NULL,"
        " Enabled INTEGER NOT NULL DEFAULT 1,"
        " MustChangePassword INTEGER NOT NULL DEFAULT 0,"
        " FailedLoginCount INTEGER NOT NULL DEFAULT 0,"
        " LockedUntil TEXT NULL,"
        " LastLoginAt TEXT NULL,"
        " PasswordChangedAt TEXT NULL,"
        " CreatedAt TEXT NOT NULL,"
        " CreatedBy TEXT NOT NULL DEFAULT '',"
        " UpdatedAt TEXT NOT NULL"
        ")",

        "CREATE TABLE IF NOT EXISTS UserSession ("
        " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " UserId INTEGER NOT NULL,"
        " StartedAt TEXT NOT NULL,"
        " LastActivityAt TEXT NOT NULL,"
        " EndedAt TEXT NULL,"
        " EndReason TEXT NULL,"
        " FOREIGN KEY (UserId) REFERENCES UserAccount(Id) ON DELETE CASCADE"
        ")",

        "CREATE INDEX IF NOT EXISTS IX_UserSession_UserId ON UserSession(UserId)",

        // 审计只追加，界面不提供删除或修改入口。
        "CREATE TABLE IF NOT EXISTS AuditLog ("
        " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " OccurredAt TEXT NOT NULL,"
        " Username TEXT NOT NULL DEFAULT '',"
        " Role TEXT NOT NULL DEFAULT '',"
        " Action TEXT NOT NULL,"
        " Target TEXT NOT NULL DEFAULT '',"
        " Detail TEXT NOT NULL DEFAULT '',"
        " Success INTEGER NOT NULL DEFAULT 1"
        ")",

        "CREATE INDEX IF NOT EXISTS IX_AuditLog_OccurredAt ON AuditLog(OccurredAt)",

        "CREATE TABLE IF NOT EXISTS AppSetting ("
        " Key TEXT PRIMARY KEY,"
        " Value TEXT NOT NULL,"
        " UpdatedAt TEXT NOT NULL"
        ")",
    };

    for (const char *statement : kStatements) {
        QSqlQuery query(db);
        if (!query.exec(QString::fromUtf8(statement))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("初始化账户表失败: %1").arg(query.lastError().text());
            }
            return false;
        }
    }
    return true;
}

} // namespace AccountSchema
