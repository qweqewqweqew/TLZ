#include "data/InspectionRepository.h"

#include "app/ShiftConfig.h"
#include "data/Database.h"

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
                    QStringLiteral("%1 超出 INTEGER 范围: %2")
                        .arg(fieldName)
                        .arg(value));
        return false;
    }
    if (out) {
        *out = QVariant::fromValue<qlonglong>(static_cast<qlonglong>(value));
    }
    return true;
}

bool tableExists(QSqlDatabase &db, const QString &tableName)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1"));
    q.addBindValue(tableName);
    return q.exec() && q.next();
}

bool columnExists(QSqlDatabase &db, const QString &tableName, const QString &columnName)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        return false;
    }
    while (q.next()) {
        if (q.value(1).toString().compare(columnName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool indexExists(QSqlDatabase &db, const QString &indexName)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type='index' AND name=? LIMIT 1"));
    q.addBindValue(indexName);
    return q.exec() && q.next();
}

bool execSql(QSqlDatabase &db, const QString &sql, QString *errorMessage)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        reportError(errorMessage, takeError(q));
        return false;
    }
    return true;
}

// 从 ProcessParameter 表把当前 recipe 的所有路径读出来，
// 作为一次任务的"工艺快照"写入 ProcessParameterHistory。
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
                 "FROM ProcessParameter "
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
            "INSERT INTO ProcessParameterHistory "
            "(InspectionRecordId, RecipeName, PathIndex, MaxParticleHeight, "
            " TargetX, TargetY, TargetZ, FeedSpeed, FeedAmount, SpindleSpeed, "
            " CutCount, SingleCutAmount, SnapshotAt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))");
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

void fillRecord(const QSqlQuery &query, InspectionRecordRow &row)
{
    row.id = query.value(QStringLiteral("Id")).toInt();
    row.backendTaskId = static_cast<quint64>(query.value(QStringLiteral("BackendTaskId")).toLongLong());
    row.recipeName = query.value(QStringLiteral("RecipeName")).toString();
    row.processStartAt = query.value(QStringLiteral("ProcessStartAt")).toDateTime();
    row.processEndAt = query.value(QStringLiteral("ProcessEndAt")).toDateTime();
    row.inspectStartAt = query.value(QStringLiteral("InspectStartAt")).toDateTime();
    row.inspectEndAt = query.value(QStringLiteral("InspectEndAt")).toDateTime();
    row.particleCount = query.value(QStringLiteral("ParticleCount")).toInt();
    row.clearedCount = query.value(QStringLiteral("ClearedCount")).toInt();
    row.maxParticleHeight = query.value(QStringLiteral("MaxParticleHeight")).toDouble();
    row.overviewImagePath = query.value(QStringLiteral("OverviewImagePath")).toString();
    row.remark = query.value(QStringLiteral("Remark")).toString();
    row.plateNo = query.value(QStringLiteral("PlateNo")).toString();
    row.shift = query.value(QStringLiteral("Shift")).toString();
    const int processValue = query.value(QStringLiteral("ProcessOutcome")).toInt();
    row.processOutcome = processValue == static_cast<int>(ProcessOutcome::Abnormal)
                             ? ProcessOutcome::Abnormal
                             : processValue == static_cast<int>(ProcessOutcome::Completed)
                                   ? ProcessOutcome::Completed
                                   : ProcessOutcome::None;
    const int reviewValue = query.value(QStringLiteral("ManualReview")).toInt();
    row.manualReview = reviewValue == static_cast<int>(ManualReview::Passed)
                           ? ManualReview::Passed
                           : reviewValue == static_cast<int>(ManualReview::Abnormal)
                                 ? ManualReview::Abnormal
                                 : ManualReview::None;
    row.reviewCompleted = false;
    row.createdAt = query.value(QStringLiteral("CreatedAt")).toDateTime();
}

