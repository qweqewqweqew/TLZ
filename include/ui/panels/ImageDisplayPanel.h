#ifndef IMAGEDISPLAYPANEL_H
#define IMAGEDISPLAYPANEL_H

#include "bridge/MillingPathVM.h"
#include "vision/PointCloudFrame.h"

#include <QFrame>
#include <QImage>
#include <QVector>
#include <QtGui/qwindowdefs.h>

class ElaComboBox;
class ElaPushButton;
class HeightMap3DFrame;
class HikLiveView;
class ImageViewFrame;
class QLabel;
class QLineEdit;
class QShowEvent;
class QStackedWidget;
class QToolButton;

// 「铜板粒子分布」卡：2D 强度图 + 路径叠加，可切到海康实时画面 / 算法标注图 / 整板三维点云。
// 单个粒子的三维点云在右侧「选中粒子」卡里单独显示。
class ImageDisplayPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ImageDisplayPanel(QWidget *parent = nullptr);

    // 更新一帧图像。任一入参可以为空 QImage 表示"这一路没有数据"。
    void updateScanFrame(const PointCloudFrame &frame);
    void show2D();
    void setLiveCameraStatus(bool connected, const QString &message);
    WId liveVideoWindowId();
    // 从左侧视图栈取出实时画面，放到右侧卡片。句柄仍由 liveVideoWindowId 提供。
    QWidget *detachLiveView();

    // 设置/清除叠加显示的打磨路径（来自 MillingPaths msg）。
    void setMillingPaths(const QVector<MillingPathVM> &paths);
    void clearMillingPaths();

    // Forward PLC tool pose / path progress onto the 2D ImageViewFrame.
    void setToolPose(double xMm, double yMm, double zMm, bool valid,
                     bool leaveGrindTrace = false);
    void clearGrindLayer();
    void setPathProgress(int completedPath, int currentPath);
    void clearToolPose();

    // 算法 JSON drawimage_path：读取标注图，供「算法图像」视图显示。
    void setAlgorithmImagePath(const QString &path);
    void clearAlgorithmImage();

    // 卡头板号、当前班次与页脚任务摘要。板号为空时输入框显示「未编号」。
    void setTaskId(quint64 taskId);
    void setPlateNumber(const QString &plateNo);
    QString plateNumber() const;
    void reloadShifts();
    void setProgressSummary(int completedPath, int pathTotal, float progressPercent);
    void clearTask();

    // 供日志使用：当前图像区显示模式。
    QString currentViewModeName() const;

signals:
    void liveViewRequested(bool active, WId targetWindow);
    void livePreviewExpandRequested();
    // 2D 图上单击路径线：0-based。
    void millingPathClicked(int pathIndex);
    // 用户离开板号输入框。空字符串表示未编号。
    void plateNumberEdited(const QString &plateNo);

protected:
    void showEvent(QShowEvent *event) override;

private:
    enum class ViewMode { View2D, LiveVideo, AlgorithmImage, PointCloud3D };
    void setViewMode(ViewMode mode);
    void refreshView();
    void applyViewButtonStyles();
    void refreshPathSummary();
    QToolButton *makeToolButton(const QString &iconResource, const QString &tooltip, QWidget *parent);

    ImageViewFrame *m_imageArea{nullptr};
    ImageViewFrame *m_algorithmImageArea{nullptr};
    HeightMap3DFrame *m_pointCloudArea{nullptr};
    HikLiveView *m_liveViewArea{nullptr};
    QStackedWidget *m_viewStack{nullptr};
    ElaPushButton *m_view2dButton{nullptr};
    ElaPushButton *m_liveViewButton{nullptr};
    ElaPushButton *m_algorithmImageButton{nullptr};
    ElaPushButton *m_pointCloudButton{nullptr};
    QLineEdit *m_plateEdit{nullptr};
    ElaComboBox *m_shiftCombo{nullptr};

    QImage m_rangeImage;      // Range，Mono16 → Grayscale8（拉伸后）
    QImage m_intensityImage;  // Intensity，Mono8
    QImage m_algorithmImage;
    QString m_algorithmImagePath;
    PointCloudFrame m_pointCloudFrame;
    QVector<MillingPathVM> m_paths;
    quint64 m_lastFrameId{0};
    quint64 m_lastTimestampNs{0};
    quint64 m_taskId{0};
    ViewMode m_mode{ViewMode::View2D};
};

#endif // IMAGEDISPLAYPANEL_H
