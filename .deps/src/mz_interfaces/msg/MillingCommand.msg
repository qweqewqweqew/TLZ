# 前端发给后端的修磨控制。Topic: milling/command
# 暂停后再次启动仍发 START。后端按自己的当前状态决定新开始还是从暂停处继续。
uint8 START=1
uint8 PAUSE=2
uint8 STOP=3

uint8 command
uint64 task_id
