# MzTLZ (local-restructure)

本地上位机 UI（Qt + ROS2），本分支为模块化目录重构版（src/ / include/）。

## 数据库：SQLite（非 SQL Server）

本分支使用 **本地 SQLite**，不是 ODBC / SQL Server。

- 配置：config/database.ini（默认 Path=data/mztlz.db，相对可执行文件目录）
- Qt 驱动：QSQLITE
- 启动时由 DatabaseBootstrap 创建/迁移表结构，再由 Database 单例打开连接

## 注意事项

### 急停（未完成）

标题栏「急停」**当前不会向 PLC 下发急停**。双击后主要是 toast / 事件日志提示；关闭窗口时会跑 config/stop_all.bat（以及相关进程 	askkill），用于停节点进程，**不是**闩锁式机床/PLC 急停。PLC 急停链路尚未实现，正式产线前需补齐设计与联调。

### Ctrl+T / Ctrl+Shift+T（演示假数据）

生产构建中仍保留调试快捷键：

- Ctrl+T：切换演示/假数据模式（可隐藏实时扫描画面、注入假 PLC 状态，并向数据库写入 task 9001 等）
- Ctrl+Shift+T：在假数据模式下模拟任务完成

正式发布前必须移除，或用编译宏/配置开关关掉，避免误触污染现场数据。

### config/start_all.bat（需保留，路径写死）

一键拉起算法 / 相机 / 后台节点，**请勿删除**。当前脚本内路径与参数多为硬编码，后续应外置到配置：

| 项 | 当前硬编码示例 |
|----|----------------|
| 算法 | D:\robot_client_move_test |
| 相机节点 | D:\Camera_Node |
| 后台 | D:\CopperMilling |
| ROS 前缀 | C:\opt\ros\foxy\x64 |
| PLC | plc_ip:=192.168.1.60（rack/slot 等） |

UI 侧将脚本 **exit code 0** 视为启动成功。换机部署前请先改这些路径。

### 海康 HCNetSDK 运行库

CMake 从 	hird_party/HCNetSDK/库文件 链接，并在 POST_BUILD 用 *.dll glob（含 HCNetSDKCom/）拷到输出目录。缺 DLL 时相机预览会失败。

## 布局速览

- src/ / include/：app、bridge、camera、data、ui、vision
- config/：ini 与 start/stop 脚本
- 	hird_party/：qcustomplot、HCNetSDK
- cmake/：辅助 CMake 模块
