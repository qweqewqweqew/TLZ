#include "ImageViewFrame.h"

#include <QEvent>
#include <QFont>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QRect>
#include <QToolTip>

namespace {

// 路径线宽 & hit test 阈值
constexpr int kCutLineWidth = 2;
constexpr int kMoveLineWidth = 1;
constexpr double kHitThresholdPx = 6.0;  // 鼠标到线段距离（widget 像素）小于此值即命中

const QColor kCutColor("#00E5FF");        // 切削段颜色（与主题 accent 一致）
const QColor kMoveColor("#5A6A78");       // 空走段颜色
const QColor kCutHoverColor("#FFFFFF");   // hover 高亮
const QColor kMoveHoverColor("#B8C8D8");

// 点到线段的最短距离（返回 widget 像素）
double distancePointToSegment(const QPointF &p,
                              const QPointF &a,
                              const QPointF &b)
{
    const QPointF ab = b - a;
    const double ab2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (ab2 <= 0.0) {
        const QPointF d = p - a;
        return std::sqrt(d.x() * d.x() + d.y() * d.y());
    }
    const QPointF ap = p - a;
    double t = (ap.x() * ab.x() + ap.y() * ab.y()) / ab2;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    const QPointF proj(a.x() + t * ab.x(), a.y() + t * ab.y());
    const QPointF d = p - proj;
    return std::sqrt(d.x() * d.x() + d.y() * d.y());
}

} // namespace

ImageViewFrame::ImageViewFrame(QWidget *parent)
    : QFrame(parent)
{
    setMouseTracking(true);
}

void ImageViewFrame::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void ImageViewFrame::clearImage()
{
    m_image = QImage();
    update();
}

void ImageViewFrame::setOverlayText(const QString &text)
{
    if (m_overlayText == text) {
        return;
    }
    m_overlayText = text;
    update();
}

void ImageViewFrame::setMillingPaths(const QVector<MillingPathVM> &paths,
                                     bool calibrationApplied)
{
    m_paths = paths;
    m_calibrationApplied = calibrationApplied;
    m_hoveredPathIndex = -1;
    m_tooltipPathIndex = -1;
    QToolTip::hideText();
    update();
}

void ImageViewFrame::clearMillingPaths()
{
    m_paths.clear();
    m_calibrationApplied = true;
    m_hoveredPathIndex = -1;
    m_tooltipPathIndex = -1;
    QToolTip::hideText();
    update();
}

QPointF ImageViewFrame::mapImageToWidget(int u, int v) const
{
    if (m_lastImageRect.isEmpty() || m_lastImageSize.isEmpty()) {
        return QPointF();
    }
    const double sx = double(m_lastImageRect.width()) / double(m_lastImageSize.width());
    const double sy = double(m_lastImageRect.height()) / double(m_lastImageSize.height());
    return QPointF(m_lastImageRect.x() + u * sx,
                   m_lastImageRect.y() + v * sy);
}

int ImageViewFrame::hitTestPath(const QPointF &widgetPos) const
{
    if (m_paths.isEmpty() || m_lastImageRect.isEmpty() || m_lastImageSize.isEmpty()) {
        return -1;
    }

    double bestDist = kHitThresholdPx;
    int bestIndex = -1;
    for (int i = 0; i < m_paths.size(); ++i) {
        const auto &p = m_paths[i];
        const QPointF a = mapImageToWidget(p.uStart, p.vStart);
        const QPointF b = mapImageToWidget(p.uEnd, p.vEnd);
        const double d = distancePointToSegment(widgetPos, a, b);
        if (d < bestDist) {
            bestDist = d;
            bestIndex = i;
        }
    }
    return bestIndex;
}

QString ImageViewFrame::tooltipForPath(const MillingPathVM &p, int indexOneBased) const
{
    return QStringLiteral(
        "路径 #%1  %2\n"
        "起点: (%3, %4)\n"
        "终点: (%5, %6)\n"
        "主轴速度: %7 rpm\n"
        "X 速度: %8\n"
        "Y 速度: %9")
        .arg(indexOneBased)
        .arg(p.doCut ? QStringLiteral("切削") : QStringLiteral("空走"))
        .arg(p.uStart).arg(p.vStart)
        .arg(p.uEnd).arg(p.vEnd)
        .arg(p.rSpeed, 0, 'f', 0)
        .arg(p.xSpeed, 0, 'f', 2)
        .arg(p.ySpeed, 0, 'f', 2);
}

