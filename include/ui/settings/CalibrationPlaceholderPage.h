#ifndef CALIBRATIONPLACEHOLDERPAGE_H
#define CALIBRATIONPLACEHOLDERPAGE_H

#include <QWidget>

// 检测与标定的占位界面。采集参数、对点表和转换矩阵只在界面内联动，不落盘。
class CalibrationPlaceholderPage : public QWidget
{
    Q_OBJECT

public:
    explicit CalibrationPlaceholderPage(QWidget *parent = nullptr);
};

#endif // CALIBRATIONPLACEHOLDERPAGE_H
