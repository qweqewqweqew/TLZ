# 与 ScanResult 按 frame_id / task_id 配对。不修改旧 ScanResult 布局。
# RAN_CAL 的 float 值直接作为 Z（毫米）。
uint64   frame_id
uint64   task_id

bool     has_metric_geometry
float64  x_offset_mm
float64  y_offset_mm
float64  x_scale_mm
float64  y_scale_mm
float32  missing_value

# encoder_values 与图像行一一对应；非空时优先用
# Y = y_offset_mm + (encoder_values[row] - encoder_values[0]) * encoder_resolution_mm。
float64  encoder_resolution_mm
int32[]  encoder_values
