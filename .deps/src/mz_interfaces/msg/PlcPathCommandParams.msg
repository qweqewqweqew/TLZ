# PC 写区路径/工艺参数（DB 8.0~59.0），与 plc_command_data.hpp 一致
float32 x
float32 y
float32 z
float32 feed_speed
float32 feed_amount
float32 spindle_speed
uint16 plunge_count
uint16 plunge_amount
int16 feed_direction  # 10=X+ 20=X- 30=Y+ 40=Y-
int16 int_spare2
int16 int_spare3
int16 int_spare4
float32 real_spare1
float32 real_spare2
float32 real_spare3
float32 real_spare4
