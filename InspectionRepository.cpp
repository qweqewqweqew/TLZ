#include "InspectionRepository.h"

#include "Database.h"

#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <limits>

namespace {

QString takeError(const QSqlQuery &query)
{
    return query.lastError().text();
}

void reportError(QString *errorMessage, const QString &text)
{
    if (errorMessage) {
        *errorMessage = text;
    }
}

bool toSqlBigIntVariant(quint64 value,
                        const QString &fieldName,
                        QVariant *out,
                        QString *errorMessage)
{
    constexpr quint64 kSqlBigIntMax =
        static_cast<quint64>(std::numeric_limits<qlonglong>::max());
    if (value > kSqlBigIntMax) {
        reportError(errorMessage,
                    QStringLiteral("%1 超出 SQL Server BIGINT 范围: %2")
                        .arg(fieldName)
                        .arg(value));
        return false;
    }
    if (out) {
        *out = QVariant::fromValue<qlonglong>(static_cast<qlonglong>(value));
    }
    return true;
}

// 从 ProcessParameter 表把当前 recipe 的所有路径读出来，
// 作为一次任务的"工艺快照"写入 ProcessParameterHistory。
// 目前仅供真实流程使用；模拟数据不写这张表，避免和唯一约束冲突。
[[maybe_unused]] bool snapshotProcessParameters(int inspectionRecordId,
                                                const QString &recipeName,
                                                QSqlDatabase &db,
                                                QString *errorMessage)
{
    QSqlQuery pick(db);
    pick.prepare("SELECT RecipeName, PathIndex, MaxParticleHeight, "
                 "       TargetX, TargetY, TargetZ, "
                 "       FeedSpeed, FeedAmount, SpindleSpeed, "
                 "       CutCount, SingleCutAmount "
                 "FROM dbo.ProcessParameter "
                 "WHERE RecipeName = ? "
                 "ORDER BY PathIndex");
    pick.addBindValue(recipeName);
    if (!pick.exec()) {
        reportError(errorMessage,
                    QStringLiteral("查询 ProcessParameter 失败: %1").arg(takeError(pick)));
        return false;
    }

    while (pick.next()) {
        QSqlQuery insert(db);
        insert.prepare(
            "INSERT INTO dbo.ProcessParameterHistory "
            "(InspectionRecordId, RecipeName, PathIndex, MaxParticleHeight, "
            " TargetX, TargetY, TargetZ, FeedSpeed, FeedAmount, SpindleSpeed, "
            " CutCount, SingleCutAmount, SnapshotAt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, SYSUTCDATETIME())");
        insert.addBindValue(inspectionRecordId);
        insert.addBindValue(pick.value("RecipeName"));
        insert.addBindValue(pick.value("PathIndex"));
        insert.addBindValue(pick.value("MaxParticleHeight"));
        insert.addBindValue(pick.value("TargetX"));
        insert.addBindValue(pick.value("TargetY"));
        insert.addBindValue(pick.value("TargetZ"));
        insert.addBindValue(pick.value("FeedSpeed"));
        insert.addBindValue(pick.value("FeedAmount"));
        insert.addBindValue(pick.value("SpindleSpeed"));
        insert.addBindValue(pick.value("CutCount"));
        insert.addBindValue(pick.value("SingleCutAmount"));

        if (!insert.exec()) {
            reportError(errorMessage,
                        QStringLiteral("写入 ProcessParameterHistory 失败: %1")
                            .arg(takeError(insert)));
            return false;
        }
    }
    return true;
}

// 为一条 InspectionRecord 随机生成若干颗粒明细。
bool insertMockParticles(int inspectionRecordId,
                         int particleCount,
                         int clearedCount,
                         const QDateTime &detectAnchor,
                         QSqlDatabase &db,
                         QString *errorMessage)
{
    auto *rng = QRandomGenerator::global();
    for (int i = 1; i <= particleCount; ++i) {
        const double x = rng->generateDouble() * 200.0;
        const double y = rng->generateDouble() * 100.0;
        const double z = 10.0 + rng->generateDouble() * 2.0;
        const double height = 0.05 + rng->generateDouble() * 0.6;
        const bool cleared = i <= clearedCount;
        const QDateTime detectedAt = detectAnchor.addSecs(i);
        const QDateTime clearedAt = cleared ? detectedAt.addSecs(2) : QDateTime();

        QSqlQuery insert(db);
        insert.prepare(
            "INSERT INTO dbo.ParticleDetection "
            "(InspectionRecordId, ParticleIndex, PositionX, PositionY, PositionZ, "
            " Height, IsCleared, ClearedAt, DetectedAt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        insert.addBindValue(inspectionRecordId);
        insert.addBindValue(i);
        insert.addBindValue(x);
        insert.addBindValue(y);
        insert.addBindValue(z);
        insert.addBindValue(height);
        insert.addBindValue(cleared ? 1 : 0);
        if (cleared) {
            insert.addBindValue(clearedAt);
        } else {
            insert.addBindValue(QVariant(QVariant::DateTime));
        }
        insert.addBindValue(detectedAt);

        if (!insert.exec()) {
            reportError(errorMessage,
                        QStringLiteral("写入 ParticleDetection 失败: %1")
                            .arg(takeError(insert)));
            return false;
        }
    }
    return true;
}

QString buildRecentSql(const InspectionQuery &q, bool &hasRecipe, bool &hasFrom, bool &hasTo)
{
    QString sql =
        "SELECT TOP (?) Id, BackendTaskId, RecipeName, ProcessStartAt, ProcessEndAt, "
        "       InspectStartAt, InspectEndAt, ParticleCount, ClearedCount, "
        "       MaxParticleHeight, OverviewImagePath, Remark, CreatedAt "
        "FROM dbo.InspectionRecord ";

    QStringList clauses;
    hasRecipe = !q.recipe.isEmpty();
    hasFrom   = q.createdFrom.isValid();
    hasTo     = q.createdTo.isValid();

    if (hasRecipe) clauses << "RecipeName = ?";
    if (hasFrom)   clauses << "CreatedAt >= ?";
    // createdTo 语义为"半开区间上界"，调用方传入的应为"截止日的次日 0 点"
    if (hasTo)     clauses << "CreatedAt < ?";

    if (!clauses.isEmpty()) {
        sql += "WHERE " + clauses.join(" AND ") + " ";
    }
    sql += "ORDER BY CreatedAt DESC, Id DESC";
    return sql;
}

} // namespace

QList<InspectionRecordRow> InspectionRepository::loadRecent(int limit,
                                                            QString *errorMessage)
{
    InspectionQuery q;
    q.limit = limit;
    return loadFiltered(q, errorMessage);
}

QList<InspectionRecordRow> InspectionRepository::loadFiltered(const InspectionQuery &q,
                                                              QString *errorMessage)
{
    QList<InspectionRecordRow> rows;

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return rows;
    }

