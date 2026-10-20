# MzTLZ (local-restructure)

本地上位机 UI（Qt + ROS2），本分支为模块化目录重构版（src/ / include/）。

## 快速开始

### 编译项目

```cmd
build.bat
```

这将自动：
- 配置 Visual Studio 2019 环境
- 使用 CMake 生成构建文件到 `build-local/`
- 编译 Release 版本
- 部署可执行文件到 `deploy/MzTLZ/`

### 运行程序

**方法1：从部署目录运行（推荐）**
```cmd
deploy\MzTLZ\MzTLZ.exe
```
或双击 `config/run.bat`

**方法2：从构建目录运行（开发调试）**
```cmd
build-local\Release\MzTLZ.exe
```

### 注意
- 编译前请关闭正在运行的 MzTLZ.exe，否则链接器无法覆盖文件
- 确保已安装 Visual Studio 2019、Qt 5.15.2、OpenCV 和 ROS2 Foxy

---

## 数据库：SQLite（非 SQL Server）

本分支使用 **本地 SQLite**，不是 ODBC / SQL Server。

- 配置：config/database.ini（默认 Path=data/mztlz.db，相对可执行文件目录）
- Qt 驱动：QSQLITE
- 启动时由 DatabaseBootstrap 创建/迁移表结构，再由 Database 单例打开连接

## 注意事项

### 启动 / 暂停 / 停止按钮（待后端联调）

「修磨实时状态」卡片中的**启动、暂停、停止按钮当前处于禁用状态**。该功能需要后端 PLC 控制接口配合实现，包括：
- 任务启动确认与路径下发
- 运行中暂停/恢复机制
- 停止后的安全回零与状态同步

正式联调前请与后端团队确认接口协议，目前仅作为 UI 占位。

### Ctrl+T / Ctrl+Shift+T（调试阶段）

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

CMake 从 third_party/HCNetSDK/库文件 链接，并在 POST_BUILD 用 *.dll glob（含 HCNetSDKCom/）拷到输出目录。缺 DLL 时相机预览会失败。

## 布局速览

- src/ / include/：app、bridge、camera、data、ui、vision
- config/：ini 与 start/stop 脚本
- third_party/：qcustomplot、HCNetSDK
- cmake/：辅助 CMake 模块

## 性能优化（2024-10-13）

最近优化了 Ctrl+T 演示模式的性能和波形图显示：

### 1. 磨削轨迹增量绘制
- **问题**：每 50ms 完整重绘整个轨迹图层，导致卡顿
- **优化**：改为增量绘制，只画新增线段
- **效果**：性能从 O(n) 降至 O(1)，轨迹越长优化越明显

### 2. 波形图线条显示
- **问题**：动态特性曲线显示为填充"带状区域"
- **优化**：禁用 QCustomPlot 自适应采样
- **效果**：波形图显示为清晰线条

详见修改的文件：
- `src/vision/ImageViewFrame.cpp` - 增量绘制逻辑
- `include/vision/ImageViewFrame.h` - 新增函数声明
- `src/ui/panels/DataStatisticsPanel.cpp` - 禁用自适应采样

---

## 脚本说明

项目根目录仅保留以下构建脚本：

- **build.bat** - 统一的构建脚本（清理、配置、编译、部署）
- **config/run.bat** - 从部署目录运行程序
- **config/start_all.bat** - 启动相机/算法/后台节点
- **config/stop_all.bat** - 停止所有节点

**⚠️ 禁止创建临时脚本**：如必须生成临时脚本（如调试），使用后立即删除。

## 报表统计页：字段与数据来源

报表页当前优先使用 SQLite 已有的 `InspectionRecord` 数据，时间范围、检测铜板数、有粒子铜板数、修磨完成数、粒子总数、完成率、平均修磨节拍和任务级最大粒子高度均来自现有记录。

为了便于在后端数据量不足时检查页面布局，报表页会在当前查询结果少于 36 条时补充内存中的演示数据。演示数据不会写入 SQLite，状态栏、班次汇总和 PDF 导出会标记“含演示数据，仅供界面预览”；当真实记录达到 36 条时，页面自动只展示真实查询结果。

以下字段本轮已在界面中预留，但数据库暂时没有可靠来源：

- **班次**：当前只有“全部班次 / 甲班 / 乙班 / 丙班”界面选项，查询暂不按班次过滤，每日表和 PDF 中显示 `--`。
- **铜板编号**：当前数据库没有板号字段，输入框只作为界面占位，不参与查询。
- **粒子高度明细分布**：当前报表使用 `InspectionRecord.MaxParticleHeight` 按任务分段绘制环形占比图，单颗粒 `ParticleDetection.Height` 的时间范围聚合接口尚未接入。
- **统一任务状态**：界面预留“复测通过、待复核、异常、修磨中、待修磨”五类状态；当前只能根据 `ParticleCount` 与 `ClearedCount` 推导修磨中和待修磨，其余状态暂显示为 0，需要后端提供统一状态字段。
- **班次汇总**：每日班次汇总表预留甲班、乙班、丙班三列，暂显示 `--`；检测合计和修磨合计仍来自现有记录。
- **Excel 导出**：项目尚未接入 QXlsx，按钮保持禁用；PDF 导出已包含班次占位列。

后续由后端补齐字段和查询接口，或在确认不需要这些维度后由前端移除对应控件。替换数据来源时，应同步更新报表页的筛选、图表、表格和 PDF 导出逻辑。
