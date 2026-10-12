#ifndef IMAGEDISPLAYPANEL_H
#define IMAGEDISPLAYPANEL_H

#include "bridge/MillingPathVM.h"
#include "vision/PointCloudFrame.h"

#include <QFrame>
#include <QImage>
#include <QVector>
#include <QtGui/qwindowdefs.h>

class ElaPushButton;
class HeightMap3DFrame;
class HikLiveView;
class ImageViewFrame;
class QStackedWidget;

class ImageDisplayPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ImageDisplayPanel(QWidget *parent = nullptr);

    // 更新一帧图像。任一入参可以为空 QImage 表示"这一路没有数据"。
    // frameId / timestampNs 用于覆盖层文本，传 0 表示不显示。
    void updateScanFrame(const PointCloudFrame &frame);

    // 调试演示入口：切换到三维高度视图或恢复二维视图。
    void showHeight3D();
    void show2D();
    void setLiveCameraStatus(bool connected, const QString &message);

    // 设置/清除叠加显示的打磨路径（来自 MillingPaths msg）。
    void setMillingPaths(const QVector<MillingPathVM> &paths);
    void clearMillingPaths();

    // Forward PLC tool pose / path progress onto the 2D ImageViewFrame.
    void setToolPose(double xMm, double yMm, double zMm, bool valid);
    void setPathProgress(int completedPath, int currentPath);
    void clearToolPose();

    // 算法 JSON drawimage_path：读取标注图，供「算法图像」视图显示。
    void setAlgorithmImagePath(const QString &path);
    void clearAlgorithmImage();

    // 供日志使用：当前图像区显示模式。
    QString currentViewModeName() const;

signals:
    void liveViewRequested(bool active, WId targetWindow);
    // 2D 图上单击路径线：0-based。
    void millingPathClicked(int pathIndex);

private:
    enum class ViewMode { View2D, Height3D, LiveVideo, AlgorithmImage };
    void setViewMode(ViewMode mode);
    void refreshView();
    void applyViewButtonStyles();

    ImageViewFrame *m_imageArea{nullptr};
    ImageViewFrame *m_algorithmImageArea{nullptr};
    HeightMap3DFrame *m_heightMapArea{nullptr};
    HikLiveView *m_liveViewArea{nullptr};
    QStackedWidget *m_viewStack{nullptr};
    ElaPushButton *m_view2dButton{nullptr};
    ElaPushButton *m_view3dButton{nullptr};
    ElaPushButton *m_liveViewButton{nullptr};
    ElaPushButton *m_algorithmImageButton{nullptr};

    QImage m_rangeImage;      // Range，Mono16 → Grayscale8（拉伸后）
    QImage m_intensityImage;  // Intensity，Mono8
    QImage m_algorithmImage;
    QString m_algorithmImagePath;
    PointCloudFrame m_pointCloudFrame;
    quint64 m_lastFrameId{0};
    quint64 m_lastTimestampNs{0};
    ViewMode m_mode{ViewMode::View2D};
};

#endif // IMAGEDISPLAYPANEL_H
