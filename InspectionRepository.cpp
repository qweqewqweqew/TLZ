#include "InspectionRepository.h"

#include "Database.h"

#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

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
        "SELECT TOP (?) Id, RecipeName, ProcessStartAt, ProcessEndAt, "
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

    // 主记录
    {
        QSqlQuery q(db);
        q.prepare("SELECT Id, RecipeName, ProcessStartAt, ProcessEndAt, "
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
