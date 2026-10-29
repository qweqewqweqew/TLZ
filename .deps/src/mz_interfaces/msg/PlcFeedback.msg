# PLC 反馈区原始数据（DB 60.0 起），由 PC S7 主动读取
uint16 status_word
uint16 completed_path
uint16 current_path
uint16 fault_code  # DATA_INTEGRATE[PLC_FAULT_CODE]: 预留，枚举待定
PlcPathParams path_params