QString normalizePlateNo(const QString &plateNo)
{
    QString normalized;
    normalized.reserve(plateNo.size());
    for (const QChar ch : plateNo) {
        const bool digit = ch.isDigit();
        const bool letter = (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
                            || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'));
        if (digit || letter) {
            normalized.append(ch);
        }
        if (normalized.size() >= 32) {
            break;
        }
    }
    return normalized;
}

QString buildRecentSql(const InspectionQuery &q,
                       bool &hasRecipe,
                       bool &hasFrom,
                       bool &hasTo,
                       bool &hasPlate,
                       bool &hasShift)
{
    QString sql =
        "SELECT Id, BackendTaskId, RecipeName, ProcessStartAt, ProcessEndAt, "
        "       InspectStartAt, InspectEndAt, ParticleCount, ClearedCount, "
        "       MaxParticleHeight, OverviewImagePath, Remark, PlateNo, Shift, "
        "       ProcessOutcome, ManualReview, CreatedAt "
        "FROM InspectionRecord ";

    QStringList clauses;
    hasRecipe = !q.recipe.isEmpty();
    hasPlate = !q.plate.isEmpty();
    hasShift = !q.shift.isEmpty();
    hasFrom = q.createdFrom.isValid();
    hasTo = q.createdTo.isValid();

    if (hasRecipe) {
        clauses << "RecipeName = ?";
    }
    if (hasPlate) {
        clauses << "PlateNo = ? COLLATE NOCASE";
    }
    if (hasShift) {
        clauses << "Shift = ?";
    }
    if (hasFrom) {
        clauses << "CreatedAt >= ?";
    }
    if (hasTo) {
        clauses << "CreatedAt < ?";
    }

    if (!clauses.isEmpty()) {
        sql += "WHERE " + clauses.join(" AND ") + " ";
    }
    sql += "ORDER BY CreatedAt DESC, Id DESC LIMIT ?";
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

    if (!ensureSchema(errorMessage)) {
        return rows;
    }

    bool hasRecipe = false;
    bool hasFrom = false;
    bool hasTo = false;
    bool hasPlate = false;
    bool hasShift = false;
    const QString sql = buildRecentSql(q, hasRecipe, hasFrom, hasTo, hasPlate, hasShift);

    QSqlQuery query(db);
    query.prepare(sql);
    if (hasRecipe) {
        query.addBindValue(q.recipe);
    }
    if (hasPlate) {
        query.addBindValue(q.plate);
    }
    if (hasShift) {
        query.addBindValue(q.shift);
    }
    if (hasFrom) {
        query.addBindValue(q.createdFrom);
    }
    if (hasTo) {
        query.addBindValue(q.createdTo);
    }
    query.addBindValue(q.limit > 0 ? q.limit : 100);

    if (!query.exec()) {
        reportError(errorMessage,
                    QStringLiteral("查询 InspectionRecord 失败: %1").arg(takeError(query)));
        return rows;
    }

    while (query.next()) {
        InspectionRecordRow row;
        fillRecord(query, row);
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
    query.prepare("SELECT DISTINCT RecipeName FROM InspectionRecord "
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

    {
        QSqlQuery q(db);
        q.prepare("SELECT Id, BackendTaskId, RecipeName, ProcessStartAt, ProcessEndAt, "
                  "       InspectStartAt, InspectEndAt, ParticleCount, ClearedCount, "
                  "       MaxParticleHeight, OverviewImagePath, Remark, PlateNo, Shift, "
        "       ProcessOutcome, ManualReview, CreatedAt "
                  "FROM InspectionRecord WHERE Id = ?");
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
        fillRecord(q, detail.head);
    }

    {
        QSqlQuery q(db);
        q.prepare("SELECT ParticleIndex, PositionX, PositionY, PositionZ, Height, "
                  "       IsCleared, DetectedAt, ClearedAt "
                  "FROM ParticleDetection "
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
            p.positionX = q.value("PositionX").toDouble();
            p.positionY = q.value("PositionY").toDouble();
            p.positionZ = q.value("PositionZ").toDouble();
            p.height = q.value("Height").toDouble();
            p.isCleared = q.value("IsCleared").toBool();
            p.detectedAt = q.value("DetectedAt").toDateTime();
            p.clearedAt = q.value("ClearedAt").toDateTime();
            detail.particles.append(p);
        }
    }

    {
        QSqlQuery q(db);
        q.prepare("SELECT PathIndex, MaxParticleHeight, TargetX, TargetY, TargetZ, "
                  "       FeedSpeed, FeedAmount, SpindleSpeed, CutCount, SingleCutAmount, "
                  "       SnapshotAt "
                  "FROM ProcessParameterHistory "
                  "WHERE InspectionRecordId = ? "
                  "ORDER BY PathIndex");
        q.addBindValue(inspectionRecordId);
        if (q.exec()) {
            while (q.next()) {
                ProcessParamHistoryRow p;
                p.pathIndex = q.value("PathIndex").toInt();
                p.maxParticleHeight = q.value("MaxParticleHeight").toDouble();
                p.targetX = q.value("TargetX").toDouble();
                p.targetY = q.value("TargetY").toDouble();
                p.targetZ = q.value("TargetZ").toDouble();
                p.feedSpeed = q.value("FeedSpeed").toDouble();
                p.feedAmount = q.value("FeedAmount").toDouble();
                p.spindleSpeed = q.value("SpindleSpeed").toDouble();
                p.cutCount = q.value("CutCount").toInt();
                p.singleCutAmount = q.value("SingleCutAmount").toDouble();
                p.snapshotAt = q.value("SnapshotAt").toDateTime();
                detail.processHistory.append(p);
            }
        }
    }

    {
        QSqlQuery q(db);
        q.prepare("SELECT Id, InspectionRecordId, BackendTaskId, FrameId, ScanOrdinal, "
                  "       TimestampNs, Width, Height, PixelFormat, "
                  "       RangePixelFormat, IntensityPixelFormat, HasRange, HasIntensity, "
                  "       CreatedAt "
                  "FROM InspectionFrame "
                  "WHERE InspectionRecordId = ? "
                  "ORDER BY ScanOrdinal, FrameId");
        q.addBindValue(inspectionRecordId);
        if (q.exec()) {
            while (q.next()) {
                InspectionFrameRow f;
                f.id = q.value("Id").toLongLong();
                f.inspectionRecordId = q.value("InspectionRecordId").toInt();
                f.backendTaskId = static_cast<quint64>(q.value("BackendTaskId").toLongLong());
                f.frameId = static_cast<quint64>(q.value("FrameId").toLongLong());
                f.scanOrdinal = q.value("ScanOrdinal").toInt();
                f.timestampNs = static_cast<quint64>(q.value("TimestampNs").toLongLong());
                f.width = q.value("Width").toInt();
                f.height = q.value("Height").toInt();
                f.pixelFormat = q.value("PixelFormat").toInt();
                f.rangePixelFormat = q.value("RangePixelFormat").toInt();
                f.intensityPixelFormat = q.value("IntensityPixelFormat").toInt();
                f.hasRange = q.value("HasRange").toBool();
                f.hasIntensity = q.value("HasIntensity").toBool();
                f.createdAt = q.value("CreatedAt").toDateTime();
                detail.frames.append(f);
            }
        }
    }

    detail.loaded = true;
    return detail;
}

bool InspectionRepository::ensureSchema(QString *errorMessage)
{
    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return false;
    }

    QSqlQuery pragma(db);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        reportError(errorMessage,
                    QStringLiteral("启用外键失败: %1").arg(takeError(pragma)));
        return false;
    }

    // 新库由 Bootstrap 建全；这里只做幂等补齐（旧文件/增量列）。
    if (!tableExists(db, QStringLiteral("InspectionRecord"))) {
        reportError(errorMessage, QStringLiteral("缺少 InspectionRecord 表，请先完成数据库引导"));
        return false;
    }

    if (!columnExists(db, QStringLiteral("InspectionRecord"), QStringLiteral("BackendTaskId"))) {
        if (!execSql(db,
                     QStringLiteral("ALTER TABLE InspectionRecord ADD COLUMN BackendTaskId INTEGER NULL"),
                     errorMessage)) {
            return false;
        }
    }

    if (!indexExists(db, QStringLiteral("UX_InspectionRecord_BackendTaskId"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "CREATE UNIQUE INDEX UX_InspectionRecord_BackendTaskId "
                         "ON InspectionRecord(BackendTaskId) WHERE BackendTaskId IS NOT NULL"),
                     errorMessage)) {
            return false;
        }
    }

    if (!tableExists(db, QStringLiteral("InspectionFrame"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "CREATE TABLE InspectionFrame ("
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
                         ")"),
                     errorMessage)) {
            return false;
        }
    }

    if (!columnExists(db, QStringLiteral("InspectionFrame"), QStringLiteral("RangePixelFormat"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "ALTER TABLE InspectionFrame ADD COLUMN RangePixelFormat INTEGER NOT NULL DEFAULT -1"),
                     errorMessage)) {
            return false;
        }
    }

    if (!columnExists(db, QStringLiteral("InspectionFrame"), QStringLiteral("IntensityPixelFormat"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "ALTER TABLE InspectionFrame ADD COLUMN IntensityPixelFormat INTEGER NOT NULL DEFAULT -1"),
                     errorMessage)) {
            return false;
        }
    }

    if (!indexExists(db, QStringLiteral("IX_InspectionFrame_RecordId"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "CREATE INDEX IX_InspectionFrame_RecordId ON InspectionFrame(InspectionRecordId)"),
                     errorMessage)) {
            return false;
        }
    }

    if (!columnExists(db, QStringLiteral("InspectionRecord"), QStringLiteral("PlateNo"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "ALTER TABLE InspectionRecord ADD COLUMN PlateNo TEXT NOT NULL DEFAULT ''"),
                     errorMessage)) {
            return false;
        }
    }

    if (!columnExists(db, QStringLiteral("InspectionRecord"), QStringLiteral("Shift"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "ALTER TABLE InspectionRecord ADD COLUMN Shift TEXT NOT NULL DEFAULT ''"),
                     errorMessage)) {
            return false;
        }
    }

    if (!columnExists(db, QStringLiteral("InspectionRecord"), QStringLiteral("ProcessOutcome"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "ALTER TABLE InspectionRecord ADD COLUMN ProcessOutcome INTEGER NOT NULL DEFAULT 0"),
                     errorMessage)) {
            return false;
        }
    }

    if (!columnExists(db, QStringLiteral("InspectionRecord"), QStringLiteral("ManualReview"))) {
        if (!execSql(db,
                     QStringLiteral(
                         "ALTER TABLE InspectionRecord ADD COLUMN ManualReview INTEGER NOT NULL DEFAULT 0"),
                     errorMessage)) {
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

    {
        QSqlQuery q(db);
        q.prepare("SELECT Id FROM InspectionRecord WHERE BackendTaskId = ?");
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

    {
        QSqlQuery q(db);
        q.prepare("INSERT INTO InspectionRecord "
                  "(BackendTaskId, RecipeName, Shift, CreatedAt, UpdatedAt) "
                  "VALUES (?, '', ?, datetime('now'), datetime('now'))");
        q.addBindValue(taskVar);
        q.addBindValue(ShiftConfig::current());
        if (q.exec()) {
            const QVariant id = q.lastInsertId();
            if (id.isValid()) {
                return id.toInt();
            }
        }

        QSqlQuery again(db);
        again.prepare("SELECT Id FROM InspectionRecord WHERE BackendTaskId = ?");
        again.addBindValue(taskVar);
        if (again.exec() && again.next()) {
            return again.value("Id").toInt();
        }
        reportError(errorMessage,
                    QStringLiteral("创建检测任务记录失败: %1").arg(takeError(q)));
        return 0;
    }
}

QString InspectionRepository::plateNoForTask(quint64 backendTaskId, QString *errorMessage)
{
    if (backendTaskId == 0) {
        return QString();
    }

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return QString();
    }
    if (!ensureSchema(errorMessage)) {
        return QString();
    }

    QVariant taskVar;
    if (!toSqlBigIntVariant(backendTaskId, QStringLiteral("task_id"), &taskVar, errorMessage)) {
        return QString();
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT PlateNo FROM InspectionRecord WHERE BackendTaskId = ?"));
    q.addBindValue(taskVar);
    if (!q.exec()) {
        reportError(errorMessage, QStringLiteral("查询板号失败: %1").arg(takeError(q)));
        return QString();
    }
    if (!q.next()) {
        return QString();
    }
    return q.value(QStringLiteral("PlateNo")).toString();
}

bool InspectionRepository::setPlateNoForTask(quint64 backendTaskId,
                                             const QString &plateNo,
                                             QString *errorMessage)
{
    const int recordId = ensureRecordForTask(backendTaskId, errorMessage);
    if (recordId <= 0) {
        return false;
    }

    QSqlDatabase db = Database::instance().handle();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE InspectionRecord SET PlateNo = ?, UpdatedAt = datetime('now') WHERE Id = ?"));
    q.addBindValue(normalizePlateNo(plateNo));
    q.addBindValue(recordId);
    if (!q.exec()) {
        reportError(errorMessage, QStringLiteral("保存板号失败: %1").arg(takeError(q)));
        return false;
    }
    return true;
}

bool InspectionRepository::renameShift(const QString &from, const QString &to, QString *errorMessage)
{
    const QString oldName = from.trimmed();
    const QString newName = to.trimmed();
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName) {
        return true;
    }

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return false;
    }
    if (!ensureSchema(errorMessage)) {
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE InspectionRecord SET Shift = ?, UpdatedAt = datetime('now') WHERE Shift = ?"));
    q.addBindValue(newName);
    q.addBindValue(oldName);
    if (!q.exec()) {
        reportError(errorMessage, QStringLiteral("更新历史班次失败: %1").arg(takeError(q)));
        return false;
    }
    return true;
}

bool InspectionRepository::replaceParticlesForTask(quint64 backendTaskId,
                                                  const QVector<ParticleRegionVM> &particles,
                                                  QString *errorMessage)
{
    const int recordId = ensureRecordForTask(backendTaskId, errorMessage);
    if (recordId <= 0) {
        return false;
    }

    QSqlDatabase db = Database::instance().handle();
    QSqlQuery remove(db);
    remove.prepare(QStringLiteral("DELETE FROM ParticleDetection WHERE InspectionRecordId = ?"));
    remove.addBindValue(recordId);
    if (!remove.exec()) {
        reportError(errorMessage, QStringLiteral("清理粒子明细失败: %1").arg(takeError(remove)));
        return false;
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO ParticleDetection "
        "(InspectionRecordId, ParticleIndex, PositionX, PositionY, PositionZ, Height) "
        "VALUES (?, ?, ?, ?, 0, ?)"));
    for (int i = 0; i < particles.size(); ++i) {
        const ParticleRegionVM &particle = particles.at(i);
        const double u = (particle.uMin + particle.uMax) * 0.5;
        const double v = (particle.vMin + particle.vMax) * 0.5;
        insert.bindValue(0, recordId);
        insert.bindValue(1, particle.index >= 0 ? particle.index : i);
        insert.bindValue(2, u);
        insert.bindValue(3, v);
        insert.bindValue(4, double(particle.height));
        if (!insert.exec()) {
            reportError(errorMessage, QStringLiteral("写入粒子明细失败: %1").arg(takeError(insert)));
            return false;
        }
    }
    return true;
}

