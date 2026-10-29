# MzTLZ (local-restructure)

本地上位机 UI（Qt + ROS2），本分支为模块化目录重构版（src/ / include/）。

## 快速开始

### 编译项目

```cmd
build.bat
```

这将自动：
- 配置 Visual Studio 2019 环境
- 使用 CMake 生成构建文件到 `build/`
- 编译 Release 版本
- 部署可执行文件及运行依赖到 `deploy/MzTLZ/`

### 运行程序

**方法1：通过运行脚本启动（推荐）**
```cmd
config\run.bat
```

脚本会先设置 Qt、OpenCV、ROS2 和本地 `mz_interfaces` 运行环境，再启动 `deploy/MzTLZ/MzTLZ.exe`。直接双击 EXE 可能缺少 ROS2 环境，不作为首选启动方式。

**方法2：从构建目录运行（开发调试）**
```cmd
build\Release\MzTLZ.exe
```

此方式同样需要自行准备 Qt、OpenCV、ROS2 和接口 DLL 环境。

### 注意
- 编译前请关闭正在运行的 `MzTLZ.exe`，否则链接或部署覆盖可能失败。
- 确保已安装 Visual Studio 2019、Qt 5.15.2、OpenCV 和 ROS2 Foxy。
- 主工程使用工作区 `.deps/install` 中的 `mz_interfaces`，部署时复制其中的接口 DLL，避免加载旧接口导致 SICK 消息符号缺失。
- 仅当 `.deps/src/mz_interfaces` 的消息定义变化或 `.deps/install` 缺失时，先执行 `.deps\build_interfaces.bat`，再执行 `build.bat`；普通 UI 修改不需要重复构建接口。
- 当前构建和运行脚本包含本机绝对路径（Qt、OpenCV、ROS2 及 `D:\TLZ\MZTLZ`）。迁移工作区或换机时必须同步修改，不能只复制 EXE。

---

## 数据接入约定（后续修改必须遵守）

1. **真实来源优先**：设备、ROS2、数据库或配置文件没有返回值时，界面保持“未获取／未设置／未采集／未标定”，不得用示例值代替。
2. **代码默认值不等于现场当前值**：需求或算法代码中的初始值只能标注为“代码默认值”，不得伪装成已回读数据。
3. **读写边界一致**：只有显示用途的页面必须只读；没有确认写入链路、权限、失败反馈和安全约束前，不开放编辑、保存或下发。
4. **接入留痕**：每接通一项数据，都要记录字段、单位、来源 topic／文件／表、读写方向、空值策略和负责模块，并同步删除对应待办。
5. **演示数据隔离**：演示功能只能在明确的调试模式下启用，不得写入生产 SQLite，也不得在正常查询中自动补数据。

## 数据库：SQLite（非 SQL Server）

本分支使用 **本地 SQLite**，不是 ODBC / SQL Server。

- 配置：config/database.ini（默认 Path=data/mztlz.db，相对可执行文件目录）
- Qt 驱动：QSQLITE
- 启动时由 DatabaseBootstrap 创建/迁移表结构，再由 Database 单例打开连接
- 设置页“数据与存储”只展示 SQLite 数据表名称、表用途和字段定义，不读取或呈现任何业务记录内容；结构表固定高度且不显示滚动条。
- 图像目录通过 `InspectionRecord.OverviewImagePath` 的实际值定位。当前前端只读取该字段供历史详情显示，没有负责保存生产图像或写入该字段的逻辑；后端需在保存检测总览图时一并写入可访问的绝对路径（或相对于程序目录的路径）。当数据库没有图像路径记录时，设置页会显示“暂无图像记录”并禁用目录按钮，不会自行猜测生产目录；已有记录若指向测试素材目录，也会按记录中的实际路径打开。

## 设备与通信

