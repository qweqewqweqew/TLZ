#ifndef HEIGHTMAP3DFRAME_H
#define HEIGHTMAP3DFRAME_H

#include "vision/PointCloudFrame.h"

#include <QWidget>

class HeightMap3DFrame : public QWidget
{
public:
    explicit HeightMap3DFrame(QWidget *parent = nullptr);

    void setPointCloudFrame(const PointCloudFrame &frame);
    void clearPointCloud();

private:
    class Window;
    Window *m_window{nullptr};
};

#endif // HEIGHTMAP3DFRAME_H
