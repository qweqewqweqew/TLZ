#include "data/DatabaseBootstrap.h"

#include "app/Logger.h"
#include "app/RuntimeIniPaths.h"
#include "auth/AccountSchema.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace {

constexpr const char *kBootstrapConnectionName = "MzTLZ_Bootstrap";

struct DatabaseSettings
{
    QString path;
};

DatabaseSettings loadSettings()
{
    const QString configPath = databaseIniPath();
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    DatabaseSettings result;
    result.path = settings.value(QStringLiteral("Database/Path"),
                                 QStringLiteral("data/mztlz.db"))
                      .toString()
                      .trimmed();
    if (result.path.isEmpty()) {
        result.path = QStringLiteral("data/mztlz.db");
    }
    return result;
}

QString resolveDatabasePath(const QString &configuredPath)
{
    const QFileInfo info(configuredPath);
    if (info.isAbsolute()) {
        return QDir::cleanPath(configuredPath);
    }
    return QDir::cleanPath(
        QDir(QCoreApplication::applicationDirPath()).filePath(configuredPath));
}

bool ensureParentDirectory(const QString &dbPath, QString *errorMessage)
{
    const QFileInfo info(dbPath);
    QDir dir = info.dir();
    if (dir.exists()) {
        return true;
    }
    if (!dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建数据库目录: %1").arg(dir.absolutePath());
        }
        return false;
    }
    return true;
}

bool execSql(QSqlDatabase &db, const char *sql, QString *errorMessage)
{
    QSqlQuery query(db);
    if (!query.exec(QString::fromUtf8(sql))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool ensureBaseSchema(QSqlDatabase &db, QString *errorMessage)
{
    // 表语义与早期设计一致；本分支使用 SQLite 类型落地。
    static const char *kStatements[] = {
        "PRAGMA foreign_keys = ON",

        "CREATE TABLE IF NOT EXISTS InspectionRecord ("
        " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " BackendTaskId INTEGER NULL,"
        " RecipeName TEXT NOT NULL DEFAULT '',"
        " ProcessStartAt TEXT NULL,"
        " ProcessEndAt TEXT NULL,"
        " InspectStartAt TEXT NULL,"
        " InspectEndAt TEXT NULL,"
        " ParticleCount INTEGER NOT NULL DEFAULT 0,"
        " ClearedCount INTEGER NOT NULL DEFAULT 0,"
        " MaxParticleHeight REAL NOT NULL DEFAULT 0,"
        " OverviewImagePath TEXT NULL,"
        " Remark TEXT NULL,"
        " PlateNo TEXT NOT NULL DEFAULT '',"
        " Shift TEXT NOT NULL DEFAULT '',"
        " ProcessOutcome INTEGER NOT NULL DEFAULT 0,"
        " ManualReview INTEGER NOT NULL DEFAULT 0,"
        " CreatedAt TEXT NOT NULL DEFAULT (datetime('now')),"
        " UpdatedAt TEXT NOT NULL DEFAULT (datetime('now'))"
        ")",

        "CREATE TABLE IF NOT EXISTS ParticleDetection ("
        " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " InspectionRecordId INTEGER NOT NULL,"
        " ParticleIndex INTEGER NOT NULL,"
        " PositionX REAL NOT NULL DEFAULT 0,"
        " PositionY REAL NOT NULL DEFAULT 0,"
        " PositionZ REAL NOT NULL DEFAULT 0,"
        " Height REAL NOT NULL DEFAULT 0,"
        " IsCleared INTEGER NOT NULL DEFAULT 0,"
        " DetectedAt TEXT NOT NULL DEFAULT (datetime('now')),"
        " ClearedAt TEXT NULL,"
        " FOREIGN KEY (InspectionRecordId) REFERENCES InspectionRecord(Id) ON DELETE CASCADE,"
        " UNIQUE (InspectionRecordId, ParticleIndex)"
        ")",

        "CREATE TABLE IF NOT EXISTS ProcessParameter ("
        " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " RecipeName TEXT NOT NULL,"
        " PathIndex INTEGER NOT NULL,"
        " MaxParticleHeight REAL NOT NULL DEFAULT 0,"
        " TargetX REAL NOT NULL DEFAULT 0,"
        " TargetY REAL NOT NULL DEFAULT 0,"
        " TargetZ REAL NOT NULL DEFAULT 0,"
        " FeedSpeed REAL NOT NULL DEFAULT 0,"
        " FeedAmount REAL NOT NULL DEFAULT 0,"
        " SpindleSpeed REAL NOT NULL DEFAULT 0,"
        " CutCount INTEGER NOT NULL DEFAULT 0,"
        " SingleCutAmount REAL NOT NULL DEFAULT 0,"
        " CreatedAt TEXT NOT NULL DEFAULT (datetime('now')),"
        " UpdatedAt TEXT NOT NULL DEFAULT (datetime('now')),"
        " UNIQUE (RecipeName, PathIndex)"
        ")",

        "CREATE TABLE IF NOT EXISTS ProcessParameterHistory ("
        " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " InspectionRecordId INTEGER NOT NULL,"
        " RecipeName TEXT NOT NULL,"
        " PathIndex INTEGER NOT NULL,"
        " MaxParticleHeight REAL NOT NULL DEFAULT 0,"
        " TargetX REAL NOT NULL DEFAULT 0,"
        " TargetY REAL NOT NULL DEFAULT 0,"
        " TargetZ REAL NOT NULL DEFAULT 0,"
        " FeedSpeed REAL NOT NULL DEFAULT 0,"
        " FeedAmount REAL NOT NULL DEFAULT 0,"
        " SpindleSpeed REAL NOT NULL DEFAULT 0,"
        " CutCount INTEGER NOT NULL DEFAULT 0,"
        " SingleCutAmount REAL NOT NULL DEFAULT 0,"
        " SnapshotAt TEXT NOT NULL DEFAULT (datetime('now')),"
        " FOREIGN KEY (InspectionRecordId) REFERENCES InspectionRecord(Id) ON DELETE CASCADE,"
        " UNIQUE (InspectionRecordId, PathIndex)"
        ")",

        "CREATE TABLE IF NOT EXISTS InspectionFrame ("
        " Id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " InspectionRecordId INTEGER NOT NULL,"
        " BackendTaskId INTEGER NOT NULL,"
        " FrameId INTEGER NOT NULL,"
        " ScanOrdinal INTEGER NOT NULL DEFAULT 0,"
        " TimestampNs INTEGER NOT NULL DEFAULT 0,"
        " Width INTEGER NOT NULL DEFAULT 0,"
        " Height INTEGER NOT NULL DEFAULT 0,"
        " PixelFormat INTEGER NOT NULL DEFAULT 0,"
        " RangePixelFormat INTEGER NOT NULL DEFAULT -1,"
        " IntensityPixelFormat INTEGER NOT NULL DEFAULT -1,"
        " HasRange INTEGER NOT NULL DEFAULT 0,"
        " HasIntensity INTEGER NOT NULL DEFAULT 0,"
        " CreatedAt TEXT NOT NULL DEFAULT (datetime('now')),"
        " FOREIGN KEY (InspectionRecordId) REFERENCES InspectionRecord(Id) ON DELETE CASCADE,"
        " UNIQUE (BackendTaskId, FrameId)"
        ")",

        "CREATE UNIQUE INDEX IF NOT EXISTS UX_InspectionRecord_BackendTaskId "
        "ON InspectionRecord(BackendTaskId) WHERE BackendTaskId IS NOT NULL",

        "CREATE INDEX IF NOT EXISTS IX_InspectionFrame_RecordId "
        "ON InspectionFrame(InspectionRecordId)",
    };

    for (const char *statement : kStatements) {
        QString error;
        if (!execSql(db, statement, &error)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("初始化基础表失败: %1").arg(error);
            }
            return false;
        }
    }
    return AccountSchema::ensure(db, errorMessage);
}

} // namespace