- 左侧为海康实时预览相机、SICK 3D 相机节点、PLC 控制系统、数据与图像存储四项。海康仅用于实时查看修磨过程；SICK 为线扫 3D 相机，通过独立相机节点采集数据。
- 海康页面不显示或修改 Topic、采集触发方式和 3D 点云参数；保存其他设置时保留 SICK 内部 Topic 与触发配置。主／子码流、预览帧率、亮度、对比度、曝光和强光抑制均在连接后通过 HCNetSDK 读取设备真实值，修改立即写入并回读，不经过底部“保存／应用”按钮。帧率使用 `NET_DVR_COMPRESSIONCFG_V30.dwVideoFrameRate` 原始枚举；图像参数使用 `NET_DVR_CAMERAPARAMCFG`。未连接、等待首次回读、读取失败或设备不支持时对应控件禁用，不用占位值代替设备值。
- SICK 页面已接入节点状态监控与四项真实采集参数：单帧采集行数、曝光时间、HDR 模式、Y 分辨率。状态来自 `camera/status`；参数通过 `sick_camera/parameters` 回读，通过 `sick_camera/set_parameters` 请求修改，并以 `sick_camera/parameter_result` 的结果和回读值为准。未收到回读时显示等待状态并禁用编辑，不使用模拟默认值；扫描采集期间不允许修改参数。
- PLC 页面因配置内容尚未确认而保持空白。未收到反馈时只显示“待反馈”，不生成模拟状态。

## 检测与标定、修磨工艺（空状态与真实来源待接入清单）

> **无真实来源的数据已经清空，禁止再用示例值伪装成现场有效数据。**

2026-10-28 已执行空状态清理：无来源数值显示“未设置”，无来源模式不默认选中，坐标点对为 0 行，转换矩阵显示“—”，点云显示“未采集”；铜板假示意、一键 `+20 mm`、调试 `±50 mm` 和未确认的安全余量公式均已删除。下面的历史占位值仅用于追溯，不得重新填入界面。

这两页仍未读取或写入 `m_sJggyPara`，未读取相机采集配置、`sListPPtsImage` / `sListPPtsRobot` 或标定结果，也不会写 ini、发 ROS topic 或下发 PLC。保存、取消、恢复默认不对这两页开放。

- “工艺规划”中的数字只是下文列出的代码默认值，不是现场当前值；所有数值框、下拉框和模式按钮均已锁定为只读，不能通过界面修改。刀盘方向进给等界面换算只使用这些初始值，不代表设备实测结果。
- “标定与点云”只保留点云、坐标点对、转换模式和矩阵的空状态；原先三个无实时来源的算法阈值输入框已移除，算法参数统一放到独立“算法”标签。转换模式名称来自需求，但当前选择未绑定算法枚举，不会保存、重算矩阵或下发。

### 算法只读参数：`program_param.json` 待接入

- “检测与标定”使用独立的“标定与点云 / 算法”标签，“修磨工艺”使用独立的“工艺规划 / 算法”标签。算法页只显示参数中文名称、用途、当前值和单位，不提供编辑、保存或下发功能。
- **当前尚未提供 `program_param.json` 的实际文件路径，也尚未实现文件读取器。** 两页所有算法值均显示“未获取”，不读取说明文档充当配置，不使用界面副本或模拟默认值。`program_param参数说明.txt` 仅用于核对字段含义和选择适合展示的参数。
- 待提供实际路径后，使用 `QJsonDocument` 等实现只读加载，并可在文件变化时刷新界面；界面不得写回 JSON。加载失败、字段缺失或文件尚不存在时继续显示“未获取”，不得回退到假数据。
- 每个值标签的对象名为 `craftReadOnlyValue`，JSON 字段名保存在 `algorithmJsonKey` 属性中，后续读取器据此回填，不另建一套界面参数副本。
- `region_x1 / region_y1 / region_x2 / region_y2` 应组合为一个 ROI 显示；`robot_wait_x / robot_wait_y / robot_wait_z` 应组合为一个等待坐标显示。布尔值显示“开 / 关”，枚举显示对应中文含义；`stepover <= 0` 的自动步距、`edge_feather_depth_step_mm = 0` 的自动级距等情况必须明确标注“自动”，不能只显示原始数字。
- `r_speed / x_speed / y_speed / z_speed / no_load_speed` 的单位尚未由说明文档确认，界面只提示“速度单位以算法配置为准”，接入时不得擅自标为 mm/min。
- `src_save`、`video_save`、`max_output_folders` 属于输出存储策略，按需求不在两个算法页展示。
- 检测算法映射字段：`target_extraction_method`、`targetsize_height_thresh`、`kmeans_distance`、`bspline_min_particle_area_mm2`、`grinding_strictness_level`、`isolated_mosaic_min_particle_area_mm2`、`isolated_mosaic_neighbor_distance_mm`、`plate_edge_safety_margin_px`、`plate_boundary_exclusion_px`、`enable_plate_lower_edge_line_detection`、`plate_lower_edge_search_px`、`region_x1 / region_y1 / region_x2 / region_y2`、`bspline_control_points_x`、`bspline_control_points_y`、`bspline_smoothing`、`bspline_fit_sample_step`、`bspline_refinement_iterations`、`bspline_outlier_rejection_ratio`、`bspline_baseline_low_quantile`、`bspline_max_particle_width_pixels`、`bspline_max_particle_aspect_ratio`。
- 修磨算法映射字段：`tool_size`、`tool_center_dead_zone_diameter_mm`、`stepover`、`single_down`、`grinding_pass_strategy`、`r_speed`、`x_speed`、`y_speed`、`z_speed`、`no_load_speed`、`enable_target_mask_scan`、`target_mask_run_merge_gap_mm`、`enableContinuousYTransition`、`enable_edge_feathering`、`edge_feather_width_mm`、`edge_feather_depth_step_mm`、`safetyZ`、`rapid_clearance_margin_mm`、`offset`、`robot_wait_x / robot_wait_y / robot_wait_z`、`enable_min_robot_y_path_filter`、`min_robot_path_y`、`force_nearest_first_region`、`enable_genetic_path_optimization`、`genetic_population_size`、`genetic_generations`。

