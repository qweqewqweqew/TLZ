#include "vision/ImageViewFrame.h"

#include "app/Theme.h"

#include <QApplication>
#include <QtGlobal>
#include <QDateTime>
#include <QMetaObject>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QLineF>
#include <QVector>
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
        m_grindLayer = QImage();
        m_hasActiveGrindPath = false;
        m_activeGrindPath = QPainterPath();
        m_grindPaths.clear();
    }
    // 同尺寸新帧也强制立即重绘，避免只靠 update() 被合并后看起来“没刷新”。
    repaint();
}

void ImageViewFrame::clearImage()
{
    clearGrindLayer();
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
    recomputeToolPoseMapping();
    if (m_hoverCoordinateValid) {
        updateHoverCoordinate(m_hoverWidgetPos);
    }
    if (m_toolPoseValid) {
        update();
    }
}

void ImageViewFrame::clearToolPose()
{
    setToolPose(0.0, 0.0, 0.0, false);
}

void ImageViewFrame::setToolPose(double xMm, double yMm, double zMm, bool valid,
                                 bool leaveGrindTrace)
{
    if (!valid) {
        if (!m_toolPoseValid && !m_toolPoseMapped) {
            return;
        }
        m_toolPoseValid = false;
        m_toolPoseMapped = false;
        m_toolTrailCount = 0;
        m_toolTrailNext = 0;
        m_toolTrail.clear();
        breakGrindStroke();
        requestPoseUpdate();
        return;
    }

    const bool same = m_toolPoseValid
        && qFuzzyIsNull(m_toolPoseXMm - xMm)
        && qFuzzyIsNull(m_toolPoseYMm - yMm)
        && qFuzzyIsNull(m_toolPoseZMm - zMm);
    m_toolPoseValid = true;
    m_toolPoseXMm = xMm;
    m_toolPoseYMm = yMm;
    m_toolPoseZMm = zMm;
    recomputeToolPoseMapping();
    if (m_toolPoseMapped) {
        // Fixed ring buffer (~0.4s at 30Hz) — never grows unbounded.
        const QPointF uv(m_toolPoseU, m_toolPoseV);
        bool accept = (m_toolTrailCount == 0);
        if (!accept) {
            const int last = (m_toolTrailNext + kToolTrailCapacity - 1) % kToolTrailCapacity;
            accept = QLineF(m_toolTrailUvRing[last], uv).length() > 0.15;
        }
        if (accept) {
            m_toolTrailUvRing[m_toolTrailNext] = uv;
            m_toolTrailNext = (m_toolTrailNext + 1) % kToolTrailCapacity;
            if (m_toolTrailCount < kToolTrailCapacity) {
                ++m_toolTrailCount;
            }
        }
        if (leaveGrindTrace) {
            strokeGrindToImageUv(m_toolPoseU, m_toolPoseV);
        } else {
            breakGrindStroke();
        }
    } else {
        breakGrindStroke();
    }
    m_leaveGrindTrace = leaveGrindTrace && m_toolPoseMapped;
    if (!same || leaveGrindTrace) {
        requestPoseUpdate();
    }
}

void ImageViewFrame::clearGrindLayer()
{
    breakGrindStroke();
    m_grindPaths.clear();
    if (m_grindLayer.isNull()) {
        return;
    }
    m_grindLayer = QImage();
    update();
}

void ImageViewFrame::breakGrindStroke()
{
    if (m_hasActiveGrindPath && !m_activeGrindPath.isEmpty()) {
        m_grindPaths.push_back(m_activeGrindPath);
    }
    m_hasActiveGrindPath = false;
    m_activeGrindPath = QPainterPath();
}

void ImageViewFrame::ensureGrindLayer()
{
    if (m_image.isNull()) {
        return;
    }
    if (m_grindLayer.size() != m_image.size()
        || m_grindLayer.format() != QImage::Format_ARGB32_Premultiplied) {
        m_grindLayer = QImage(m_image.size(), QImage::Format_ARGB32_Premultiplied);
        m_grindLayer.fill(Qt::transparent);
    }
}