QList<double> InspectionRepository::loadParticleHeights(const QList<int> &inspectionRecordIds,
                                                        QString *errorMessage)
{
    QList<double> heights;
    if (inspectionRecordIds.isEmpty()) {
        return heights;
    }
    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return heights;
    }
    if (!ensureSchema(errorMessage)) {
        return heights;
    }

    for (int offset = 0; offset < inspectionRecordIds.size(); offset += 200) {
        const int count = qMin(200, inspectionRecordIds.size() - offset);
        QStringList marks;
        marks.reserve(count);
        for (int i = 0; i < count; ++i) {
            marks << QStringLiteral("?");
        }
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
                          "SELECT Height FROM ParticleDetection "
                          "WHERE InspectionRecordId IN (%1) AND Height > 0")
                          .arg(marks.join(QStringLiteral(","))));
        for (int i = 0; i < count; ++i) {
            query.addBindValue(inspectionRecordIds.at(offset + i));
        }
        if (!query.exec()) {
            reportError(errorMessage, QStringLiteral("查询粒子高度失败: %1").arg(takeError(query)));
            return heights;
        }
        while (query.next()) {
            heights.append(query.value(0).toDouble());
        }
    }
    return heights;
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

    if (!db.transaction()) {
        reportError(errorMessage, QStringLiteral("开启事务失败: %1").arg(db.lastError().text()));
        return false;
    }

    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO InspectionFrame "
        "    (InspectionRecordId, BackendTaskId, FrameId, ScanOrdinal, "
        "     TimestampNs, Width, Height, PixelFormat, RangePixelFormat, "
        "     IntensityPixelFormat, HasRange, HasIntensity) "
        "VALUES (?, ?, ?, "
        "        (SELECT COUNT(1) + 1 FROM InspectionFrame WHERE BackendTaskId = ?), "
        "        ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(BackendTaskId, FrameId) DO UPDATE SET "
        "    HasRange = CASE WHEN excluded.HasRange = 1 THEN 1 ELSE InspectionFrame.HasRange END, "
        "    HasIntensity = CASE WHEN excluded.HasIntensity = 1 THEN 1 ELSE InspectionFrame.HasIntensity END, "
        "    TimestampNs = CASE WHEN InspectionFrame.TimestampNs = 0 THEN excluded.TimestampNs "
        "                       ELSE InspectionFrame.TimestampNs END, "
        "    Width = CASE WHEN InspectionFrame.Width = 0 THEN excluded.Width ELSE InspectionFrame.Width END, "
        "    Height = CASE WHEN InspectionFrame.Height = 0 THEN excluded.Height ELSE InspectionFrame.Height END, "
        "    PixelFormat = CASE WHEN InspectionFrame.PixelFormat = 0 AND excluded.PixelFormat > 0 "
        "                       THEN excluded.PixelFormat ELSE InspectionFrame.PixelFormat END, "
        "    RangePixelFormat = CASE WHEN excluded.RangePixelFormat >= 0 THEN excluded.RangePixelFormat "
        "                            ELSE InspectionFrame.RangePixelFormat END, "
        "    IntensityPixelFormat = CASE WHEN excluded.IntensityPixelFormat >= 0 "
        "                                THEN excluded.IntensityPixelFormat "
        "                                ELSE InspectionFrame.IntensityPixelFormat END");

    q.addBindValue(recordId);
    q.addBindValue(taskVar);
    q.addBindValue(frameVar);
    q.addBindValue(taskVar); // ScanOrdinal subquery
    q.addBindValue(timestampVar);
    q.addBindValue(width);
    q.addBindValue(height);
    q.addBindValue(legacyPixelFormat);
    q.addBindValue(rangePixelFormat);
    q.addBindValue(intensityPixelFormat);
    q.addBindValue(hasRange ? 1 : 0);
    q.addBindValue(hasIntensity ? 1 : 0);

    if (!q.exec()) {
        db.rollback();
        reportError(errorMessage,
                    QStringLiteral("写入 InspectionFrame 失败: %1").arg(takeError(q)));
        return false;
    }

    if (!db.commit()) {
        reportError(errorMessage, QStringLiteral("提交事务失败: %1").arg(db.lastError().text()));
        return false;
    }
    return true;
}