“安全与权限”已接入 `AccountSettingsPage`，使用真实账户服务显示当前身份、用户列表、角色权限矩阵、会话超时、登录锁定策略和操作审计。用户列表在未登录和无管理权限时也可只读查看；账户写操作仍要求管理员权限，审计无权限时保持空状态，不生成示例用户或日志。

对应实现文件：

- `src/ui/settings/CalibrationPlaceholderPage.cpp`：检测与标定。
- `src/ui/settings/ProcessCraftPlaceholderPage.cpp`：修磨工艺。
- `src/ui/settings/CraftPlaceholder.cpp`：两页共享控件样式，不是数据来源。
- `src/ui/settings/AccountSettingsPage.cpp`：真实账户、权限和审计数据。

### 1. 已清除的无来源占位值

下表记录曾经用于展示控件和布局的默认值，**这些值现已从界面清除**。后续确认真实字段时，必须重新核对默认值、单位、取值范围和模式枚举，不能直接沿用界面按钮序号。

| 页面 | 控件 / 数据 | 原占位值或行为（已清除） | 后续真实来源与处理 |
| --- | --- | --- | --- |
| 检测与标定 | 单帧采集行数 | `2500` 行 | 相机或采集节点的真实配置；未提供默认值依据 |
| 检测与标定 | 相机曝光时间 | `450.0 μs` | 相机或采集节点的真实配置；未提供默认值依据 |
| 检测与标定 | Y 轴步进分辨率 | `0.05128 mm/行`，显示 5 位小数 | 实际运动／采集标定参数；5 位小数需求已明确，但当前数值没有来源 |
| 检测与标定 | HDR 模式 | 默认选中“自动切换（复检）”，界面序号 `1` | 后续复检真实 HDR 控制字段；初检关闭、复检开启的逻辑尚未接入 |
| 检测与标定 | 扫描物理长度 | 初始显示 `128.20000 mm` | 由占位的 `2500 × 0.05128` 即时计算，不是独立测量值 |
| 检测与标定 | 粒子轴尖高度阈值 | `0.0 mm` | 粒子识别算法阈值，当前无来源 |
| 检测与标定 | 粒子最小识别面积 | `4.0 mm²` | 粒子识别算法阈值，当前无来源 |
| 检测与标定 | 点云 Z 向滤波 | `15.0 mm` | 点云滤波算法参数，当前无来源 |
| 检测与标定 | 3D 点云预览 | 固定文案“等待粒子区域数据”，没有点云 | 接入真实粒子区域和扫描点云；无数据时继续明确显示空状态 |
| 检测与标定 | 坐标转换模式 | 默认“刚性”，界面序号 `0` | 标定算法真实转换模式；等比／非等比／仿射目前均未接入 |
| 修磨工艺 | 主轴转速 | `0 rpm` | 主轴工艺参数；用户清单只列出字段名称，未提供默认值 |
| 修磨工艺 | 空跑最大速度 X / Y / Z | `15000 / 15000 / 6000 mm/min` | 各轴真实空跑速度限制；当前数值无工艺或设备依据 |
| 修磨工艺 | 加工最大速度 X / Y / Z | `3000 / 3000 / 800 mm/min` | 各轴真实加工速度限制；当前数值无工艺或设备依据 |
| 修磨工艺 | XY 速度规划模式 | 默认“轴比例”，界面序号 `1` | 真实 XY 速度规划 `mode`；模式名称和枚举值均待确认 |
| 修磨工艺 | Z 速度规划模式 | 默认“恒定”，界面序号 `0` | 真实 Z 速度规划 `mode`；模式名称和枚举值均待确认 |
| 修磨工艺 | XY、Z 两组 K1 / K2 / K3 / K4 | 两组均为 `1.50 / 0.85 / 0.50 / 0.20` | 两组真实速度规划系数；当前数值无来源，不是用户提供的工艺默认值 |
| 修磨工艺 | Z 补偿生产／调试模式 | 默认“生产精调”；生产范围 `±2 mm`，调试范围 `±50 mm` | 生产精调 `±2 mm` 有需求描述；模式状态、调试范围和持久化均未接入 |
| 修磨工艺 | 一键抬高 Z | 仅在调试模式可用，点击后界面值 `+20 mm` | `+20 mm` 是 UI 占位动作，没有真实字段或设备动作依据，不下发机床 |
| 修磨工艺 | 铜板剖面图 | 固定绘制的铜板、粒子和虚线 | 纯示意图，不是扫描点云、粒子高度或实际加工位置 |

