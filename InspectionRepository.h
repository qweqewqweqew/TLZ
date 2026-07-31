#ifndef INSPECTIONREPOSITORY_H
#define INSPECTIONREPOSITORY_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

// 检测记录一行（历史查询列表用）
struct InspectionRecordRow
{
    int        id = 0;
    quint64    backendTaskId = 0;   // 后端任务号（无符号展示用；库里为 BIGINT）
    QString    recipeName;
    QDateTime  processStartAt;
    QDateTime  processEndAt;
    QDateTime  inspectStartAt;
    QDateTime  inspectEndAt;
    int        particleCount = 0;
    int        clearedCount = 0;
    double     maxParticleHeight = 0.0;
    QString    overviewImagePath;
    QString    remark;
    QDateTime  createdAt;
};

// 单颗颗粒明细
struct ParticleDetectionRow
{
    int        particleIndex = 0;
    double     positionX = 0.0;
    double     positionY = 0.0;
    double     positionZ = 0.0;
    double     height = 0.0;
    bool       isCleared = false;
    QDateTime  detectedAt;
    QDateTime  clearedAt;
};

// 工艺快照单条路径
struct ProcessParamHistoryRow
{
    int        pathIndex = 0;
    double     maxParticleHeight = 0.0;
    double     targetX = 0.0;
    double     targetY = 0.0;
    double     targetZ = 0.0;
    double     feedSpeed = 0.0;
    double     feedAmount = 0.0;
    double     spindleSpeed = 0.0;
    int        cutCount = 0;
    double     singleCutAmount = 0.0;
    QDateTime  snapshotAt;
};

// 一帧扫描图像的映射记录（一次任务允许多帧：初检/复检/确认检测…）
struct InspectionFrameRow
{
    qint64   id = 0;
    int      inspectionRecordId = 0;
    quint64  backendTaskId = 0;
    quint64  frameId = 0;
    int      scanOrdinal = 0;      // 该任务内第几次扫描（从 1 开始）
    quint64  timestampNs = 0;
    int      width = 0;
    int      height = 0;
    int      pixelFormat = 0;
    bool     hasRange = false;
    bool     hasIntensity = false;
    QDateTime createdAt;
};

// 一次检测任务的完整明细
struct InspectionDetail
{
    InspectionRecordRow            head;
    QList<ParticleDetectionRow>    particles;
    QList<ProcessParamHistoryRow>  processHistory;
    QList<InspectionFrameRow>      frames;
    bool                           loaded = false;
};

// 列表加载的过滤条件
struct InspectionQuery
{
    int        limit = 100;
    QString    recipe;        // 空则不筛选
    QDateTime  createdFrom;   // 无效则不筛选（应传入 UTC）
    QDateTime  createdTo;     // 半开区间上界（应传入 UTC），无效则不筛选
};

class InspectionRepository
{
public:
    // 兼容旧接口：按 CreatedAt 倒序取 limit 条
    static QList<InspectionRecordRow> loadRecent(int limit,
                                                 QString *errorMessage = nullptr);

    // 支持配方 / 时间范围过滤
    static QList<InspectionRecordRow> loadFiltered(const InspectionQuery &q,
                                                   QString *errorMessage = nullptr);

    // 数据库里已经出现过的所有配方名（用于筛选下拉）
    static QStringList loadRecipeNames(QString *errorMessage = nullptr);

    // 某条检测记录的完整明细（颗粒明细 + 工艺快照）
    static InspectionDetail loadDetail(int inspectionRecordId,
                                       QString *errorMessage = nullptr);

    // 一次性插入若干条随机模拟数据（用于演示历史查询），返回实际插入行数
    static int insertMockRecords(int count,
                                 QString *errorMessage = nullptr);

    // 幂等结构迁移：BackendTaskId 列 + 过滤唯一索引 + InspectionFrame 表。
    // 老库首次启动自动补齐；所有语句均为 IF 保护，可重复执行。
    static bool ensureSchema(QString *errorMessage = nullptr);

    // 按后端 task_id 查找或创建检测任务记录（幂等）。
    // 返回 InspectionRecord.Id；失败返回 0 并填 errorMessage。
    static int ensureRecordForTask(quint64 backendTaskId,
                                   QString *errorMessage = nullptr);

    // 保存一帧扫描映射（task_id + frame_id 唯一，重发时补全 HasRange/HasIntensity）。
    // task_id 对应的 InspectionRecord 不存在时会先自动创建。
    static bool saveFrameForTask(quint64 backendTaskId,
                                 quint64 frameId,
                                 quint64 timestampNs,
                                 int width,
                                 int height,
                                 int pixelFormat,
                                 bool hasRange,
                                 bool hasIntensity,
                                 QString *errorMessage = nullptr);
};

#endif // INSPECTIONREPOSITORY_H
