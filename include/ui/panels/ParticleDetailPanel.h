#ifndef PARTICLEDETAILPANEL_H
#define PARTICLEDETAILPANEL_H

#include "bridge/MillingPathVM.h"
#include "vision/PointCloudFrame.h"

#include <QFrame>
#include <QVector>

class HeightMap3DFrame;
class QVBoxLayout;
class QWidget;

// 右侧点云区：显示「2D 图里选中的单个粒子」的三维点云，右侧竖排修磨数据。
class ParticleDetailPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ParticleDetailPanel(QWidget *parent = nullptr);

    void setPointCloudFrame(const PointCloudFrame &frame);
    void setMillingPaths(const QVector<MillingPathVM> &paths);
    void setParticleRegions(const QVector<ParticleRegionVM> &particles);
    void selectParticle(int particleIndex);
    // 任务级信息：算法给出的最大粒子高度（mm，负数=无）与铜板等级
    void setTaskInfo(int maxParticleHeight, const QString &grade);
    // 0-based；负数取消手动选择，回到跟随当前路径
    void selectPath(int pathIndex);
    // PLC 进度：currentPath 为 1-based，0 表示没有
    void setPathProgress(int completedPath, int currentPath);
    void setTaskProgress(float progressPercent);
    void setSideContent(QWidget *content);
    void clear();

private:
    void refresh();
    // 从整板点云里裁出当前选中粒子附近的区域，喂给 3D 视图。
    void refreshParticleCloud();

    HeightMap3DFrame *m_heightMap{nullptr};
    QWidget *m_preview{nullptr};
    QWidget *m_sideHost{nullptr};
    QVBoxLayout *m_sideLayout{nullptr};

    PointCloudFrame m_frame;
    QVector<MillingPathVM> m_paths;
    QVector<ParticleRegionVM> m_particles;
    int m_selectedParticle{-1};
    int m_selected{-1};
    int m_completed{0};
    int m_current{0};
    int m_maxParticleHeight{-1};
    QString m_grade;
};

#endif // PARTICLEDETAILPANEL_H