### 2. 原硬编码坐标点对（已清除）

`CalibrationPlaceholderPage.cpp` 曾通过 `samples[6][6]` 写入以下 6 行示例，现数组和示例行均已删除，表格为 0 行并显示“未采集坐标点对”。以下仅保留历史记录，列顺序为 `Img X/Y/Z`、`Rob X/Y/Z`：

```text
#   Img X   Img Y   Img Z   Rob X   Rob Y   Rob Z
1   101.45   88.20   -0.12  880.20   86.00    0.00
2   883.10   86.06   -0.19  880.20   86.20    0.18
3   883.30  860.40   -1.30  881.50  858.00    0.21
4   118.90  858.16   -1.45  119.00  858.00    0.16
5   500.20  472.40   -0.80  500.20  472.50    0.10
6   500.18  472.55   -0.40  500.00  472.50   20.00
```

这些点从未读取 `std::vector<s_PPts> sListPPtsImage` 或 `std::vector<s_PPts> sListPPtsRobot`，也不是标定文件中的记录。原残差公式和红／绿阈值显示逻辑也已随示例数据移除。后续应接入真实点对、标定算法的误差定义和写回方式；没有数据时保持空表，不得恢复示例点。

### 3. 原硬编码 4×4 转换矩阵（已清除）

`CalibrationPlaceholderPage.cpp` 的 `values[4][4]` 曾固定为以下矩阵，现数组已删除，所有矩阵单元格显示“—”：

```text
 0.999982   0.000841  -0.005981   12.45010
-0.000812   0.999981   0.004950   -4.18230
 0.006041  -0.004942   0.999977    1.24040
 0.000000   0.000000   0.000000    1.000000
```

该矩阵从未由坐标点计算，也没有读取标定算法输出。后续应显示真实 3D 转换矩阵，并明确矩阵方向、单位、是否允许编辑以及结果发送目标；未标定前保持“未标定”和空单元格，不得恢复这份示例矩阵。

### 4. 有用户提供的代码默认值，但仍未实时绑定

下列数值来自用户提供的 `m_sJggyPara` 赋值或说明，因此有代码默认值依据；但当前页面只是把这些值重新写成只读控件的初始值，**仍没有读取结构体，也不能证明它们已经过现场工艺验证，更不能视为设备当前值**。后续接入实时读取前不得解除只读限制。

