#ifndef PROCESSCRAFTPLACEHOLDERPAGE_H
#define PROCESSCRAFTPLACEHOLDERPAGE_H

#include <QWidget>

// 修磨工艺的占位界面。速度、进给模式和安全余量只在界面内联动，不落盘。
class ProcessCraftPlaceholderPage : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessCraftPlaceholderPage(QWidget *parent = nullptr);
};

#endif // PROCESSCRAFTPLACEHOLDERPAGE_H
