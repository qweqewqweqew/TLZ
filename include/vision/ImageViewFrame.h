#ifndef IMAGEVIEWFRAME_H
#define IMAGEVIEWFRAME_H

#include "bridge/MillingPathVM.h"
#include "vision/PointCloudFrame.h"

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

class DynamicLayer;
class QMouseEvent;
class QPainter;
class QResizeEvent;
class QTimer;
class QWheelEvent;

class ImageViewFrame : public QFrame
{
    Q_OBJECT
    friend class DynamicLayer;

public:
    explicit ImageViewFrame(QWidget *parent = nullptr);

    // 设置要显示的图像。空 QImage 会回到"等待图像"占位状态。
    void setImage(const QImage &image);
    void clearImage();
    bool hasImage() const { return !m_image.isNull(); }

    // 顶部覆盖层文本，比如 "frame=123 3200x3000"，为空则不绘制。
    void setOverlayText(const QString &text);

    // 叠加打磨路径。paths 为空即清空。
    void setMillingPaths(const QVector<MillingPathVM> &paths);
    void clearMillingPaths();
    void setParticleRegions(const QVector<ParticleRegionVM> &particles);
    void setSelectedParticle(int particleIndex);
    void setMetricFrame(const PointCloudFrame &frame);

    // PLC tool pose in millimetres (same metric space as hover readout).
    // valid=false clears the marker so no ghost point remains.
    void setToolPose(double xMm, double yMm, double zMm, bool valid,
                     bool leaveGrindTrace = false);
    void clearGrindLayer();
    // PLC path indices: currentPath is 1-based (0 = none); completedPath is count.
    void setPathProgress(int completedPath, int currentPath);
    void clearToolPose();

    // 工具栏缩放：以当前视图中心为锚点；fitToView 回到整图适配。
    void zoomIn();
    void zoomOut();
    void fitToView();

signals:
    // 单击叠加路径线：0-based 下标。
    void millingPathClicked(int pathIndex);
    // 单击粒子轮廓：0-based 下标。
    void particleClicked(int particleIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF imageViewportRect() const;
    QSizeF fittedImageSize() const;
    QRectF currentImageRect() const;
    void resetView();
    void applyZoom(double newZoom, const QPointF &anchorHint);
    void clampPanOffset();
    QPointF mapImageToWidget(int u, int v) const;
    QPoint mapWidgetToImage(const QPointF &widgetPos) const;
    void updateHoverCoordinate(const QPointF &widgetPos);
    QRect hoverBoxRect(const QPointF &anchor, const QString &text, const QRect &area) const;
    QString coordinateTextAt(int column, int row) const;
    int hitTestPath(const QPointF &widgetPos) const;
    int hitTestParticle(const QPointF &widgetPos) const;
    void drawParticleContours(QPainter &painter, const QRectF &dstRect, const QRect &clip) const;
    // 同上，但使用自定义命中半径（用于"点击粒子附近"的宽容命中）。
    int hitTestPathInRange(const QPointF &widgetPos, double thresholdPx) const;
    QString tooltipForPath(const MillingPathVM &path, int indexOneBased) const;
    bool mapMetricToImage(double xMm, double yMm, double *uOut, double *vOut) const;
    void recomputeToolPoseMapping();
    void requestPoseUpdate();
    void strokeGrindToImageUv(double u, double v);
    void breakGrindStroke();
    void ensureBasePixmap(const QSize &pixelSize);
    void noteInteraction();
    void drawGrindPaths(QPainter &painter, const QRectF &dstRect, const QRect &clip) const;
    void paintDynamic(QPainter &painter);
    void refreshDynamic();
    void invalidateDynamic(const QRect &rect);
    void ensureViewCache();
    QRect toolPoseDirtyRect() const;
    QRect pathSegmentRect(int index) const;

private:
    QImage m_image;
    QString m_overlayText;
    PointCloudFrame m_metricFrame;
    QVector<MillingPathVM> m_paths;
    QVector<ParticleRegionVM> m_particles;
    int m_selectedParticle{-1};
    QPointF m_hoverWidgetPos;
    QString m_hoverCoordinateText;
    bool m_hoverCoordinateValid{false};
    QRectF m_lastImageRect;
    QSize m_lastImageSize;
    int m_hoveredPathIndex{-1};
    int m_tooltipPathIndex{-1};

    double m_zoomFactor{1.0};
    QPointF m_panOffset;
    QPoint m_dragStartPosition;
    QPoint m_lastDragPosition;
    bool m_dragging{false};
    bool m_dragMoved{false};

    bool m_toolPoseValid{false};
    bool m_toolPoseMapped{false};
    double m_toolPoseXMm{0.0};
    double m_toolPoseYMm{0.0};
    double m_toolPoseZMm{0.0};
    double m_toolPoseU{0.0};
    double m_toolPoseV{0.0};
    int m_completedPathCount{0};
    int m_currentPathOneBased{0};
    qint64 m_lastPoseUpdateMs{0};
    bool m_poseUpdatePending{false};
    static constexpr int kToolTrailCapacity = 12;
    QPointF m_toolTrailUvRing[kToolTrailCapacity];
    int m_toolTrailCount{0};
    int m_toolTrailNext{0}; // next write index in ring
    QVector<QPointF> m_toolTrail; // widget-space scratch for paint
    // 适配视口的底图。缩放和平移只裁这块，不再每次重采样整幅扫描图。
    QPixmap m_basePixmap;
    QPixmap m_viewCache;
    bool m_viewCacheDirty{true};
    QRect m_lastDynamicDirty;
    qint64 m_baseImageKey{0};
    QSize m_basePixelSize;
    QTimer *m_interactionTimer{nullptr};
    bool m_interacting{false};
    bool m_leaveGrindTrace{false};
    bool m_hasActiveGrindPath{false};
    QPainterPath m_activeGrindPath; // current doCut segment
    QVector<QPainterPath> m_grindPaths; // finished doCut segments
    DynamicLayer *m_dynamic{nullptr};
};

#endif // IMAGEVIEWFRAME_H