void ImageViewFrame::rebuildGrindLayer()
{
    if (m_image.isNull()) {
        return;
    }
    ensureGrindLayer();
    m_grindLayer.fill(Qt::transparent);

    // Stroke width ~ drill indicator diameter (~20px on screen).
    double sx = 1.0;
    if (!m_lastImageRect.isEmpty() && m_lastImageSize.width() > 0) {
        sx = double(m_lastImageRect.width()) / double(m_lastImageSize.width());
    }
    double penW = 20.0 / qMax(sx, 0.05);
    penW = qBound(16.0, penW, 96.0);

    QPainter gp(&m_grindLayer);
    gp.setRenderHint(QPainter::Antialiasing, true);
    gp.setCompositionMode(QPainter::CompositionMode_SourceOver);
    QColor ink(Theme::kAccent);
    ink.setAlpha(85); // ~33% perceived; texture remains readable
    QPen pen(ink, penW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    gp.setPen(pen);
    gp.setBrush(Qt::NoBrush);

    for (const QPainterPath &path : m_grindPaths) {
        if (!path.isEmpty()) {
            gp.drawPath(path);
        }
    }
    if (m_hasActiveGrindPath && !m_activeGrindPath.isEmpty()) {
        gp.drawPath(m_activeGrindPath);
    }
}

void ImageViewFrame::strokeGrindToImageUv(double u, double v)
{
    if (m_image.isNull()) {
        return;
    }
    const QPointF cur(u, v);
    if (!m_hasActiveGrindPath) {
        m_activeGrindPath = QPainterPath();
        m_activeGrindPath.moveTo(cur);
        m_hasActiveGrindPath = true;
        rebuildGrindLayer();
        return;
    }

    // Skip near-zero segments; path still holds last point.
    if (QLineF(m_activeGrindPath.currentPosition(), cur).length() < 0.5) {
        return;
    }
    m_activeGrindPath.lineTo(cur);
    // Redraw full cut strokes once so translucent alpha does not stack.
    rebuildGrindLayer();
}

void ImageViewFrame::setPathProgress(int completedPath, int currentPath)
{
    const int completed = qMax(0, completedPath);
    const int current = qMax(0, currentPath);
    if (completed == m_completedPathCount && current == m_currentPathOneBased) {
        return;
    }
    m_completedPathCount = completed;
    m_currentPathOneBased = current;
    requestPoseUpdate();
}

void ImageViewFrame::requestPoseUpdate()
{
    // PLC can be high-rate; Qt already coalesces update() into one paint per event-loop pass.
    // Still gate hard repaints to ~30 Hz so we do not flood when the event loop is idle.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastPoseUpdateMs > 0 && (now - m_lastPoseUpdateMs) < 33) {
        if (!m_poseUpdatePending) {
            m_poseUpdatePending = true;
            QMetaObject::invokeMethod(this, [this]() {
                m_poseUpdatePending = false;
                m_lastPoseUpdateMs = QDateTime::currentMSecsSinceEpoch();
                update();
            }, Qt::QueuedConnection);
        }
        return;
    }
    m_lastPoseUpdateMs = now;
    m_poseUpdatePending = false;
    update();
}

void ImageViewFrame::recomputeToolPoseMapping()
{
    m_toolPoseMapped = false;
    if (!m_toolPoseValid) {
        return;
    }
    double u = 0.0;
    double v = 0.0;
    if (mapMetricToImage(m_toolPoseXMm, m_toolPoseYMm, &u, &v)) {
        m_toolPoseU = u;
        m_toolPoseV = v;
        m_toolPoseMapped = true;
    }
}

