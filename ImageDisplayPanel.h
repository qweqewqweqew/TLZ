#ifndef IMAGEDISPLAYPANEL_H
#define IMAGEDISPLAYPANEL_H

#include "MillingPathVM.h"

#include <QFrame>
#include <QImage>
#include <QVector>

class ElaPushButton;
class ImageViewFrame;

class ImageDisplayPanel : public QFrame
{
    Q_OBJECT

public:
    explicit ImageDisplayPanel(QWidget *parent = nullptr);

    // 更新一帧图像。任一入参可以为空 QImage 表示"这一路没有数据"。
    // frameId / timestampNs 用于覆盖层文本，传 0 表示不显示。
    void updateScanFrame(const QImage &range,
                         const QImage &intensity,
                         quint64 frameId,
                         quint64 timestampNs);

    // 设置/清除叠加显示的打磨路径（来自 MillingPaths msg）。
    // calibrationApplied=false 时，叠加层会半透明 + 角标提示"未标定"。
    void setMillingPaths(const QVector<MillingPathVM> &paths, bool calibrationApplied);
    void clearMillingPaths();

private:
    enum class ViewMode { Range2D, Intensity };
    void setViewMode(ViewMode mode);
    void refreshView();

    ImageViewFrame *m_imageArea{nullptr};
    ElaPushButton *m_view2dButton{nullptr};
    ElaPushButton *m_view3dButton{nullptr};

    QImage m_rangeImage;      // Range，Mono16 → Grayscale8（拉伸后）
    QImage m_intensityImage;  // Intensity，Mono8
    quint64 m_lastFrameId{0};
    quint64 m_lastTimestampNs{0};
    ViewMode m_mode{ViewMode::Range2D};
};

#endif // IMAGEDISPLAYPANEL_H
