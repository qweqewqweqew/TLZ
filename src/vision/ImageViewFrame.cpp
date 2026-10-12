#include "vision/ImageViewFrame.h"

#include "app/Theme.h"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRect>
#include <QToolTip>
#include <QWheelEvent>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

constexpr int kCutLineWidth = 2;
constexpr int kMoveLineWidth = 1;
constexpr double kHitThresholdPx = 6.0;
constexpr double kZoomStep = 1.15;
constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 20.0;

const QColor kCutColor("#F5C400");
const QColor kMoveColor("#2ECC71");
const QColor kCutHoverColor("#FFE566");
const QColor kMoveHoverColor("#7DFFB0");

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
    const QSize previousSize = m_image.size();
    m_image = image;
    if (m_image.isNull() || previousSize != m_image.size()) {
        resetView();
    }
    // 同尺寸新帧也强制立即重绘，避免只靠 update() 被合并后看起来“没刷新”。
    repaint();
}

void ImageViewFrame::clearImage()
{
    setImage(QImage());
}

void ImageViewFrame::setOverlayText(const QString &text)
{
    if (m_overlayText == text) {
        return;
    }
    m_overlayText = text;
    update();
}

void ImageViewFrame::setMillingPaths(const QVector<MillingPathVM> &paths)
{
    m_paths = paths;
    m_hoveredPathIndex = -1;
    m_tooltipPathIndex = -1;
    QToolTip::hideText();
    update();
}

void ImageViewFrame::clearMillingPaths()
{
    m_paths.clear();
    m_hoveredPathIndex = -1;
    m_tooltipPathIndex = -1;
    QToolTip::hideText();
    update();
}

void ImageViewFrame::setMetricFrame(const PointCloudFrame &frame)
{
    m_metricFrame = frame;
    if (m_hoverCoordinateValid) {
        updateHoverCoordinate(m_hoverWidgetPos);
    }
}

QRectF ImageViewFrame::imageViewportRect() const
{
    return QRectF(rect().adjusted(1, 1, -2, -2));
}

QSizeF ImageViewFrame::fittedImageSize() const
{
    if (m_image.isNull()) {
        return QSizeF();
    }
    const QSize target = m_image.size().scaled(imageViewportRect().size().toSize(),
                                                Qt::KeepAspectRatio);
    return QSizeF(target);
}

QRectF ImageViewFrame::currentImageRect() const
{
    const QRectF area = imageViewportRect();
    const QSizeF fitted = fittedImageSize();
    if (area.isEmpty() || fitted.isEmpty()) {
        return QRectF();
    }

    const QSizeF shown(fitted.width() * m_zoomFactor,
                       fitted.height() * m_zoomFactor);
    return QRectF(area.center().x() - shown.width() / 2.0 + m_panOffset.x(),
                  area.center().y() - shown.height() / 2.0 + m_panOffset.y(),
                  shown.width(),
                  shown.height());
}

void ImageViewFrame::resetView()
{
    m_zoomFactor = kMinZoom;
    m_panOffset = QPointF();
    m_dragging = false;
    m_dragMoved = false;
    m_hoveredPathIndex = -1;
    m_tooltipPathIndex = -1;
    m_hoverCoordinateValid = false;
    m_hoverCoordinateText.clear();
    QToolTip::hideText();
    setCursor(m_image.isNull() ? Qt::ArrowCursor : Qt::OpenHandCursor);
    update();
}