bool ImageViewFrame::mapMetricToImage(double xMm, double yMm, double *uOut, double *vOut) const
{
    if (!uOut || !vOut) {
        return false;
    }
    if (!m_metricFrame.hasMetricGeometry || m_metricFrame.width == 0 || m_metricFrame.height == 0) {
        return false;
    }
    if (qFuzzyIsNull(m_metricFrame.xScaleMm)) {
        return false;
    }

    const double u = (xMm - m_metricFrame.xOffsetMm) / m_metricFrame.xScaleMm;
    double v = 0.0;
    const int height = int(m_metricFrame.height);
    const bool hasEncoder = m_metricFrame.encoderResolutionMm > 0.0
        && m_metricFrame.encoderValues.size() == height;
    if (hasEncoder) {
        // Find row whose metric Y is closest to yMm (same formula as hover readout).
        double bestDist = 1.0e100;
        int bestRow = -1;
        const qint32 enc0 = m_metricFrame.encoderValues.first();
        for (int row = 0; row < height; ++row) {
            const double y = m_metricFrame.yOffsetMm
                + double(qint64(m_metricFrame.encoderValues[row]) - qint64(enc0))
                    * m_metricFrame.encoderResolutionMm;
            const double d = std::abs(y - yMm);
            if (d < bestDist) {
                bestDist = d;
                bestRow = row;
            }
        }
        if (bestRow < 0) {
            return false;
        }
        v = double(bestRow);
    } else {
        if (qFuzzyIsNull(m_metricFrame.yScaleMm)) {
            return false;
        }
        v = (yMm - m_metricFrame.yOffsetMm) / m_metricFrame.yScaleMm;
    }

    if (u < -0.5 || v < -0.5
        || u > double(m_metricFrame.width) - 0.5
        || v > double(m_metricFrame.height) - 0.5) {
        return false;
    }
    *uOut = u;
    *vOut = v;
    return true;
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

    if (!m_grindLayer.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(dstRect, m_grindLayer);
    }

    if (!m_paths.isEmpty()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setClipRect(dstRect.intersected(QRectF(area)));

        const bool useProgress = (m_completedPathCount > 0 || m_currentPathOneBased > 0);
        const int activeIndex = m_currentPathOneBased > 0 ? m_currentPathOneBased - 1 : -1;

        for (int i = 0; i < m_paths.size(); ++i) {
            const auto &p = m_paths[i];
            const QPointF a = mapImageToWidget(p.uStart, p.vStart);
            const QPointF b = mapImageToWidget(p.uEnd, p.vEnd);

            const bool hovered = (i == m_hoveredPathIndex);
            QPen pen;
            if (useProgress) {
                if (i < m_completedPathCount) {
                    QColor done(Theme::kTextMuted);
                    done.setAlpha(160);
                    pen.setColor(done);
                    pen.setWidth(kMoveLineWidth);
                } else if (i == activeIndex) {
                    pen.setColor(QColor(Theme::kRealtime));
                    pen.setWidth(kCutLineWidth + 1);
                } else {
                    QColor pending(Theme::kAccent);
                    pending.setAlpha(110);
                    pen.setColor(pending);
                    pen.setWidth(kMoveLineWidth);
                }
                if (hovered) {
                    pen.setWidth(pen.width() + 1);
                }
            } else if (p.doCut) {
                pen.setColor(hovered ? kCutHoverColor : kCutColor);
                pen.setWidth(hovered ? kCutLineWidth + 1 : kCutLineWidth);
            } else {
                pen.setColor(hovered ? kMoveHoverColor : kMoveColor);
                pen.setWidth(hovered ? kMoveLineWidth + 1 : kMoveLineWidth);
            }
            pen.setStyle(Qt::SolidLine);
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.drawLine(a, b);
        }

        painter.restore();
    }

    // Tool pose marker (PLC XYZ -> same metric mapping as hover).
    if (m_toolPoseValid && m_toolPoseMapped && !dstRect.isEmpty()) {
        const double sx = double(m_lastImageRect.width()) / double(m_lastImageSize.width());
        const double sy = double(m_lastImageRect.height()) / double(m_lastImageSize.height());
        m_toolTrail.clear();
        m_toolTrail.reserve(m_toolTrailCount);
        const int start = (m_toolTrailNext - m_toolTrailCount + kToolTrailCapacity)
            % kToolTrailCapacity;
        for (int i = 0; i < m_toolTrailCount; ++i) {
            const QPointF &uv = m_toolTrailUvRing[(start + i) % kToolTrailCapacity];
            m_toolTrail.push_back(QPointF(
                m_lastImageRect.x() + uv.x() * sx,
                m_lastImageRect.y() + uv.y() * sy));
        }
        const QPointF tip(
            m_lastImageRect.x() + m_toolPoseU * sx,
            m_lastImageRect.y() + m_toolPoseV * sy);

        // Soft trail (about 0.4s of recent tips) then current bit disc.
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setClipRect(dstRect.intersected(QRectF(area)));
        const QRectF drawBounds = dstRect.adjusted(-20, -20, 20, 20);
        constexpr double kBitRadius = 7.5; // ~15px diameter
        for (int i = 0; i < m_toolTrail.size(); ++i) {
            const QPointF &pt = m_toolTrail.at(i);
            if (!drawBounds.contains(pt)) {
                continue;
            }
            const double age = (m_toolTrail.size() <= 1)
                ? 1.0
                : double(i + 1) / double(m_toolTrail.size());
            QColor trail(Theme::kAccent);
            trail.setAlpha(int(191 * age * 0.45));
            painter.setPen(Qt::NoPen);
            painter.setBrush(trail);
            painter.drawEllipse(pt, kBitRadius * 0.85, kBitRadius * 0.85);
        }
        if (drawBounds.contains(tip)) {
            QColor bit(Theme::kAccent);
            bit.setAlpha(191); // 75% opacity
            painter.setPen(Qt::NoPen);
            painter.setBrush(bit);
            painter.drawEllipse(tip, kBitRadius, kBitRadius);
            // Tiny realtime center for alignment; keep subordinate to the disc.
            QColor core(Theme::kRealtime);
            core.setAlpha(220);
            painter.setBrush(core);
            painter.drawEllipse(tip, 2.0, 2.0);
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
