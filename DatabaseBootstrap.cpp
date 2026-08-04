#include "DatabaseBootstrap.h"

#include "Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

constexpr const char *kBootstrapConnectionName = "MzTLZ_Bootstrap";

struct DatabaseSettings
{
    QString server;
    QString databaseName;
};

DatabaseSettings loadSettings()
{
    const QString configPath = QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("database.ini"));
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    DatabaseSettings result;
    result.server = settings.value(QStringLiteral("Database/Server"),
                                   QStringLiteral("localhost\\SQLEXPRESS"))
                        .toString()
                        .trimmed();
    result.databaseName = settings.value(QStringLiteral("Database/Name"),
                                         QStringLiteral("MzTLZ"))
                              .toString()
                              .trimmed();
    return result;
}

void appendUnique(QStringList &values, const QString &value)
{
    if (value.isEmpty()) {
        return;
    }
    for (const QString &existing : values) {
        if (existing.compare(value, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    values.append(value);
}

QStringList serverCandidates(const QString &configuredServer)
{
    QStringList result;
    appendUnique(result, configuredServer);
    appendUnique(result, QStringLiteral("localhost\\SQLEXPRESS"));
    appendUnique(result, QStringLiteral("localhost"));
    return result;
}

const QStringList &odbcDriverCandidates()
{
    static const QStringList kDrivers = {
        QStringLiteral("ODBC Driver 18 for SQL Server"),
        QStringLiteral("ODBC Driver 17 for SQL Server"),
        QStringLiteral("SQL Server Native Client 11.0"),
        QStringLiteral("SQL Server"),
    };
    return kDrivers;
}

QString connectionString(const QString &driver,
                         const QString &server,
                         const QString &databaseName)
{
    return QStringLiteral(
               "DRIVER={%1};"
               "SERVER=%2;"
               "DATABASE=%3;"
               "Trusted_Connection=Yes;"
               "Encrypt=No;"
               "TrustServerCertificate=Yes;")
        .arg(driver, server, databaseName);
}

bool ensureDatabaseExists(QSqlDatabase &db,
                          const QString &databaseName,
                          QString *errorMessage)
{
    QSqlQuery check(db);
    check.prepare(QStringLiteral("SELECT DB_ID(?)"));
    check.addBindValue(databaseName);
    if (!check.exec() || !check.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("检查数据库失败: %1")
                                .arg(check.lastError().text());
        }
        return false;
    }
    if (!check.value(0).isNull()) {
        return true;
    }

    QSqlQuery create(db);
    if (!create.exec(QStringLiteral("CREATE DATABASE [%1]").arg(databaseName))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("创建数据库 %1 失败: %2")
                                .arg(databaseName, create.lastError().text());
        }
        return false;
    }
    LOG("[DB] 已创建数据库: %s", databaseName.toUtf8().constData());
    return true;
}

bool ensureBaseSchema(QSqlDatabase &db, QString *errorMessage)
{
    static const char *kStatements[] = {
        "IF OBJECT_ID('dbo.InspectionRecord', 'U') IS NULL "
        "CREATE TABLE dbo.InspectionRecord ("
        " Id INT IDENTITY(1,1) NOT NULL CONSTRAINT PK_InspectionRecord PRIMARY KEY,"
        " BackendTaskId BIGINT NULL,"
        " RecipeName NVARCHAR(128) NOT NULL CONSTRAINT DF_InspectionRecord_Recipe DEFAULT N'',"
        " ProcessStartAt DATETIME2 NULL,"
        " ProcessEndAt DATETIME2 NULL,"
        " InspectStartAt DATETIME2 NULL,"
        " InspectEndAt DATETIME2 NULL,"
        " ParticleCount INT NOT NULL CONSTRAINT DF_InspectionRecord_ParticleCount DEFAULT 0,"
        " ClearedCount INT NOT NULL CONSTRAINT DF_InspectionRecord_ClearedCount DEFAULT 0,"
        " MaxParticleHeight FLOAT NOT NULL CONSTRAINT DF_InspectionRecord_MaxHeight DEFAULT 0,"
        " OverviewImagePath NVARCHAR(1024) NULL,"
        " Remark NVARCHAR(1024) NULL,"
        " CreatedAt DATETIME2 NOT NULL CONSTRAINT DF_InspectionRecord_CreatedAt DEFAULT SYSUTCDATETIME(),"
        " UpdatedAt DATETIME2 NOT NULL CONSTRAINT DF_InspectionRecord_UpdatedAt DEFAULT SYSUTCDATETIME()"
        ")",

        "IF OBJECT_ID('dbo.ParticleDetection', 'U') IS NULL "
        "CREATE TABLE dbo.ParticleDetection ("
        " Id BIGINT IDENTITY(1,1) NOT NULL CONSTRAINT PK_ParticleDetection PRIMARY KEY,"
        " InspectionRecordId INT NOT NULL,"
        " ParticleIndex INT NOT NULL,"
        " PositionX FLOAT NOT NULL CONSTRAINT DF_ParticleDetection_X DEFAULT 0,"
        " PositionY FLOAT NOT NULL CONSTRAINT DF_ParticleDetection_Y DEFAULT 0,"
        " PositionZ FLOAT NOT NULL CONSTRAINT DF_ParticleDetection_Z DEFAULT 0,"
        " Height FLOAT NOT NULL CONSTRAINT DF_ParticleDetection_Height DEFAULT 0,"
        " IsCleared BIT NOT NULL CONSTRAINT DF_ParticleDetection_Cleared DEFAULT 0,"
        " DetectedAt DATETIME2 NOT NULL CONSTRAINT DF_ParticleDetection_DetectedAt DEFAULT SYSUTCDATETIME(),"
        " ClearedAt DATETIME2 NULL,"
        " CONSTRAINT FK_ParticleDetection_Record FOREIGN KEY (InspectionRecordId)"
        "  REFERENCES dbo.InspectionRecord(Id) ON DELETE CASCADE,"
        " CONSTRAINT UQ_ParticleDetection_Record_Index UNIQUE (InspectionRecordId, ParticleIndex)"
        ")",

        "IF OBJECT_ID('dbo.ProcessParameter', 'U') IS NULL "
        "CREATE TABLE dbo.ProcessParameter ("
        " Id INT IDENTITY(1,1) NOT NULL CONSTRAINT PK_ProcessParameter PRIMARY KEY,"
        " RecipeName NVARCHAR(128) NOT NULL,"
        " PathIndex INT NOT NULL,"
        " MaxParticleHeight FLOAT NOT NULL CONSTRAINT DF_ProcessParameter_MaxHeight DEFAULT 0,"
        " TargetX FLOAT NOT NULL CONSTRAINT DF_ProcessParameter_X DEFAULT 0,"
        " TargetY FLOAT NOT NULL CONSTRAINT DF_ProcessParameter_Y DEFAULT 0,"
        " TargetZ FLOAT NOT NULL CONSTRAINT DF_ProcessParameter_Z DEFAULT 0,"
        " FeedSpeed FLOAT NOT NULL CONSTRAINT DF_ProcessParameter_FeedSpeed DEFAULT 0,"
        " FeedAmount FLOAT NOT NULL CONSTRAINT DF_ProcessParameter_FeedAmount DEFAULT 0,"
        " SpindleSpeed FLOAT NOT NULL CONSTRAINT DF_ProcessParameter_SpindleSpeed DEFAULT 0,"
        " CutCount INT NOT NULL CONSTRAINT DF_ProcessParameter_CutCount DEFAULT 0,"
        " SingleCutAmount FLOAT NOT NULL CONSTRAINT DF_ProcessParameter_SingleCut DEFAULT 0,"
        " CreatedAt DATETIME2 NOT NULL CONSTRAINT DF_ProcessParameter_CreatedAt DEFAULT SYSUTCDATETIME(),"
        " UpdatedAt DATETIME2 NOT NULL CONSTRAINT DF_ProcessParameter_UpdatedAt DEFAULT SYSUTCDATETIME(),"
        " CONSTRAINT UQ_ProcessParameter_Recipe_Path UNIQUE (RecipeName, PathIndex)"
        ")",

        "IF OBJECT_ID('dbo.ProcessParameterHistory', 'U') IS NULL "
        "CREATE TABLE dbo.ProcessParameterHistory ("
        " Id BIGINT IDENTITY(1,1) NOT NULL CONSTRAINT PK_ProcessParameterHistory PRIMARY KEY,"
        " InspectionRecordId INT NOT NULL,"
        " RecipeName NVARCHAR(128) NOT NULL,"
        " PathIndex INT NOT NULL,"
        " MaxParticleHeight FLOAT NOT NULL CONSTRAINT DF_ProcessHistory_MaxHeight DEFAULT 0,"
        " TargetX FLOAT NOT NULL CONSTRAINT DF_ProcessHistory_X DEFAULT 0,"
        " TargetY FLOAT NOT NULL CONSTRAINT DF_ProcessHistory_Y DEFAULT 0,"
        " TargetZ FLOAT NOT NULL CONSTRAINT DF_ProcessHistory_Z DEFAULT 0,"
        " FeedSpeed FLOAT NOT NULL CONSTRAINT DF_ProcessHistory_FeedSpeed DEFAULT 0,"
        " FeedAmount FLOAT NOT NULL CONSTRAINT DF_ProcessHistory_FeedAmount DEFAULT 0,"
        " SpindleSpeed FLOAT NOT NULL CONSTRAINT DF_ProcessHistory_SpindleSpeed DEFAULT 0,"
        " CutCount INT NOT NULL CONSTRAINT DF_ProcessHistory_CutCount DEFAULT 0,"
        " SingleCutAmount FLOAT NOT NULL CONSTRAINT DF_ProcessHistory_SingleCut DEFAULT 0,"
        " SnapshotAt DATETIME2 NOT NULL CONSTRAINT DF_ProcessHistory_SnapshotAt DEFAULT SYSUTCDATETIME(),"
        " CONSTRAINT FK_ProcessHistory_Record FOREIGN KEY (InspectionRecordId)"
        "  REFERENCES dbo.InspectionRecord(Id) ON DELETE CASCADE,"
        " CONSTRAINT UQ_ProcessHistory_Record_Path UNIQUE (InspectionRecordId, PathIndex)"
        ")",
    };

    for (const char *statement : kStatements) {
        QSqlQuery query(db);
        if (!query.exec(QString::fromLatin1(statement))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("初始化基础表失败: %1")
                                    .arg(query.lastError().text());
            }
            return false;
        }
    }
    return true;
}

} // namespace