bool InspectionRepository::setProcessOutcomeForTask(quint64 backendTaskId,
                                                    ProcessOutcome outcome,
                                                    QString *errorMessage)
{
    if (outcome != ProcessOutcome::Abnormal && outcome != ProcessOutcome::Completed) {
        reportError(errorMessage, QStringLiteral("打磨结果无效"));
        return false;
    }

    const int recordId = ensureRecordForTask(backendTaskId, errorMessage);
    if (recordId <= 0) {
        return false;
    }

    QSqlDatabase db = Database::instance().handle();
    QSqlQuery read(db);
    read.prepare(QStringLiteral(
        "SELECT ProcessOutcome, ManualReview FROM InspectionRecord WHERE Id = ?"));
    read.addBindValue(recordId);
    if (!read.exec()) {
        reportError(errorMessage, QStringLiteral("查询打磨结果失败: %1").arg(takeError(read)));
        return false;
    }
    if (!read.next()) {
        reportError(errorMessage, QStringLiteral("找不到该检测记录"));
        return false;
    }

    const int current = read.value(0).toInt();
    const int manual = read.value(1).toInt();
    if (manual == static_cast<int>(ManualReview::Passed)
        || manual == static_cast<int>(ManualReview::Abnormal)) {
        return true;
    }
    if (outcome == ProcessOutcome::Abnormal
        && current == static_cast<int>(ProcessOutcome::Completed)) {
        return true;
    }
    if (current == static_cast<int>(outcome)) {
        return true;
    }

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE InspectionRecord SET ProcessOutcome = ?, UpdatedAt = datetime('now') WHERE Id = ?"));
    update.addBindValue(static_cast<int>(outcome));
    update.addBindValue(recordId);
    if (!update.exec()) {
        reportError(errorMessage, QStringLiteral("保存打磨结果失败: %1").arg(takeError(update)));
        return false;
    }
    return true;
}

