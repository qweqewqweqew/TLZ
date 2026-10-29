# 共享内存对象名称
string   shm_name
bool is_cam_connect
uint64   offset

# task_id 标识一次完整检测任务；同一任务的初检和复检使用不同的 frame_id
# frame_id=-1 表示取图失败需重拍
uint64   frame_id
uint64 task_id
uint64   timestamp_ns

uint32   width
uint32   height
uint32   stride
# OpenCV cv::Mat::type(): scan_range=5 (CV_32FC1), scan_intensity=0 (CV_8UC1)
uint32   pixel_format
uint64   data_size
