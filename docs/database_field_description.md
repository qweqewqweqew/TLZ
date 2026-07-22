# MzTLZ 数据库字段说明

## 概览

当前数据库：`MzTLZ`

当前表：

1. `dbo.ProcessParameter` — 工艺参数表（当前活动配方）
2. `dbo.InspectionRecord` — 检测记录表（一次任务的档案封面）
3. `dbo.ProcessParameterHistory` — 工艺参数历史快照表（该次任务加工时用的配方冻结副本）
4. `dbo.ParticleDetection` — 颗粒检测明细表（该次任务检测到的每颗粒）

表间关系：

```
ProcessParameter          （独立，当前正在用的配方）

InspectionRecord          （每次任务一行）
    ├── ProcessParameterHistory   （那次用的配方快照）
    └── ParticleDetection         （那次检测到的颗粒明细）
```

## 表：dbo.ProcessParameter

用途：存储当前正在使用的加工配方。一个配方由多条路径组成，每条路径一行。

| 字段 | 类型 | 含义 | 备注 |
|---|---|---|---|
| `Id` | `INT` | 主键 | 自增 |
| `RecipeName` | `NVARCHAR(100)` | 配方名 | 工艺模板名 |
| `PathIndex` | `INT` | 路径序号 | 该行在配方中的第几条路径 |
| `MaxParticleHeight` | `FLOAT` | 最大颗粒高度 | 工艺允许上限 |
| `TargetX` | `FLOAT` | 目标坐标 X | 路径终点 |
| `TargetY` | `FLOAT` | 目标坐标 Y | 路径终点 |
| `TargetZ` | `FLOAT` | 目标坐标 Z | 路径终点 |
| `FeedSpeed` | `FLOAT` | 进给速度 | |
| `FeedAmount` | `FLOAT` | 进给量 | |
| `SpindleSpeed` | `FLOAT` | 主轴转速 | |
| `CutCount` | `INT` | 切削次数 | |
| `SingleCutAmount` | `FLOAT` | 单次切削量 | |
| `CreatedAt` | `DATETIME2` | 创建时间 | 默认当前时间 |
| `UpdatedAt` | `DATETIME2` | 更新时间 | 默认当前时间 |

### 示例种子数据

当前测试配方名：`DefaultRecipe`

| RecipeName | PathIndex | TargetX | TargetY | TargetZ | FeedSpeed | FeedAmount | SpindleSpeed | CutCount | SingleCutAmount |
|---|---|---|---|---|---|---|---|---|---|
| `DefaultRecipe` | `1` | `100.0` | `50.0` | `10.0` | `120.0` | `0.8` | `3000.0` | `2` | `0.4` |
| `DefaultRecipe` | `2` | `120.0` | `55.0` | `10.5` | `120.0` | `0.8` | `3000.0` | `2` | `0.4` |
| `DefaultRecipe` | `3` | `140.0` | `60.0` | `11.0` | `120.0` | `0.8` | `3000.0` | `2` | `0.4` |

## 表：dbo.InspectionRecord

用途：一次加工/检测任务的档案封面，每次任务一行。

| 字段 | 类型 | 含义 | 备注 |
|---|---|---|---|
| `Id` | `INT` | 主键 | 自增 |
| `RecipeName` | `NVARCHAR(200)` | 使用的配方名 | |
| `ProcessStartAt` | `DATETIME2` | 加工开始时间 | 可空 |
| `ProcessEndAt` | `DATETIME2` | 加工结束时间 | 可空 |
| `InspectStartAt` | `DATETIME2` | 检测开始时间 | 可空 |
| `InspectEndAt` | `DATETIME2` | 检测结束时间 | 可空 |
| `ParticleCount` | `INT` | 检测到的颗粒总数 | |
| `ClearedCount` | `INT` | 已清除颗粒数 | |
| `MaxParticleHeight` | `FLOAT` | 实测最大颗粒高度 | 本次实测值 |
| `OverviewImagePath` | `NVARCHAR(1000)` | 全景图片路径 | 可空 |
| `Remark` | `NVARCHAR(1000)` | 备注 | 可空 |
| `CreatedAt` | `DATETIME2` | 创建时间 | 默认当前时间 |
| `UpdatedAt` | `DATETIME2` | 更新时间 | 默认当前时间 |

## 表：dbo.ProcessParameterHistory

用途：加工时用的配方冻结副本。工艺参数表随时会被改，历史任务需要能查到当时用的参数，所以在任务开始时把当时的配方复制到这里。字段结构与 `ProcessParameter` 保持一致，另加溯源字段。

| 字段 | 类型 | 含义 | 备注 |
|---|---|---|---|
| `Id` | `BIGINT` | 主键 | 自增 |
| `InspectionRecordId` | `INT` | 属于哪次检测记录 | 关联 `InspectionRecord.Id` |
| `RecipeName` | `NVARCHAR(200)` | 配方名 | |
| `PathIndex` | `INT` | 路径序号 | |
| `MaxParticleHeight` | `FLOAT` | 最大颗粒高度 | |
| `TargetX` | `FLOAT` | 目标坐标 X | |
| `TargetY` | `FLOAT` | 目标坐标 Y | |
| `TargetZ` | `FLOAT` | 目标坐标 Z | |
| `FeedSpeed` | `FLOAT` | 进给速度 | |
| `FeedAmount` | `FLOAT` | 进给量 | |
| `SpindleSpeed` | `FLOAT` | 主轴转速 | |
| `CutCount` | `INT` | 切削次数 | |
| `SingleCutAmount` | `FLOAT` | 单次切削量 | |
| `SnapshotAt` | `DATETIME2` | 快照生成时间 | |

约束：`(InspectionRecordId, PathIndex)` 唯一。

## 表：dbo.ParticleDetection

用途：一次检测中识别到的每一颗颗粒。挂在 `InspectionRecord` 下面，一对多。

| 字段 | 类型 | 含义 | 备注 |
|---|---|---|---|
| `Id` | `BIGINT` | 主键 | 自增 |
| `InspectionRecordId` | `INT` | 属于哪次检测记录 | 关联 `InspectionRecord.Id` |
| `ParticleIndex` | `INT` | 颗粒序号 | 本次任务内的编号 |
| `PositionX` | `FLOAT` | 颗粒位置 X | 可空 |
| `PositionY` | `FLOAT` | 颗粒位置 Y | 可空 |
| `PositionZ` | `FLOAT` | 颗粒位置 Z | 可空 |
| `Height` | `FLOAT` | 颗粒高度 | 可空 |
| `IsCleared` | `BIT` | 是否已清除 | |
| `ClearedAt` | `DATETIME2` | 清除时间 | 可空 |
| `DetectedAt` | `DATETIME2` | 检测时间 | |

## 对外说明（一句话）

- `ProcessParameter`：工艺配置和路径配置（当前正在使用的配方）
- `InspectionRecord`：每次任务的档案封面
- `ProcessParameterHistory`：那次任务加工时用的配方快照
- `ParticleDetection`：那次任务检测到的颗粒明细

## 当前备注

- `ProcessParameter` 已有 6 行测试数据（配方 `DefaultRecipe`，3 条路径）
- 其余三张表当前为空
- 下一步可选事项：
  1. 补充外键约束（`ParticleDetection.InspectionRecordId`、`ProcessParameterHistory.InspectionRecordId`）
  2. 为 `ProcessParameter.RecipeName + PathIndex` 增加唯一约束
  3. 导出为 Word / Excel 供交付