bool InspectionRepository::setManualReview(int inspectionRecordId,
                                           ManualReview review,
                                           QString *errorMessage)
{
    if (review != ManualReview::Passed && review != ManualReview::Abnormal) {
        reportError(errorMessage, QStringLiteral("人工审核结果无效"));
        return false;
    }
    if (inspectionRecordId <= 0) {
        reportError(errorMessage, QStringLiteral("检测记录无效"));
        return false;
    }

    QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        reportError(errorMessage, QStringLiteral("数据库未连接"));
        return false;
    }
    if (!ensureSchema(errorMessage)) {
        return false;
    }

    QSqlQuery read(db);
    read.prepare(QStringLiteral("SELECT ProcessOutcome FROM InspectionRecord WHERE Id = ?"));
    read.addBindValue(inspectionRecordId);
    if (!read.exec()) {
        reportError(errorMessage, QStringLiteral("查询打磨结果失败: %1").arg(takeError(read)));
        return false;
    }
    if (!read.next()) {
        reportError(errorMessage, QStringLiteral("找不到该检测记录"));
        return false;
    }
    if (read.value(0).toInt() != static_cast<int>(ProcessOutcome::Completed)) {
        reportError(errorMessage, QStringLiteral("只有打磨正常结束后才能人工审核"));
        return false;
    }

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE InspectionRecord SET ManualReview = ?, UpdatedAt = datetime('now') WHERE Id = ?"));
    update.addBindValue(static_cast<int>(review));
    update.addBindValue(inspectionRecordId);
    if (!update.exec()) {
        reportError(errorMessage, QStringLiteral("保存人工审核失败: %1").arg(takeError(update)));
        return false;
    }
    return true;
}