    // 老库首次查询前自动补齐 BackendTaskId 列等新结构
    if (!ensureSchema(errorMessage)) {
        return rows;
    }

    bool hasRecipe = false, hasFrom = false, hasTo = false;
    const QString sql = buildRecentSql(q, hasRecipe, hasFrom, hasTo);

    QSqlQuery query(db);
    query.prepare(sql);
    query.addBindValue(q.limit > 0 ? q.limit : 100);
    if (hasRecipe) query.addBindValue(q.recipe);
    if (hasFrom)   query.addBindValue(q.createdFrom);
    if (hasTo)     query.addBindValue(q.createdTo);

    if (!query.exec()) {
        reportError(errorMessage,
                    QStringLiteral("查询 InspectionRecord 失败: %1").arg(takeError(query)));
        return rows;
    }

    while (query.next()) {
        InspectionRecordRow row;
        row.id                = query.value("Id").toInt();
        row.backendTaskId     = static_cast<quint64>(query.value("BackendTaskId").toLongLong());
        row.recipeName        = query.value("RecipeName").toString();
        row.processStartAt    = query.value("ProcessStartAt").toDateTime();
        row.processEndAt      = query.value("ProcessEndAt").toDateTime();
        row.inspectStartAt    = query.value("InspectStartAt").toDateTime();
        row.inspectEndAt      = query.value("InspectEndAt").toDateTime();
        row.particleCount     = query.value("ParticleCount").toInt();
        row.clearedCount      = query.value("ClearedCount").toInt();
        row.maxParticleHeight = query.value("MaxParticleHeight").toDouble();
        row.overviewImagePath = query.value("OverviewImagePath").toString();
        row.remark            = query.value("Remark").toString();
        row.createdAt         = query.value("CreatedAt").toDateTime();
        rows.append(row);
    }
    return rows;
}

