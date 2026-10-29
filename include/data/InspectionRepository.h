#ifndef INSPECTIONREPOSITORY_H
#define INSPECTIONREPOSITORY_H

#include "bridge/MillingPathVM.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

// 打磨是否正常结束。0 表示还没有结束结果。
enum class ProcessOutcome {
    None = 0,
    Abnormal = 1,
    Completed = 2
};

// 人工在历史记录里做的标记。只有打磨正常结束后才能标。
enum class ManualReview {
    None = 0,
    Passed = 1,
    Abnormal = 2
};

// 一条记录当前展示的状态。复核完成要等打磨后的第二次扫描，现在不会写入。
enum class TaskStatus {
    Unset = 0,
    Abnormal = 1,
    MillCompleted = 2,
    ReviewCompleted = 3,
    ManualPassed = 4
};

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
    QString    plateNo;           // 用户填写的板号；空表示未编号
    QString    shift;             // 建记录时的当前班次；空表示当时未选班次
    ProcessOutcome processOutcome = ProcessOutcome::None;
    ManualReview manualReview = ManualReview::None;
    bool       reviewCompleted = false; // 仅演示数据使用；真实记录保持 false
    QDateTime  createdAt;
};

inline TaskStatus taskStatusOf(const InspectionRecordRow &row)
{
    if (row.manualReview == ManualReview::Abnormal
        || row.processOutcome == ProcessOutcome::Abnormal) {
        return TaskStatus::Abnormal;
    }
    if (row.manualReview == ManualReview::Passed) {
        return TaskStatus::ManualPassed;
    }
    if (row.reviewCompleted && row.processOutcome == ProcessOutcome::Completed) {
        return TaskStatus::ReviewCompleted;
    }
    if (row.processOutcome == ProcessOutcome::Completed) {
        return TaskStatus::MillCompleted;
    }
    return TaskStatus::Unset;
}

inline QString taskStatusText(TaskStatus status)
{
    switch (status) {
    case TaskStatus::Abnormal:
        return QStringLiteral("异常");
    case TaskStatus::MillCompleted:
        return QStringLiteral("修磨完成");
    case TaskStatus::ReviewCompleted:
        return QStringLiteral("复核完成");
    case TaskStatus::ManualPassed:
        return QStringLiteral("人工审核");
    case TaskStatus::Unset:
        break;
    }
    return QStringLiteral("未结束");
}

inline QString taskStatusDetail(const InspectionRecordRow &row)
{
    if (row.manualReview == ManualReview::Abnormal) {
        return QStringLiteral("异常（人工审核）");
    }
    if (row.manualReview == ManualReview::Passed) {
        return QStringLiteral("人工审核（通过）");
    }
    return taskStatusText(taskStatusOf(row));
}

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
    int      rangePixelFormat = -1;
    int      intensityPixelFormat = -1;
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
    QString    plate;         // 空则不筛选，按板号精确匹配（忽略大小写）
    QString    shift;         // 空则不筛选
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

    // 幂等结构迁移：补齐 BackendTaskId / InspectionFrame 等（SQLite）。
    // 老库首次启动自动补齐；可重复执行。
    static bool ensureSchema(QString *errorMessage = nullptr);

    // 按后端 task_id 查找或创建检测任务记录（幂等）。
    // 返回 InspectionRecord.Id；失败返回 0 并填 errorMessage。
    static int ensureRecordForTask(quint64 backendTaskId,
                                   QString *errorMessage = nullptr);

    // 已有记录返回板号；没有记录时返回空，不新建。
    static QString plateNoForTask(quint64 backendTaskId,
                                  QString *errorMessage = nullptr);

    // 写入板号。只保留字母和数字，空字符串表示未编号。记录不存在时会先创建。
    static bool setPlateNoForTask(quint64 backendTaskId,
                                  const QString &plateNo,
                                  QString *errorMessage = nullptr);

    // 把已经写入记录的班次名称改成新名称。历史行跟着改，避免汇总表对不上。
    static bool renameShift(const QString &from, const QString &to, QString *errorMessage = nullptr);

    // 写入打磨结束结果。已有人工标记时不再改写；正常结束后不再被改成异常。
    static bool setProcessOutcomeForTask(quint64 backendTaskId,
                                         ProcessOutcome outcome,
                                         QString *errorMessage = nullptr);

    // 人工审核：通过或异常。只有打磨正常结束的记录可以标记，两种结果可以互相改。
    static bool setManualReview(int inspectionRecordId,
                                ManualReview review,
                                QString *errorMessage = nullptr);

    // 用这次任务的粒子轮廓结果替换 ParticleDetection。高度单位毫米。
    static bool replaceParticlesForTask(quint64 backendTaskId,
                                        const QVector<ParticleRegionVM> &particles,
                                        QString *errorMessage = nullptr);

    // 这些检测记录下已保存的粒子高度，单位毫米。
    static QList<double> loadParticleHeights(const QList<int> &inspectionRecordIds,
                                             QString *errorMessage = nullptr);

    // 保存一帧扫描映射（task_id + frame_id 唯一，重发时补全 HasRange/HasIntensity）。
    // task_id 对应的 InspectionRecord 不存在时会先自动创建。
    static bool saveFrameForTask(quint64 backendTaskId,
                                 quint64 frameId,
                                 quint64 timestampNs,
                                 int width,
                                 int height,
                                 int rangePixelFormat,
                                 int intensityPixelFormat,
                                 bool hasRange,
                                 bool hasIntensity,
                                 QString *errorMessage = nullptr);
};

#endif // INSPECTIONREPOSITORY_H