void ImageViewFrame::clampPanOffset()
{
    const QRectF area = imageViewportRect();
    const QSizeF fitted = fittedImageSize();
    if (area.isEmpty() || fitted.isEmpty()) {
        m_panOffset = QPointF();
        return;
    }

    const double shownWidth = fitted.width() * m_zoomFactor;
    const double shownHeight = fitted.height() * m_zoomFactor;
    const double maxX = qMax(0.0, (shownWidth - area.width()) / 2.0);
    const double maxY = qMax(0.0, (shownHeight - area.height()) / 2.0);
    m_panOffset.setX(qBound(-maxX, m_panOffset.x(), maxX));
    m_panOffset.setY(qBound(-maxY, m_panOffset.y(), maxY));
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

QPoint ImageViewFrame::mapWidgetToImage(const QPointF &widgetPos) const
{
    if (m_lastImageRect.isEmpty() || m_lastImageSize.isEmpty()) {
        return QPoint(-1, -1);
    }
    const double column =
        (widgetPos.x() - m_lastImageRect.x()) / m_lastImageRect.width()
        * double(m_lastImageSize.width());
    const double row =
        (widgetPos.y() - m_lastImageRect.y()) / m_lastImageRect.height()
        * double(m_lastImageSize.height());
    const int u = int(std::floor(column));
    const int v = int(std::floor(row));
    if (u < 0 || v < 0 || u >= m_lastImageSize.width() || v >= m_lastImageSize.height()) {
        return QPoint(-1, -1);
    }
    return QPoint(u, v);
}

void ImageViewFrame::updateHoverCoordinate(const QPointF &widgetPos)
{
    m_hoverWidgetPos = widgetPos;
    const QPoint pixel = mapWidgetToImage(widgetPos);
    if (pixel.x() < 0) {
        if (m_hoverCoordinateValid) {
            m_hoverCoordinateValid = false;
            m_hoverCoordinateText.clear();
            update();
        }
        return;
    }

    const QString text = coordinateTextAt(pixel.x(), pixel.y());
    if (m_hoverCoordinateValid && m_hoverCoordinateText == text) {
        if (m_hoverWidgetPos != widgetPos) {
            update();
        }
        return;
    }
    m_hoverCoordinateValid = true;
    m_hoverCoordinateText = text;
    update();
}

QString ImageViewFrame::coordinateTextAt(int column, int row) const
{
    const QString pixelLine = QStringLiteral("像素  %1, %2").arg(column).arg(row);
    const int width = int(m_metricFrame.width);
    const int height = int(m_metricFrame.height);
    const bool inMetric = width > 0 && height > 0 && column < width && row < height;

    QString xText = QStringLiteral("--");
    QString yText = QStringLiteral("--");
    QString zText = QStringLiteral("--");

    if (inMetric && m_metricFrame.hasMetricGeometry) {
        const double x = m_metricFrame.xOffsetMm + double(column) * m_metricFrame.xScaleMm;
        const bool hasEncoder = m_metricFrame.encoderResolutionMm > 0.0 &&
            m_metricFrame.encoderValues.size() == height;
        const double y = hasEncoder
            ? m_metricFrame.yOffsetMm +
                double(qint64(m_metricFrame.encoderValues[row]) -
                       qint64(m_metricFrame.encoderValues.first())) *
                    m_metricFrame.encoderResolutionMm
            : m_metricFrame.yOffsetMm + double(row) * m_metricFrame.yScaleMm;
        xText = QString::number(x, 'f', 3);
        yText = QString::number(y, 'f', 3);
    }

    if (inMetric && m_metricFrame.hasFloatRange()) {
        const float *range =
            reinterpret_cast<const float *>(m_metricFrame.rangeFloat32.constData());
        const float z = range[std::size_t(row) * std::size_t(width) + std::size_t(column)];
        if (std::isfinite(z) && z != m_metricFrame.missingValue) {
            zText = QString::number(double(z), 'f', 3);
        }
    }

    return QStringLiteral("%1\nX  %2 mm\nY  %3 mm\nZ  %4 mm")
        .arg(pixelLine, xText, yText, zText);
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
    painter.fillRect(area, QColor(Theme::kCanvasBg));

    if (m_image.isNull()) {
        QColor gridColor(Theme::kBorder);
        gridColor.setAlpha(70);
        QPen gridPen(gridColor);
        gridPen.setWidth(1);
        painter.setPen(gridPen);

        constexpr int gridSize = 48;
        for (int x = area.left() + gridSize; x < area.right(); x += gridSize) {
            painter.drawLine(x, area.top(), x, area.bottom());
        }
        for (int y = area.top() + gridSize; y < area.bottom(); y += gridSize) {
            painter.drawLine(area.left(), y, area.right(), y);
        }

        QColor centerColor(Theme::kAccent);
        centerColor.setAlpha(55);
        QPen centerPen(centerColor);
        centerPen.setWidth(1);
        painter.setPen(centerPen);
        painter.drawLine(area.center().x(), area.top(), area.center().x(), area.bottom());
        painter.drawLine(area.left(), area.center().y(), area.right(), area.center().y());

        m_lastImageRect = QRectF();
        m_lastImageSize = QSize();
        return;
    }

    const QSize imgSize = m_image.size();
    clampPanOffset();
    const QRectF dstRect = currentImageRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(dstRect, m_image);

    m_lastImageRect = dstRect;
    m_lastImageSize = imgSize;

    if (!m_paths.isEmpty()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setClipRect(dstRect.intersected(QRectF(area)));

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
                pen.setStyle(Qt::SolidLine);
            }
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.drawLine(a, b);
        }

        painter.restore();
    }

    if (!m_overlayText.isEmpty()) {
        QFont f = painter.font();
        f.setPixelSize(12);
        painter.setFont(f);
        const QRect textArea = area.adjusted(8, 6, -8, -6);
        painter.setPen(QColor(0, 0, 0, 160));
        painter.drawText(textArea.adjusted(1, 1, 1, 1), Qt::AlignLeft | Qt::AlignTop, m_overlayText);
        painter.setPen(QColor(Theme::kTextSecondary));
        painter.drawText(textArea, Qt::AlignLeft | Qt::AlignTop, m_overlayText);
    }

    if (m_hoverCoordinateValid && !m_hoverCoordinateText.isEmpty() && !m_dragging) {
        QFont hoverFont = painter.font();
        hoverFont.setPixelSize(13);
        painter.setFont(hoverFont);
        const QFontMetrics metrics(hoverFont);
        const QRect textBound = metrics.boundingRect(
            QRect(0, 0, 280, 120),
            Qt::AlignLeft | Qt::AlignTop,
            m_hoverCoordinateText);
        const QSize boxSize(textBound.width() + 16, textBound.height() + 12);
        QPoint topLeft = m_hoverWidgetPos.toPoint() + QPoint(16, 18);
        if (topLeft.x() + boxSize.width() > area.right() - 4) {
            topLeft.setX(m_hoverWidgetPos.toPoint().x() - boxSize.width() - 12);
        }
        if (topLeft.y() + boxSize.height() > area.bottom() - 4) {
            topLeft.setY(m_hoverWidgetPos.toPoint().y() - boxSize.height() - 12);
        }
        topLeft.setX(qMax(area.left() + 4, topLeft.x()));
        topLeft.setY(qMax(area.top() + 4, topLeft.y()));

        const QRect box(topLeft, boxSize);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(Theme::kBorder), 1));
        painter.setBrush(QColor(20, 28, 36, 210));
        painter.drawRoundedRect(box, 4, 4);
        painter.setPen(QColor(Theme::kTextPrimary));
        painter.drawText(box.adjusted(8, 6, -8, -6),
                         Qt::AlignLeft | Qt::AlignTop,
                         m_hoverCoordinateText);
    }
}

