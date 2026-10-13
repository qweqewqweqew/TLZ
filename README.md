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

### 急停（软件急停，需后端配合）

双击标题栏「急停」会向 ROS 2 topic `plc/emergency_stop`（`std_msgs/Bool`，`true`，reliable）发送急停指令，并按订阅情况提示：

| 情况 | 界面提示 |
|------|----------|
| 有节点订阅 | 「急停指令已发送」 |
| 无节点订阅 | 「急停可能未生效」，要求立即使用硬件急停 |
| ROS 2 未运行 | 「急停未发送」，要求立即使用硬件急停 |

**后端（CopperMilling / milling_controller）需要订阅 `plc/emergency_stop` 并写 PLC 停机位**，否则指令无人接收。软件急停不能替代硬件急停回路。关闭窗口时运行的 `stop_all.bat` 只是 `taskkill` 节点进程，也不是机床急停。

### Ctrl+T / Ctrl+Shift+T（演示假数据）

- `Ctrl+T`：切换演示模式。加载 `TestImage`，在 2D 图上沿蛇形路径动画显示刀具与磨削轨迹，并注入假的 PLC 反馈（task 9001）。演示期间真实扫描帧不会刷新画面
- `Ctrl+Shift+T`：在演示模式下模拟任务完成

演示期间的任务/扫描帧不会写入 SQLite（只按演示开关判断，真实的 task 9001 照常写库）。

快捷键由 CMake 选项 `MZ_ENABLE_DEMO_MODE` 控制，默认开启。现场包用 `build.bat --no-demo` 构建即可去掉这两个快捷键。

### config/start_all.bat（路径可覆盖）

一键拉起算法 / 相机 / 后台节点，**请勿删除**。UI 侧将脚本 **exit code 0** 视为启动成功。

换机时不要改脚本本身：把 `start_all.local.bat.example` 复制为同目录下的 `start_all.local.bat`，只改不同的项（该文件不进 git，`build.bat` 也不会覆盖它）。也可以直接设置同名环境变量。

| 变量 | 默认值 |
|------|--------|
| `MZ_ALGO_ROOT` | `D:\robot_client_move_test` |
| `MZ_CAMERA_DIR` | `D:\Camera_Node` |
| `MZ_BACKEND_ROOT` | `D:\CopperMilling` |
| `MZ_ROS_PREFIX` | `C:\opt\ros\foxy\x64` |
| `MZ_OPENCV_BIN` | `C:\opencv\build\x64\vc15\bin` |
| `MZ_PLC_IP` | `192.168.1.60` |
| `MZ_MILLING_ARGS` | 完整覆盖 milling_controller 参数（设置后 `MZ_PLC_IP` 不生效） |

### 海康 HCNetSDK 运行库

CMake 从 `third_party/HCNetSDK/库文件` 链接 `.lib`，并在 POST_BUILD 用 `*.dll` glob（含 `HCNetSDKCom/`）拷到输出目录。**仓库里目前只有 `.lib`，没有 `.dll`**：`.gitignore` 里的全局 `*.dll` 规则把它们挡掉了。需要手动把 SDK 的运行库放进该目录，否则相机实时预览会在加载时失败；缺 DLL 时 CMake 配置阶段会给出警告。

## 构建

Windows + VS2019 + Qt 5.15.2（msvc2019_64）+ ROS 2 Foxy + OpenCV：

```bat
build.bat
REM 现场包（去掉 Ctrl+T 演示）：
build.bat --no-demo
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
