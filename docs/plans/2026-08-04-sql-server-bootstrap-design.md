# SQL Server 自动引导设计

## 目标

目标电脑只需安装并启动 SQL Server，不需要预先在 SSMS 中手工创建 `MzTLZ` 数据库和业务表。前端首次启动时自动完成连接探测、建库和基础结构初始化；后续启动重复执行不会破坏已有数据。

## 连接策略

EXE 同目录新增 `database.ini`：

```ini
[Database]
Server=localhost\SQLEXPRESS
Name=MzTLZ
```

前端优先尝试配置的实例，然后依次回退到 `localhost\SQLEXPRESS` 和 `localhost`，兼容 SQL Server Express 命名实例与默认实例。连接继续使用 Windows 集成认证，不在配置文件中保存账号密码。ODBC 驱动按 18、17、Native Client 11 和系统 `SQL Server` 驱动依次尝试。

## 初始化流程

新增独立的 `DatabaseBootstrap` 模块。程序启动数据库连接前，该模块先连接候选实例的 `master` 数据库，检查目标数据库是否存在；不存在时执行 `CREATE DATABASE`。随后连接目标数据库，幂等创建以下基础表：

- `InspectionRecord`：检测任务主记录。
- `ParticleDetection`：颗粒明细。
- `ProcessParameter`：当前工艺参数。
- `ProcessParameterHistory`：任务工艺快照。

基础表成功后，把实际可用的服务器和数据库名写入本进程环境，现有 `Database` 连接复用该结果。之后继续执行 `InspectionRepository::ensureSchema()`，补齐 `BackendTaskId`、`InspectionFrame` 和相关索引。

## 错误处理

数据库名只允许字母、数字和下划线，防止配置值进入标识符 SQL。若 SQL Server 服务未启动、Windows 用户没有建库权限、ODBC 驱动缺失或实例名错误，启动不会崩溃；现有数据库错误提示会显示最后一次实际连接错误，日志记录所有已尝试的驱动与实例。

## 验证

构建后临时将配置指向唯一测试库，启动前端，确认数据库和五张最终业务表均被创建。验证完成后仅删除该测试数据库，恢复部署配置为 `MzTLZ`，再重建部署包并进行直接启动检查。