void ImageViewFrame::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull() || event->angleDelta().y() == 0) {
        QFrame::wheelEvent(event);
        return;
    }

    const QRectF oldRect = currentImageRect();
    if (oldRect.isEmpty()) {
        return;
    }

    const double wheelSteps = double(event->angleDelta().y()) / 120.0;
    const double newZoom = qBound(kMinZoom,
                                  m_zoomFactor * std::pow(kZoomStep, wheelSteps),
                                  kMaxZoom);
    if (qFuzzyCompare(newZoom, m_zoomFactor)) {
        event->accept();
        return;
    }

    QPointF anchor = event->posF();
    if (!oldRect.contains(anchor)) {
        anchor = oldRect.center();
    }
    const double relativeX = (anchor.x() - oldRect.left()) / oldRect.width();
    const double relativeY = (anchor.y() - oldRect.top()) / oldRect.height();

    m_zoomFactor = newZoom;
    const QRectF area = imageViewportRect();
    const QSizeF fitted = fittedImageSize();
    const QSizeF shown(fitted.width() * m_zoomFactor,
                       fitted.height() * m_zoomFactor);
    const QPointF centeredTopLeft(area.center().x() - shown.width() / 2.0,
                                  area.center().y() - shown.height() / 2.0);
    m_panOffset = anchor - centeredTopLeft -
                  QPointF(relativeX * shown.width(), relativeY * shown.height());
    clampPanOffset();
    update();
    event->accept();
}

void ImageViewFrame::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const bool passedThreshold =
            (event->pos() - m_dragStartPosition).manhattanLength() >=
            QApplication::startDragDistance();
        if (m_dragMoved || passedThreshold) {
            const QPoint delta = m_dragMoved
                                     ? event->pos() - m_lastDragPosition
                                     : event->pos() - m_dragStartPosition;
            m_dragMoved = true;
            m_panOffset += QPointF(delta);
            clampPanOffset();
            m_lastDragPosition = event->pos();
            m_hoveredPathIndex = -1;
            m_tooltipPathIndex = -1;
            m_hoverCoordinateValid = false;
            m_hoverCoordinateText.clear();
            QToolTip::hideText();
            setCursor(Qt::ClosedHandCursor);
            update();
        }
        event->accept();
        return;
    }

    updateHoverCoordinate(event->pos());
    const int idx = hitTestPath(event->pos());
    if (idx != m_hoveredPathIndex) {
        m_hoveredPathIndex = idx;
        update();
    }
    QFrame::mouseMoveEvent(event);
}

void ImageViewFrame::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && !m_image.isNull()) {
        resetView();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && !m_image.isNull()) {
        m_dragging = true;
        m_dragMoved = false;
        m_dragStartPosition = event->pos();
        m_lastDragPosition = event->pos();
        event->accept();
        return;
    }

    QFrame::mousePressEvent(event);
}

void ImageViewFrame::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging) {
        QFrame::mouseReleaseEvent(event);
        return;
    }

    const bool wasDrag = m_dragMoved;
    m_dragging = false;
    m_dragMoved = false;
    setCursor(m_image.isNull() ? Qt::ArrowCursor : Qt::OpenHandCursor);

    if (wasDrag) {
        event->accept();
        return;
    }

    const int idx = hitTestPath(event->pos());
    if (idx >= 0 && idx < m_paths.size()) {
        const QString text = tooltipForPath(m_paths[idx], idx + 1);
        const QPoint tooltipPos = event->globalPos() + QPoint(14, 18);
        m_tooltipPathIndex = idx;
        QToolTip::showText(tooltipPos, text, this);
        emit millingPathClicked(idx);
    } else if (m_tooltipPathIndex != -1) {
        m_tooltipPathIndex = -1;
        QToolTip::hideText();
    }
    event->accept();
}

void ImageViewFrame::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    if (m_hoveredPathIndex != -1) {
        m_hoveredPathIndex = -1;
    }
    m_tooltipPathIndex = -1;
    m_hoverCoordinateValid = false;
    m_hoverCoordinateText.clear();
    QToolTip::hideText();
    update();
}