QStringList InspectionRepository::loadRecipeNames(QString *errorMessage)
{
    QStringList names;

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return names;
    }

    QSqlQuery query(db);
    query.prepare("SELECT DISTINCT RecipeName FROM dbo.InspectionRecord "
                  "WHERE RecipeName IS NOT NULL AND RecipeName <> '' "
                  "ORDER BY RecipeName");
    if (!query.exec()) {
        reportError(errorMessage,
                    QStringLiteral("查询配方列表失败: %1").arg(takeError(query)));
        return names;
    }
    while (query.next()) {
        names << query.value(0).toString();
    }
    return names;
}

InspectionDetail InspectionRepository::loadDetail(int inspectionRecordId,
                                                  QString *errorMessage)
{
    InspectionDetail detail;

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return detail;
    }

    if (!ensureSchema(errorMessage)) {
        return detail;
    }

    // 主记录
    {
        QSqlQuery q(db);
        q.prepare("SELECT Id, BackendTaskId, RecipeName, ProcessStartAt, ProcessEndAt, "
                  "       InspectStartAt, InspectEndAt, ParticleCount, ClearedCount, "
                  "       MaxParticleHeight, OverviewImagePath, Remark, CreatedAt "
                  "FROM dbo.InspectionRecord WHERE Id = ?");
        q.addBindValue(inspectionRecordId);
        if (!q.exec()) {
            reportError(errorMessage,
                        QStringLiteral("查询 InspectionRecord 失败: %1").arg(takeError(q)));
            return detail;
        }
        if (!q.next()) {
            reportError(errorMessage, QStringLiteral("找不到该检测记录"));
            return detail;
        }
        auto &row = detail.head;
        row.id                = q.value("Id").toInt();
        row.backendTaskId     = static_cast<quint64>(q.value("BackendTaskId").toLongLong());
        row.recipeName        = q.value("RecipeName").toString();
        row.processStartAt    = q.value("ProcessStartAt").toDateTime();
        row.processEndAt      = q.value("ProcessEndAt").toDateTime();
        row.inspectStartAt    = q.value("InspectStartAt").toDateTime();
        row.inspectEndAt      = q.value("InspectEndAt").toDateTime();
        row.particleCount     = q.value("ParticleCount").toInt();
        row.clearedCount      = q.value("ClearedCount").toInt();
        row.maxParticleHeight = q.value("MaxParticleHeight").toDouble();
        row.overviewImagePath = q.value("OverviewImagePath").toString();
        row.remark            = q.value("Remark").toString();
        row.createdAt         = q.value("CreatedAt").toDateTime();
    }

    // 颗粒明细
    {
        QSqlQuery q(db);
        q.prepare("SELECT ParticleIndex, PositionX, PositionY, PositionZ, Height, "
                  "       IsCleared, DetectedAt, ClearedAt "
                  "FROM dbo.ParticleDetection "
                  "WHERE InspectionRecordId = ? "
                  "ORDER BY ParticleIndex");
        q.addBindValue(inspectionRecordId);
        if (!q.exec()) {
            reportError(errorMessage,
                        QStringLiteral("查询颗粒明细失败: %1").arg(takeError(q)));
            return detail;
        }
        while (q.next()) {
            ParticleDetectionRow p;
            p.particleIndex = q.value("ParticleIndex").toInt();
            p.positionX     = q.value("PositionX").toDouble();
            p.positionY     = q.value("PositionY").toDouble();
            p.positionZ     = q.value("PositionZ").toDouble();
            p.height        = q.value("Height").toDouble();
            p.isCleared     = q.value("IsCleared").toBool();
            p.detectedAt    = q.value("DetectedAt").toDateTime();
            p.clearedAt     = q.value("ClearedAt").toDateTime();
            detail.particles.append(p);
        }
    }

    // 工艺快照（可能为空）
    {
        QSqlQuery q(db);
        q.prepare("SELECT PathIndex, MaxParticleHeight, TargetX, TargetY, TargetZ, "
                  "       FeedSpeed, FeedAmount, SpindleSpeed, CutCount, SingleCutAmount, "
                  "       SnapshotAt "
                  "FROM dbo.ProcessParameterHistory "
                  "WHERE InspectionRecordId = ? "
                  "ORDER BY PathIndex");
        q.addBindValue(inspectionRecordId);
        if (q.exec()) {
            while (q.next()) {
                ProcessParamHistoryRow p;
                p.pathIndex         = q.value("PathIndex").toInt();
                p.maxParticleHeight = q.value("MaxParticleHeight").toDouble();
                p.targetX           = q.value("TargetX").toDouble();
                p.targetY           = q.value("TargetY").toDouble();
                p.targetZ           = q.value("TargetZ").toDouble();
                p.feedSpeed         = q.value("FeedSpeed").toDouble();
                p.feedAmount        = q.value("FeedAmount").toDouble();
                p.spindleSpeed      = q.value("SpindleSpeed").toDouble();
                p.cutCount          = q.value("CutCount").toInt();
                p.singleCutAmount   = q.value("SingleCutAmount").toDouble();
                p.snapshotAt        = q.value("SnapshotAt").toDateTime();
                detail.processHistory.append(p);
            }
        }
        // 工艺快照查询失败不视为致命错误（模拟数据没有这份表）
    }

    // 扫描帧列表（按第几次扫描排序；旧库可能没有该表，失败不视为致命）
    {
        QSqlQuery q(db);
        q.prepare("SELECT Id, InspectionRecordId, BackendTaskId, FrameId, ScanOrdinal, "
                  "       TimestampNs, Width, Height, PixelFormat, "
                  "       RangePixelFormat, IntensityPixelFormat, HasRange, HasIntensity, "
                  "       CreatedAt "
                  "FROM dbo.InspectionFrame "
                  "WHERE InspectionRecordId = ? "
                  "ORDER BY ScanOrdinal, FrameId");
        q.addBindValue(inspectionRecordId);
        if (q.exec()) {
            while (q.next()) {
                InspectionFrameRow f;
                f.id                  = q.value("Id").toLongLong();
                f.inspectionRecordId  = q.value("InspectionRecordId").toInt();
                f.backendTaskId       = static_cast<quint64>(q.value("BackendTaskId").toLongLong());
                f.frameId             = static_cast<quint64>(q.value("FrameId").toLongLong());
                f.scanOrdinal         = q.value("ScanOrdinal").toInt();
                f.timestampNs         = static_cast<quint64>(q.value("TimestampNs").toLongLong());
                f.width               = q.value("Width").toInt();
                f.height              = q.value("Height").toInt();
                f.pixelFormat         = q.value("PixelFormat").toInt();
                f.rangePixelFormat    = q.value("RangePixelFormat").toInt();
                f.intensityPixelFormat = q.value("IntensityPixelFormat").toInt();
                f.hasRange            = q.value("HasRange").toBool();
                f.hasIntensity        = q.value("HasIntensity").toBool();
                f.createdAt           = q.value("CreatedAt").toDateTime();
                detail.frames.append(f);
            }
        }
    }

    detail.loaded = true;
    return detail;
}

