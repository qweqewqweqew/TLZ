#ifndef MILLINGPATHVM_H
#define MILLINGPATHVM_H

#include <QMetaType>
#include <QVector>

// 前端使用的打磨路径视图对象，屏蔽 ROS 类型。
// 字段与 mz_interfaces/msg/MillingPath.msg 一一对应。
struct MillingPathVM
{
    int uStart{0};
    int vStart{0};
    int uEnd{0};
    int vEnd{0};
    float rSpeed{0.0f};
    float xSpeed{0.0f};
    float ySpeed{0.0f};
    bool doCut{false};
};

// 算法给出的一颗粒子。轮廓与扫描图像素坐标一致。
struct ParticleRegionVM
{
    int index{0};
    float height{0.0f};
    int uMin{0};
    int vMin{0};
    int uMax{0};
    int vMax{0};
    QVector<int> contourU;
    QVector<int> contourV;
};

Q_DECLARE_METATYPE(MillingPathVM)
Q_DECLARE_METATYPE(QVector<MillingPathVM>)
Q_DECLARE_METATYPE(ParticleRegionVM)
Q_DECLARE_METATYPE(QVector<ParticleRegionVM>)

#endif // MILLINGPATHVM_H
