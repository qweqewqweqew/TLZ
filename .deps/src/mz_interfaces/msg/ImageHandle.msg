# 相机端发布到后端的检测图像句柄。
# 指向共享内存中的一个快照，用于单帧检测或最新帧缓存触发的检测。

# 共享内存对象名称，例如：Local\hik_camera_demo_shm
string shm_name

# 共享内存区域内的字节偏移。
# 当前演示中每次映射仅包含一个快照，因此该值为 0。
uint64 offset

# 快照大小（字节）。
uint64 size

# 图像元数据。
uint32 width
uint32 height
uint32 stride
uint32 pixel_format

# 帧标识与采集时间。
uint64 frame_id

# 相机设备时间戳（纳秒），基于 GigE Vision 规范的 1 GHz tick 时钟。
uint64 timestamp_ns

# 相机标识，如序列号、用户自定义名称、型号或 IP 地址。
string camera_id

# 保留字段，用于未来的任务路由。当前检测流程中该值为 0。
uint64 task_id

# 保留字段，用于未来的工站路由。当前检测流程中该值为空。
string station_id