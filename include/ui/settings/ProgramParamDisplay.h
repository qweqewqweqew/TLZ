#ifndef PROGRAMPARAMDISPLAY_H
#define PROGRAMPARAMDISPLAY_H

class QWidget;

// 把算法运行目录里的 program_param.json 填进只读参数行。不写回文件。
void attachProgramParamDisplay(QWidget *page);

#endif // PROGRAMPARAMDISPLAY_H
