#ifndef IMAGEVIEWFRAME_H
#define IMAGEVIEWFRAME_H

#include "MillingPathVM.h"

#include <QFrame>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QVector>

class ImageViewFrame : public QFrame
{
    Q_OBJECT

public:
    explicit ImageViewFrame(QWidget *parent = nullptr);

    // 设置要显示的图像。空 QImage 会回到"等待图像"占位状态。
    void setImage(const QImage &image);
    void clearImage();
    bool hasImage() const { return !m_image.isNull(); }

    // 顶部覆盖层文本，比如 "frame=123 3200x3000"，为空则不绘制。
    void setOverlayText(const QString &text);

    // 叠加打磨路径。paths 为空即清空。
    // calibrationApplied=false 时，路径整体半透明 + 右下角"未标定"角标。
    void setMillingPaths(const QVector<MillingPathVM> &paths, bool calibrationApplied);
    void clearMillingPaths();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // 把图像原坐标 (u, v) 映射到当前 widget 坐标。
    // 前提：m_lastImageRect 已经在 paintEvent 中被更新过（即已渲染过一次图）。
    QPointF mapImageToWidget(int u, int v) const;

    // 找到鼠标位置附近（阈值内）最近的一条路径索引，找不到返回 -1。
    int hitTestPath(const QPointF &widgetPos) const;

    // 生成 tooltip 文本
    QString tooltipForPath(const MillingPathVM &p, int indexOneBased) const;

private:
    QImage m_image;
    QString m_overlayText;

    QVector<MillingPathVM> m_paths;
    bool m_calibrationApplied{true};

    // 记录最近一次绘制时图像在 widget 内部的目标矩形与原图尺寸，
    // 用于在 mouseMoveEvent 中做像素坐标反查。
    QRect m_lastImageRect;
    QSize m_lastImageSize;

    int m_hoveredPathIndex{-1};
    int m_tooltipPathIndex{-1};
};

#endif // IMAGEVIEWFRAME_H