void ImageViewFrame::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect area = rect().adjusted(1, 1, -2, -2);
    painter.fillRect(area, QColor("#0B1117"));

    if (m_image.isNull()) {
        // 占位：网格 + 中心十字 + 文本
        QPen gridPen(QColor(60, 80, 96, 70));
        gridPen.setWidth(1);
        painter.setPen(gridPen);

        constexpr int gridSize = 48;
        for (int x = area.left() + gridSize; x < area.right(); x += gridSize) {
            painter.drawLine(x, area.top(), x, area.bottom());
        }
        for (int y = area.top() + gridSize; y < area.bottom(); y += gridSize) {
            painter.drawLine(area.left(), y, area.right(), y);
        }

        QPen centerPen(QColor(0, 229, 255, 55));
        centerPen.setWidth(1);
        painter.setPen(centerPen);
        painter.drawLine(area.center().x(), area.top(), area.center().x(), area.bottom());
        painter.drawLine(area.left(), area.center().y(), area.right(), area.center().y());

        // 图像未就绪时，清掉记录的图像矩形，避免误命中。
        m_lastImageRect = QRect();
        m_lastImageSize = QSize();
        return;
    }

    // 保持纵横比缩放到可用区域中心。
    const QSize imgSize = m_image.size();
    const QSize target = imgSize.scaled(area.size(), Qt::KeepAspectRatio);
    const QRect dstRect(area.x() + (area.width() - target.width()) / 2,
                        area.y() + (area.height() - target.height()) / 2,
                        target.width(),
                        target.height());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(dstRect, m_image);

    // 记录本次绘制的图像矩形，供 hit test / setMillingPaths 后的重绘复用。
    m_lastImageRect = dstRect;
    m_lastImageSize = imgSize;

    // 叠加打磨路径
    if (!m_paths.isEmpty()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        // 未标定时整层半透明
        if (!m_calibrationApplied) {
            painter.setOpacity(0.6);
        }
        // 裁剪到图像区域，防止画到黑边外面
        painter.setClipRect(dstRect);

        for (int i = 0; i < m_paths.size(); ++i) {
            const auto &p = m_paths[i];
            const QPointF a = mapImageToWidget(p.uStart, p.vStart);
            const QPointF b = mapImageToWidget(p.uEnd, p.vEnd);

            const bool hovered = (i == m_hoveredPathIndex);
            QPen pen;
            if (p.doCut) {
                pen.setColor(hovered ? kCutHoverColor : kCutColor);
                pen.setWidth(hovered ? kCutLineWidth + 1 : kCutLineWidth);
                pen.setStyle(Qt::SolidLine);
            } else {
                pen.setColor(hovered ? kMoveHoverColor : kMoveColor);
                pen.setWidth(hovered ? kMoveLineWidth + 1 : kMoveLineWidth);
                pen.setStyle(Qt::DashLine);
            }
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.drawLine(a, b);
        }

        painter.restore();

        // 未标定角标
        if (!m_calibrationApplied) {
            QFont f = painter.font();
            f.setPixelSize(11);
            f.setBold(true);
            painter.setFont(f);
            const QString tag = QStringLiteral("未标定");
            const QRect tagRect(dstRect.right() - 68,
                                dstRect.bottom() - 24,
                                60, 18);
            painter.fillRect(tagRect, QColor(241, 196, 15, 200));
            painter.setPen(QColor("#1B2632"));
            painter.drawText(tagRect, Qt::AlignCenter, tag);
        }
    }

    if (!m_overlayText.isEmpty()) {
        QFont f = painter.font();
        f.setPixelSize(12);
        painter.setFont(f);
        const QRect textArea = area.adjusted(8, 6, -8, -6);
        painter.setPen(QColor(0, 0, 0, 160));
        painter.drawText(textArea.adjusted(1, 1, 1, 1), Qt::AlignLeft | Qt::AlignTop, m_overlayText);
        painter.setPen(QColor("#B8C8D8"));
        painter.drawText(textArea, Qt::AlignLeft | Qt::AlignTop, m_overlayText);
    }
}

void ImageViewFrame::mouseMoveEvent(QMouseEvent *event)
{
    QFrame::mouseMoveEvent(event);

    const int idx = hitTestPath(event->pos());
    if (idx != m_hoveredPathIndex) {
        m_hoveredPathIndex = idx;
        update();
    }
}

void ImageViewFrame::mousePressEvent(QMouseEvent *event)
{
    QFrame::mousePressEvent(event);

    if (event->button() != Qt::LeftButton) {
        return;
    }

    const int idx = hitTestPath(event->pos());
    if (idx >= 0 && idx < m_paths.size()) {
        const QString text = tooltipForPath(m_paths[idx], idx + 1);
        const QPoint tooltipPos = event->globalPos() + QPoint(14, 18);
        m_tooltipPathIndex = idx;
        QToolTip::showText(tooltipPos, text, this);
    } else {
        if (m_tooltipPathIndex != -1) {
            m_tooltipPathIndex = -1;
            QToolTip::hideText();
        }
    }
}

void ImageViewFrame::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    if (m_hoveredPathIndex != -1) {
        m_hoveredPathIndex = -1;
        update();
    }
    m_tooltipPathIndex = -1;
    QToolTip::hideText();
}