int InspectionRepository::insertMockRecords(int count, QString *errorMessage)
{
    if (count <= 0) {
        return 0;
    }

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return 0;
    }

    auto *rng = QRandomGenerator::global();
    int inserted = 0;

    const QStringList recipeChoices = {
        QStringLiteral("DefaultRecipe"),
        QStringLiteral("HighSpeedA"),
        QStringLiteral("FineFinishB"),
    };

    for (int i = 0; i < count; ++i) {
        if (!db.transaction()) {
            reportError(errorMessage,
                        QStringLiteral("开启事务失败: %1").arg(db.lastError().text()));
            return inserted;
        }

        const QDateTime now = QDateTime::currentDateTime();
        const QDateTime processStart = now.addSecs(-static_cast<qint64>(rng->bounded(3600) + 60 * (count - i)));
        const QDateTime processEnd   = processStart.addSecs(60 + rng->bounded(240));
        const QDateTime inspectStart = processEnd.addSecs(5);
        const QDateTime inspectEnd   = inspectStart.addSecs(20 + rng->bounded(120));

        const int    particleCount = rng->bounded(3, 12);
        const int    clearedCount  = rng->bounded(0, particleCount + 1);
        const double maxHeight     = 0.1 + rng->generateDouble() * 0.6;
        const QString overviewPath = QStringLiteral("D:/mztlz/mock/overview_%1_%2.png")
                                         .arg(now.toString("yyyyMMddHHmmss"))
                                         .arg(i);
        const QString recipe = recipeChoices.at(rng->bounded(recipeChoices.size()));
        const QString remark = QStringLiteral("模拟任务 #%1").arg(i + 1);

        QSqlQuery insertHead(db);
        insertHead.prepare(
            "INSERT INTO dbo.InspectionRecord "
            "(RecipeName, ProcessStartAt, ProcessEndAt, InspectStartAt, InspectEndAt, "
            " ParticleCount, ClearedCount, MaxParticleHeight, OverviewImagePath, Remark, "
            " CreatedAt, UpdatedAt) "
            "OUTPUT INSERTED.Id "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, SYSUTCDATETIME(), SYSUTCDATETIME())");
        insertHead.addBindValue(recipe);
        insertHead.addBindValue(processStart);
        insertHead.addBindValue(processEnd);
        insertHead.addBindValue(inspectStart);
        insertHead.addBindValue(inspectEnd);
        insertHead.addBindValue(particleCount);
        insertHead.addBindValue(clearedCount);
        insertHead.addBindValue(maxHeight);
        insertHead.addBindValue(overviewPath);
        insertHead.addBindValue(remark);

        if (!insertHead.exec() || !insertHead.next()) {
            reportError(errorMessage,
                        QStringLiteral("写入 InspectionRecord 失败: %1")
                            .arg(takeError(insertHead)));
            db.rollback();
            return inserted;
        }

        const int newId = insertHead.value(0).toInt();

        // 模拟数据只写主记录和颗粒明细，不写 ProcessParameterHistory，
        // 避免与真实工艺快照的唯一约束冲突。
        if (!insertMockParticles(newId, particleCount, clearedCount, inspectStart, db, errorMessage)) {
            db.rollback();
            return inserted;
        }

        if (!db.commit()) {
            reportError(errorMessage,
                        QStringLiteral("提交事务失败: %1").arg(db.lastError().text()));
            db.rollback();
            return inserted;
        }
        ++inserted;
    }

    return inserted;
}