bool DatabaseBootstrap::ensureDatabase(QString *errorMessage)
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QODBC"))) {
        const QString message = QStringLiteral(
            "Qt QODBC 插件不可用，请检查部署目录中的 sqldrivers/qsqlodbc.dll");
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    const DatabaseSettings settings = loadSettings();
    static const QRegularExpression kSafeDatabaseName(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,127}$"));
    if (!kSafeDatabaseName.match(settings.databaseName).hasMatch()) {
        const QString message = QStringLiteral(
            "database.ini 中的数据库名无效，只允许字母、数字和下划线: %1")
                                    .arg(settings.databaseName);
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    QString selectedServer;
    QString selectedDriver;
    QString lastError;
    QStringList attemptedConnections;
    bool initialized = false;

    {
        QSqlDatabase db = QSqlDatabase::contains(kBootstrapConnectionName)
                              ? QSqlDatabase::database(kBootstrapConnectionName, false)
                              : QSqlDatabase::addDatabase(QStringLiteral("QODBC"),
                                                          kBootstrapConnectionName);
        db.setConnectOptions(QStringLiteral("SQL_ATTR_LOGIN_TIMEOUT=3"));

        for (const QString &driver : odbcDriverCandidates()) {
            for (const QString &server : serverCandidates(settings.server)) {
                attemptedConnections << QStringLiteral("%1 @ %2").arg(driver, server);
                db.close();
                db.setDatabaseName(connectionString(driver, server, QStringLiteral("master")));
                if (!db.open()) {
                    lastError = db.lastError().text();
                    continue;
                }

                QString createError;
                if (!ensureDatabaseExists(db, settings.databaseName, &createError)) {
                    lastError = createError;
                    continue;
                }

                db.close();
                db.setDatabaseName(connectionString(driver, server, settings.databaseName));
                if (!db.open()) {
                    lastError = db.lastError().text();
                    continue;
                }

                QString schemaError;
                if (!ensureBaseSchema(db, &schemaError)) {
                    lastError = schemaError;
                    continue;
                }

                selectedServer = server;
                selectedDriver = driver;
                initialized = true;
                break;
            }
            if (initialized) {
                break;
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(kBootstrapConnectionName);

    if (!initialized) {
        const QString message = QStringLiteral(
            "无法初始化 SQL Server 数据库 %1。已尝试: %2。最后错误: %3")
                                    .arg(settings.databaseName,
                                         attemptedConnections.join(QStringLiteral(", ")),
                                         lastError);
        LOG("[DB] %s", message.toUtf8().constData());
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    qputenv("MZTLZ_DB_SERVER", selectedServer.toUtf8());
    qputenv("MZTLZ_DB_NAME", settings.databaseName.toUtf8());
    LOG("[DB] 数据库已就绪: server=%s database=%s driver=%s",
        selectedServer.toUtf8().constData(),
        settings.databaseName.toUtf8().constData(),
        selectedDriver.toUtf8().constData());
    return true;
}
