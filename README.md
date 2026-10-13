# MzTLZ (local-restructure)

本地上位机 UI（Qt + ROS2），本分支为模块化目录重构版（`src/` / `include/`）。

## 数据库：SQLite（非 SQL Server）

本分支使用 **本地 SQLite**，不是 ODBC / SQL Server（`main` 分支仍是 SQL Server）。

- 配置：`config/database.ini`（默认 `Path=data/mztlz.db`，相对可执行文件目录）
- Qt 驱动：`QSQLITE`（部署目录需有 `sqldrivers/qsqlite.dll`）
- 启动时由 `DatabaseBootstrap` 创建/迁移表结构，再由 `Database` 单例打开连接
- 旧 SQL Server 中的历史记录不会自动迁移到 SQLite

## 2D 视图：刀具位置与路径进度

- 收到 `plc/feedback` 后，2D 图上用半透明圆点显示 `path_params.x_pos / y_pos` 对应的位置，并带约 0.4 s 的拖尾
- 映射使用当前扫描帧的 `scan_geometry`（`x_offset_mm` / `x_scale_mm`，有编码器值时按行查找 Y）；没有几何信息或点落在图外时不显示
- 路径线按 PLC `completed_path` / `current_path` 着色：已完成为灰色，当前为黄色，未执行为淡蓝色；下发新路径或清空路径时进度归零
- 没有进行中的任务时，PLC 反馈不会显示刀具圆点；圆点重绘限制在约 30 Hz
- 磨削轨迹（半透明笔刷）目前只在 Ctrl+T 演示模式中绘制

前提：PLC 坐标与扫描图坐标处于同一坐标系（mm）。若两者之间有标定偏移，圆点会整体偏移或不显示。

## 注意事项

### 急停（未完成）

标题栏「急停」**当前不会向 PLC 下发急停**。双击后只有 toast 和事件日志提示。关闭窗口时会运行 `config/stop_all.bat`（对相关进程执行 `taskkill`），用于停止节点进程，**不是**闩锁式机床/PLC 急停。PLC 急停链路尚未实现，正式产线前需补齐设计与联调。

### Ctrl+T / Ctrl+Shift+T（演示假数据）

生产构建中仍保留调试快捷键：

- `Ctrl+T`：切换演示模式。加载 `TestImage`，在 2D 图上沿蛇形路径动画显示刀具与磨削轨迹，并注入假的 PLC 反馈（task 9001）。演示期间真实扫描帧不会刷新画面
- `Ctrl+Shift+T`：在演示模式下模拟任务完成

演示期间的任务/扫描帧不会写入 SQLite（只按演示开关判断，真实的 task 9001 照常写库）。正式发布前仍建议用编译宏或配置开关关掉，避免误触。

### config/start_all.bat（需保留，路径写死）

一键拉起算法 / 相机 / 后台节点，**请勿删除**。当前脚本内路径与参数多为硬编码，后续应外置到配置：

| 项 | 当前硬编码示例 |
|----|----------------|
| 算法 | `D:\robot_client_move_test` |
| 相机节点 | `D:\Camera_Node` |
| 后台 | `D:\CopperMilling` |
| ROS 前缀 | `C:\opt\ros\foxy\x64` |
| PLC | `plc_ip:=192.168.1.60`（rack/slot 等） |

UI 侧将脚本 **exit code 0** 视为启动成功。换机部署前请先改这些路径。

### 海康 HCNetSDK 运行库

CMake 从 `third_party/HCNetSDK/库文件` 链接 `.lib`，并在 POST_BUILD 用 `*.dll` glob（含 `HCNetSDKCom/`）拷到输出目录。**仓库里目前只有 `.lib`，没有 `.dll`**，需要手动把 SDK 的运行库放进该目录，否则相机实时预览会在加载时失败。

## 构建

Windows + VS2019 + Qt 5.15.2（msvc2019_64）+ ROS 2 Foxy + OpenCV：

```bat
build.bat
```

产物在 `build\Release\MzTLZ.exe`，打包目录为 `deploy\MzTLZ`。注意 `build.bat` 会用 `config/` 里的 ini 覆盖 `deploy\MzTLZ` 中的同名文件（`backend_params.ini` 除外）。

`mz_interfaces` 默认从 `../Camera_Node/install/mz_interfaces` 查找，可用 `MZ_INTERFACES_PREFIX` 覆盖。

## 测试

`tests/` 是独立的 Qt Test 工程，只依赖 Qt5 Core/Gui/Widgets/Test（不需要 ROS、HCNetSDK），覆盖 `ImageViewFrame` 的路径进度、刀具重绘节流和磨削笔宽缩放：

```bat
cmake -S tests -B build-tests
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release
```

测试以 `-platform offscreen` 运行，可在无显示环境（CI）执行。

## 布局速览

- `src/` / `include/`：`app`、`bridge`、`camera`、`data`、`ui`、`vision`
- `config/`：ini 与 start/stop 脚本
- `third_party/`：`qcustomplot`、`HCNetSDK`
- `cmake/`：辅助 CMake 模块
- `tests/`：独立的 Qt Test 工程