bool InspectionRepository::ensureSchema(QString *errorMessage)
{
    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return false;
    }

    // 每条语句都带 IF 保护，可重复执行（幂等增量迁移）
    static const char *kStatements[] = {
        // 1) InspectionRecord 增加后端任务号列
        "IF COL_LENGTH('dbo.InspectionRecord', 'BackendTaskId') IS NULL "
        "ALTER TABLE dbo.InspectionRecord ADD BackendTaskId BIGINT NULL",

        // 2) 过滤唯一索引：一个后端任务只对应一条记录（NULL 不参与唯一约束）
        "IF NOT EXISTS (SELECT 1 FROM sys.indexes "
        "WHERE name = 'UX_InspectionRecord_BackendTaskId' "
        "AND object_id = OBJECT_ID('dbo.InspectionRecord')) "
        "CREATE UNIQUE INDEX UX_InspectionRecord_BackendTaskId "
        "ON dbo.InspectionRecord(BackendTaskId) WHERE BackendTaskId IS NOT NULL",

        // 3) 扫描帧映射表：一个 task_id 下挂多个 frame_id
        "IF OBJECT_ID('dbo.InspectionFrame', 'U') IS NULL "
        "CREATE TABLE dbo.InspectionFrame ("
        "    Id BIGINT IDENTITY(1,1) NOT NULL PRIMARY KEY,"
        "    InspectionRecordId INT NOT NULL,"
        "    BackendTaskId BIGINT NOT NULL,"
        "    FrameId BIGINT NOT NULL,"
        "    ScanOrdinal INT NOT NULL DEFAULT 0,"
        "    TimestampNs BIGINT NOT NULL DEFAULT 0,"
        "    Width INT NOT NULL DEFAULT 0,"
        "    Height INT NOT NULL DEFAULT 0,"
        "    PixelFormat INT NOT NULL DEFAULT 0,"
        "    RangePixelFormat INT NOT NULL DEFAULT -1,"
        "    IntensityPixelFormat INT NOT NULL DEFAULT -1,"
        "    HasRange BIT NOT NULL DEFAULT 0,"
        "    HasIntensity BIT NOT NULL DEFAULT 0,"
        "    CreatedAt DATETIME2 NOT NULL DEFAULT SYSUTCDATETIME(),"
        "    CONSTRAINT FK_InspectionFrame_InspectionRecord "
        "      FOREIGN KEY (InspectionRecordId) REFERENCES dbo.InspectionRecord(Id) ON DELETE CASCADE,"
        "    CONSTRAINT UQ_InspectionFrame_Task_Frame UNIQUE (BackendTaskId, FrameId)"
        ")",

        // 4) 按检测记录查帧的索引
        "IF NOT EXISTS (SELECT 1 FROM sys.indexes "
        "WHERE name = 'IX_InspectionFrame_RecordId' "
        "AND object_id = OBJECT_ID('dbo.InspectionFrame')) "
        "CREATE INDEX IX_InspectionFrame_RecordId ON dbo.InspectionFrame(InspectionRecordId)",

        "IF COL_LENGTH('dbo.InspectionFrame', 'RangePixelFormat') IS NULL "
        "ALTER TABLE dbo.InspectionFrame ADD RangePixelFormat INT NOT NULL DEFAULT -1",

        "IF COL_LENGTH('dbo.InspectionFrame', 'IntensityPixelFormat') IS NULL "
        "ALTER TABLE dbo.InspectionFrame ADD IntensityPixelFormat INT NOT NULL DEFAULT -1",
    };

    for (const char *sql : kStatements) {
        QSqlQuery q(db);
        if (!q.exec(QString::fromLatin1(sql))) {
            reportError(errorMessage,
                        QStringLiteral("结构迁移失败: %1").arg(takeError(q)));
            return false;
        }
    }
    return true;
}

