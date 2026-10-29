# 算法识别后的全部打磨路径（后端一次性发布，供前端展示）
uint64 task_id
uint16 path_total
uint16 max_particle_height
# 算法 JSON particle_board_grade，任务级，原样转发给前端
string particle_board_grade
# 算法 JSON drawimage_path，任务级，原样转发给前端
string drawimage_path
MillingPath[] paths
# 算法 JSON particles，每颗粒子的高度和像素轮廓
ParticleRegion[] particles