| 控件 | 当前初始值 | 后续应绑定的字段 |
| --- | --- | --- |
| 刀具直径 | `80 mm` | `m_sJggyPara.fDjDiamiter` |
| 粒子最高点上抬 | `2.0 mm` | `m_sJggyPara.fJgLzUpOffset` |
| 铜板上平面上抬 | `0.0 mm` | `m_sJggyPara.fJgDpUpOffset` |
| Z 路径进给模式 | 默认“线性进给” | `m_sJggyPara.iModeJgPPZStepMode`（`0` 等距、`1` 线性） |
| 固定 Z 向进给 | `2.0 mm` | `m_sJggyPara.fJgPPZStepFix` |
| 线性 Z 进给 K / B | `0.5 / 1.5` | `m_sJggyPara.fJgPPZStepK` / `fJgPPZStepB` |
| 线性 Z 进给 Min / Max | `3 / 10` | `m_sJggyPara.fJgPPZStepMin` / `fJgPPZStepMax` |
| Y 路径进给模式 | 默认“固定进给” | `m_sJggyPara.iModeJgPPYStepMode`（需求说明为 `0` 固定、`2` 按刀盘大小；界面内部按钮序号不是实际枚举） |
| 固定 Y 向进给 | `70 mm` | `m_sJggyPara.fJgPPYStepFix` |
| 刀盘比例系数 K | `0.8` | `m_sJggyPara.fJgPPYStepK` |
| 粒子加工排序 | 默认“局部最近” | `m_sJggyPara.iModeJgPPSort`（`0–5`；界面当前仅设置下拉索引 `4`） |
| 气缸侧安全距离 | `60 mm` | `m_sJggyPara.fQgSafeDis` |
| 完工抬刀高度 | `5 mm` | `m_sJggyPara.fJgTdZUp` |
| 下刀位置偏移 | `50 mm` | `m_sJggyPara.fOffsetCutStartPos` |
| 路径段左 / 右偏移 | `0 / 0 mm` | `m_sJggyPara.fOffsetBothEndsL` / `fOffsetBothEndsR` |
| 路径点整体偏移 X / Y / Z | `0 / 0 / 0 mm` | `m_sJggyPara.fOffsetPtsX` / `fOffsetPtsY` / `fOffsetPtsZ` |

刀盘方向进给仍根据上述控件值做界面换算。原“安全距离 − 刀具直径 / 2”安全余量公式已删除；页面只保留用户原注释中的定义：`fQgSafeDis` 是加工中心到气缸的距离，未考虑刀盘直径。后续如需显示安全余量，必须先由后端确认公式，不能直接作为安全验收依据。

### 5. 后续接入要求

- 沿用现有设置页的保存、撤销、权限和审计，不要在占位页里另写一套写文件逻辑。
- 接入真实数据后，逐项删除本清单中的占位说明，并记录真实字段、单位、来源 topic／文件和读写方向。
- 没有真实数据时显示“未采集／未标定／无数据”，不要用示例值伪装成有效结果。
- 原先已经接上的坐标补偿（`backend_params.ini` 的 `OffsetX/Y/Z`，以及 `plc/coordinate_offset`）和现场示教仍在 `ProcessSettingsPage` / `BackendParamsDialog`；如果合并到“修磨工艺”，必须复用原有存储与下发链路，不能只把控件值写进新界面。

## 注意事项

### 启动 / 暂停 / 停止（已发 topic，待后端执行）

左侧导航的**开始、暂停、停止**在非演示模式下发布 `milling/command`。`command` 只有 `1` 启动、`2` 暂停、`3` 停止。暂停后再次开始仍发 `1`，由后端决定是新开始还是从暂停处继续。

按钮文字跟 `plc/feedback` 的状态字走，不在点击后自行切换。状态字为 `4`、`12`、`30`、`40`、`50`、`60` 时显示「暂停」，其余显示「开始」。PLC 触发拍照并把状态字变为 `40` 时，按钮会变成「暂停」。Ctrl+T 演示模式不发布该 topic。

正式联调前请与后端确认已订阅 `milling/command`。字段见 `docs/前后端算法对接说明.md`。

### 粒子信息与「粒子三维点云」（占位实现，待算法 topic）

右侧「粒子三维点云」的目标是：**按算法给出的粒子信息，从扫描点云里取出该粒子所在区域的数据并三维显示**。

当前状态：

- 算法侧**已经实现了粒子识别/定位**，但**还没有通过 ROS topic 把粒子数据发给本程序**；本程序目前只收到 `MillingPaths`（铣削路径，含 `uStart/vStart/uEnd/vEnd` 像素坐标）。
- 因此本程序**只能用路径近似代表粒子**，属于占位实现：
  - 2D 图命中：先按路径线 6 px 精确命中，未命中时再用 26 px 容差找最近的路径（`ImageViewFrame::hitTestPathInRange`，常量 `kParticleClickThresholdPx`）。
  - 3D 裁剪：取选中路径的 `u/v` 包围盒，四周留白 `max(16, span/2)` 像素，从整板 `rangeFloat32` 里切出子区域显示（`ParticleDetailPanel.cpp` 的 `cropParticleCloud`）。