int InspectionRepository::ensureRecordForTask(quint64 backendTaskId,
                                              QString *errorMessage)
{
    if (backendTaskId == 0) {
        reportError(errorMessage, QStringLiteral("task_id 无效（0）"));
        return 0;
    }

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return 0;
    }
    if (!ensureSchema(errorMessage)) {
        return 0;
    }

    QVariant taskVar;
    if (!toSqlBigIntVariant(backendTaskId, QStringLiteral("task_id"), &taskVar, errorMessage)) {
        return 0;
    }

    // 已存在则直接返回
    {
        QSqlQuery q(db);
        q.prepare("SELECT Id FROM dbo.InspectionRecord WHERE BackendTaskId = ?");
        q.addBindValue(taskVar);
        if (!q.exec()) {
            reportError(errorMessage,
                        QStringLiteral("按 BackendTaskId 查询失败: %1").arg(takeError(q)));
            return 0;
        }
        if (q.next()) {
            return q.value("Id").toInt();
        }
    }

    // 不存在则创建；RecipeName 先写空串，避免污染配方筛选列表
    {
        QSqlQuery q(db);
        q.prepare("INSERT INTO dbo.InspectionRecord "
                  "(BackendTaskId, RecipeName, CreatedAt, UpdatedAt) "
                  "OUTPUT INSERTED.Id "
                  "VALUES (?, '', SYSUTCDATETIME(), SYSUTCDATETIME())");
        q.addBindValue(taskVar);
        if (q.exec() && q.next()) {
            return q.value(0).toInt();
        }
        // 并发/重复提交兜底：唯一索引冲突时重新查一次
        QSqlQuery again(db);
        again.prepare("SELECT Id FROM dbo.InspectionRecord WHERE BackendTaskId = ?");
        again.addBindValue(taskVar);
        if (again.exec() && again.next()) {
            return again.value("Id").toInt();
        }
        reportError(errorMessage,
                    QStringLiteral("创建检测任务记录失败: %1").arg(takeError(q)));
        return 0;
    }
}

