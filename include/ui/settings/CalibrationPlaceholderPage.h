#ifndef CALIBRATIONPLACEHOLDERPAGE_H
#define CALIBRATIONPLACEHOLDERPAGE_H

#include <QWidget>

struct PointCloudFrame;
class HeightMap3DFrame;
class QLabel;

// 检测与标定。点对和矩阵只读配置文件；左侧点云只显示 SICK 扫图，不写回。
class CalibrationPlaceholderPage : public QWidget
{
    Q_OBJECT

public:
    explicit CalibrationPlaceholderPage(QWidget *parent = nullptr);

    void setScanFrame(const PointCloudFrame &frame);

private:
    HeightMap3DFrame *m_cloud{nullptr};
    QLabel *m_cloudStatus{nullptr};
};

#endif // CALIBRATIONPLACEHOLDERPAGE_H