- 这意味着：**粒子轮廓、粒子中心、粒子编号、单粒子高度/等级目前都不是真实数据**，只是路径附近的一块点云。多条路径属于同一粒子时，看起来会是「按路径分块」，而不是「按粒子分块」。

待算法 topic 接入后应替换的部分：

1. 新增或扩展现有消息（例如 `mz_interfaces/msg/ParticleInfo`，随 `MillingPaths` 一起下发），字段建议：`particle_id`、`bbox`（`u_min/v_min/u_max/v_max` 像素坐标）或 `center_u/center_v + radius`，以及可选的高度、等级。
2. `Ros2Bridge` 增加订阅，转换成 VM 后交给 `MainWindow` → `ParticleDetailPanel`。
3. 2D 图按粒子 bbox 绘制轮廓，命中测试改为「点在粒子 bbox 内」，不再依赖路径线容差。
4. `cropParticleCloud(...)` 的入参由 `MillingPathVM` 改为粒子 bbox，同时把粒子编号显示到卡片标题/工具提示里。

在此之前，界面上看到的粒子三维点云请按「路径附近区域点云」理解，不要作为粒子检测结果的验收依据。

### Ctrl+T / Ctrl+Shift+T（调试阶段）

生产构建中仍保留调试快捷键：

- Ctrl+T：切换演示/假数据模式，可隐藏实时扫描画面、注入假 PLC 状态，并使用演示任务 `9001`。
- Ctrl+Shift+T：在假数据模式下模拟任务完成。
- `MainWindow::persistTaskRecord` 已明确跳过演示模式和任务 `9001`，**演示任务不会写入 SQLite**。

正式发布前必须移除这些快捷键，或用编译宏/配置开关关闭，避免误触覆盖真实界面状态或造成误判。

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

项目使用以下脚本：

- **build.bat** - 主工程统一配置、编译和部署，不执行全量清理。
- **.deps/build_interfaces.bat** - 仅在本地 ROS2 接口变化或接口安装目录缺失时重建 `mz_interfaces`。
- **config/run.bat** - 设置运行环境并从部署目录启动程序。
- **config/start_all.bat** - 启动相机/算法/后台节点。
- **config/stop_all.bat** - 停止所有节点。

**⚠️ 禁止创建临时脚本**：如必须生成临时脚本（如调试），使用后立即删除。

## 报表统计页：字段与数据来源

正常模式下，报表只使用 SQLite 已有的 `InspectionRecord` 数据。时间范围、检测铜板数、有粒子铜板数、修磨完成数、粒子总数、完成率、平均修磨节拍和任务级最大粒子高度均来自现有记录，**不会自动补充演示数据**。

只有 Ctrl+T 演示模式开启且当前查询结果少于 36 条时，报表才补充内存中的演示数据。演示数据不会写入 SQLite，状态栏、班次汇总和 PDF 导出会标记“含演示数据，仅供界面预览”；演示模式下真实记录达到 36 条时，不再补充演示记录。

班次和板号已经接到检测记录上。粒子高度占比仍然缺每个粒子自己的高度。

- **班次**：默认白班、晚班，日报按这个名单出列。名单在「设置 → 账号管理 → 设置班次」里添加、改名、删除；清空后仍回到白班、晚班，避免汇总表没有班次列。铜板粒子分布卡头的班次框只选择当前班次，选中的名称写入之后新建的 `InspectionRecord.Shift`。旧记录保持原值，没填过的留空，日报里只进合计、不进某一列。报表筛选和每日汇总的列都跟这份名单走。
- **铜板编号**：在「铜板粒子分布」卡头填写，不弹窗提醒。只允许字母和数字；留空显示「未编号」，填了就按填写内容显示，并写入 `InspectionRecord.PlateNo`。报表里的板号框按这个字段筛选。
- **粒子高度占比**：缺的是每个粒子的高度。`ParticleDetection` 表有 `Height` 列，但当前没有写入。任务上只有 `InspectionRecord.MaxParticleHeight`。因此正常模式下这张图不拿任务最大高度冒充分颗粒占比，只提示还没有每个粒子的高度。演示模式里的圆环仍按任务最大高度分档，图注会标明这一点。
- **统一任务状态**：仍未接入。正常模式下不根据其他字段推测状态，状态分布图保持空白；只有演示模式才填演示分布。
- **Excel 导出**：已接入 `third_party/QXlsx`，导出当前查询的每日汇总和检测记录。PDF 一并导出。演示数据仍只留在内存里，不会写入 SQLite。