bool InspectionRepository::saveFrameForTask(quint64 backendTaskId,
                                            quint64 frameId,
                                            quint64 timestampNs,
                                            int width,
                                            int height,
                                            int rangePixelFormat,
                                            int intensityPixelFormat,
                                            bool hasRange,
                                            bool hasIntensity,
                                            QString *errorMessage)
{
    if (backendTaskId == 0 || frameId == 0) {
        reportError(errorMessage, QStringLiteral("task_id / frame_id 无效（0）"));
        return false;
    }

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return false;
    }

    const int recordId = ensureRecordForTask(backendTaskId, errorMessage);
    if (recordId <= 0) {
        return false;
    }

    QVariant taskVar;
    QVariant frameVar;
    QVariant timestampVar;
    if (!toSqlBigIntVariant(backendTaskId, QStringLiteral("task_id"), &taskVar, errorMessage) ||
        !toSqlBigIntVariant(frameId, QStringLiteral("frame_id"), &frameVar, errorMessage) ||
        !toSqlBigIntVariant(timestampNs, QStringLiteral("timestamp_ns"), &timestampVar, errorMessage)) {
        return false;
    }

    const int legacyPixelFormat = rangePixelFormat >= 0
        ? rangePixelFormat
        : intensityPixelFormat;

    // (task_id, frame_id) 已存在时只补全标记/元数据，不重复建行——
    // 对应 range / intensity 两路消息先后到达的场景
    QSqlQuery q(db);
    q.prepare(
        "MERGE dbo.InspectionFrame WITH (HOLDLOCK) AS t "
        "USING (SELECT ? AS BackendTaskId, ? AS FrameId) AS s "
        "ON t.BackendTaskId = s.BackendTaskId AND t.FrameId = s.FrameId "
        "WHEN MATCHED THEN UPDATE SET "
        "    HasRange = CASE WHEN ? = 1 THEN 1 ELSE t.HasRange END, "
        "    HasIntensity = CASE WHEN ? = 1 THEN 1 ELSE t.HasIntensity END, "
        "    TimestampNs = CASE WHEN t.TimestampNs = 0 THEN ? ELSE t.TimestampNs END, "
        "    Width = CASE WHEN t.Width = 0 THEN ? ELSE t.Width END, "
        "    Height = CASE WHEN t.Height = 0 THEN ? ELSE t.Height END, "
        "    PixelFormat = CASE WHEN t.PixelFormat = 0 AND ? > 0 THEN ? ELSE t.PixelFormat END, "
        "    RangePixelFormat = CASE WHEN ? >= 0 THEN ? ELSE t.RangePixelFormat END, "
        "    IntensityPixelFormat = CASE WHEN ? >= 0 THEN ? ELSE t.IntensityPixelFormat END "
        "WHEN NOT MATCHED THEN INSERT "
        "    (InspectionRecordId, BackendTaskId, FrameId, ScanOrdinal, "
        "     TimestampNs, Width, Height, PixelFormat, RangePixelFormat, "
        "     IntensityPixelFormat, HasRange, HasIntensity) "
        "    VALUES (?, ?, ?, "
        "            (SELECT COUNT(1) + 1 FROM dbo.InspectionFrame WHERE BackendTaskId = ?), "
        "            ?, ?, ?, ?, ?, ?, ?, ?);");
    // USING
    q.addBindValue(taskVar);
    q.addBindValue(frameVar);
    // MATCHED
    q.addBindValue(hasRange ? 1 : 0);
    q.addBindValue(hasIntensity ? 1 : 0);
    q.addBindValue(timestampVar);
    q.addBindValue(width);
    q.addBindValue(height);
    q.addBindValue(legacyPixelFormat);
    q.addBindValue(legacyPixelFormat);
    q.addBindValue(rangePixelFormat);
    q.addBindValue(rangePixelFormat);
    q.addBindValue(intensityPixelFormat);
    q.addBindValue(intensityPixelFormat);
    // NOT MATCHED
    q.addBindValue(recordId);
    q.addBindValue(taskVar);
    q.addBindValue(frameVar);
    q.addBindValue(taskVar);
    q.addBindValue(timestampVar);
    q.addBindValue(width);
    q.addBindValue(height);
    q.addBindValue(legacyPixelFormat);
    q.addBindValue(rangePixelFormat);
    q.addBindValue(intensityPixelFormat);
    q.addBindValue(hasRange ? 1 : 0);
    q.addBindValue(hasIntensity ? 1 : 0);

    if (!q.exec()) {
        reportError(errorMessage,
                    QStringLiteral("写入 InspectionFrame 失败: %1").arg(takeError(q)));
        return false;
    }
    return true;
}