bool DatabaseBootstrap::ensureDatabase(QString *errorMessage)
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        const QString message = QStringLiteral(
            "Qt QSQLITE 插件不可用，请检查部署目录中的 sqldrivers/qsqlite.dll");
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    const DatabaseSettings settings = loadSettings();
    const QString dbPath = resolveDatabasePath(settings.path);
    if (!ensureParentDirectory(dbPath, errorMessage)) {
        return false;
    }

    {
        QSqlDatabase db = QSqlDatabase::contains(kBootstrapConnectionName)
                              ? QSqlDatabase::database(kBootstrapConnectionName, false)
                              : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          kBootstrapConnectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            const QString message = QStringLiteral("无法打开/创建 SQLite 数据库 %1: %2")
                                        .arg(dbPath, db.lastError().text());
            LOG("[DB] %s", message.toUtf8().constData());
            if (errorMessage) {
                *errorMessage = message;
            }
            QSqlDatabase::removeDatabase(kBootstrapConnectionName);
            return false;
        }

        QString schemaError;
        const bool ok = ensureBaseSchema(db, &schemaError);
        db.close();
        QSqlDatabase::removeDatabase(kBootstrapConnectionName);

        if (!ok) {
            LOG("[DB] %s", schemaError.toUtf8().constData());
            if (errorMessage) {
                *errorMessage = schemaError;
            }
            return false;
        }
    }

    qputenv("MZTLZ_DB_PATH", dbPath.toUtf8());
    LOG("[DB] 数据库已就绪: sqlite=%s", dbPath.toUtf8().constData());
    return true;
}
