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
#include <QResizeEvent>
#include <QVector>
#include <QRect>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

constexpr int kCutLineWidth = 2;
constexpr int kMoveLineWidth = 1;
constexpr double kHitThresholdPx = 6.0;
// 点击选中粒子的宽容半径：不必精确压在路径线上，点在粒子附近即可。
// TODO(粒子数据): 算法下发粒子 bbox 后，命中测试应改为"点是否落在粒子区域内"，
// 这里的路径线容差只是过渡方案。详见 README「粒子信息与粒子三维点云」。
constexpr double kParticleClickThresholdPx = 26.0;
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

class DynamicLayer : public QWidget
{
public:
    explicit DynamicLayer(ImageViewFrame *host)
        : QWidget(host)
        , m_host(host)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setClipRegion(event->region());
        m_host->paintDynamic(painter);
    }

private:
    ImageViewFrame *m_host;
};

ImageViewFrame::ImageViewFrame(QWidget *parent)
    : QFrame(parent)
{
    setMouseTracking(true);
    m_dynamic = new DynamicLayer(this);
    m_dynamic->setGeometry(rect());
    m_dynamic->raise();
    Theme::watch(this, [this]() {
        m_viewCacheDirty = true;
        update();
        refreshDynamic();
    });
}

void ImageViewFrame::setImage(const QImage &image)
{
    const QSize previousSize = m_image.size();
    m_image = image;
    m_viewCacheDirty = true;
    if (m_image.isNull() || previousSize != m_image.size()) {
        resetView();
        m_basePixmap = QPixmap();
        m_baseImageKey = 0;
        m_hasActiveGrindPath = false;
        m_activeGrindPath = QPainterPath();
        m_grindPaths.clear();
    }
    // 同尺寸新帧也强制立即重绘，避免只靠 update() 被合并后看起来“没刷新”。
    repaint();
    if (m_dynamic) {
        m_dynamic->repaint();
    }
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
    refreshDynamic();
}

void ImageViewFrame::setMillingPaths(const QVector<MillingPathVM> &paths)
{
    m_paths = paths;
    m_hoveredPathIndex = -1;
    m_tooltipPathIndex = -1;
    QToolTip::hideText();
    refreshDynamic();
}

void ImageViewFrame::clearMillingPaths()
{
    m_paths.clear();
    m_hoveredPathIndex = -1;
    m_tooltipPathIndex = -1;
    QToolTip::hideText();
    refreshDynamic();
}

void ImageViewFrame::setMetricFrame(const PointCloudFrame &frame)
{
    m_metricFrame = frame;
    recomputeToolPoseMapping();
    if (m_hoverCoordinateValid) {
        updateHoverCoordinate(m_hoverWidgetPos);
    }
    if (m_toolPoseValid) {
        refreshDynamic();
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
    const bool hadStroke = m_hasActiveGrindPath || !m_grindPaths.isEmpty();
    m_hasActiveGrindPath = false;
    m_activeGrindPath = QPainterPath();
    m_grindPaths.clear();
    if (hadStroke) {
        refreshDynamic();
    }
}

void ImageViewFrame::breakGrindStroke()
{
    if (m_hasActiveGrindPath && !m_activeGrindPath.isEmpty()) {
        m_grindPaths.push_back(m_activeGrindPath);
    }
    m_hasActiveGrindPath = false;
    m_activeGrindPath = QPainterPath();
}

void ImageViewFrame::noteInteraction()
{
    m_interacting = true;
    if (!m_interactionTimer) {
        m_interactionTimer = new QTimer(this);
        m_interactionTimer->setSingleShot(true);
        connect(m_interactionTimer, &QTimer::timeout, this, [this]() {
            m_interacting = false;
            m_viewCacheDirty = true;
            update();
        });
    }
    m_interactionTimer->start(90);
}

void ImageViewFrame::ensureBasePixmap(const QSize &pixelSize)
{
    if (pixelSize.isEmpty() || m_image.isNull()) {
        return;
    }
    if (!m_basePixmap.isNull() && m_baseImageKey == m_image.cacheKey()
        && m_basePixelSize == pixelSize) {
        return;
    }
    QImage scaled = m_image.scaled(pixelSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_basePixmap = QPixmap::fromImage(scaled);
    m_baseImageKey = m_image.cacheKey();
    m_basePixelSize = pixelSize;
}

void ImageViewFrame::drawGrindPaths(QPainter &painter,
                                    const QRectF &dstRect,
                                    const QRect &clip) const
{
    if (dstRect.isEmpty() || m_image.isNull()) {
        return;
    }
    if (m_grindPaths.isEmpty() && !m_hasActiveGrindPath) {
        return;
    }

    painter.save();
    painter.setClipRect(dstRect.intersected(QRectF(clip)));
    painter.setRenderHint(QPainter::Antialiasing, true);
    QTransform xform;
    xform.translate(dstRect.left(), dstRect.top());
    xform.scale(dstRect.width() / double(m_image.width()),
                dstRect.height() / double(m_image.height()));
    painter.setTransform(xform, true);

    // 线宽按屏幕像素，不随视图缩放变粗。位置仍贴在铜板像素上。
    QColor ink(Theme::markColor());
    ink.setAlpha(150);
    QPen pen(ink, 18.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    for (const QPainterPath &path : m_grindPaths) {
        if (!path.isEmpty()) {
            painter.drawPath(path);
        }
    }
    if (m_hasActiveGrindPath && !m_activeGrindPath.isEmpty()) {
        painter.drawPath(m_activeGrindPath);
    }
    painter.restore();
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
        return;
    }

    // Skip near-zero segments; path still holds last point.
    if (QLineF(m_activeGrindPath.currentPosition(), cur).length() < 0.5) {
        return;
    }

    m_activeGrindPath.lineTo(cur);
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
    refreshDynamic();
}

void ImageViewFrame::requestPoseUpdate()
{
    // 只刷新刀具和最新刀路经过的区域，底图缓存不动。
    const auto fire = [this]() {
        const QRect next = toolPoseDirtyRect();
        if (next.isEmpty()) {
            m_lastDynamicDirty = QRect();
            refreshDynamic();
            return;
        }
        const QRect previous = m_lastDynamicDirty;
        m_lastDynamicDirty = next;
        invalidateDynamic(previous);
        invalidateDynamic(next);
    };

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastPoseUpdateMs > 0 && (now - m_lastPoseUpdateMs) < 33) {
        if (!m_poseUpdatePending) {
            m_poseUpdatePending = true;
            QMetaObject::invokeMethod(this, [this, fire]() {
                m_poseUpdatePending = false;
                m_lastPoseUpdateMs = QDateTime::currentMSecsSinceEpoch();
                fire();
            }, Qt::QueuedConnection);
        }
        return;
    }
    m_lastPoseUpdateMs = now;
    m_poseUpdatePending = false;
    fire();
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
    m_viewCacheDirty = true;
    update();
    refreshDynamic();
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

QRect ImageViewFrame::hoverBoxRect(const QPointF &anchor,
                                   const QString &text,
                                   const QRect &area) const
{
    if (text.isEmpty() || area.isEmpty()) {
        return QRect();
    }
    QFont hoverFont(QStringLiteral("Microsoft YaHei"));
    hoverFont.setPixelSize(13);
    QFont valueFont = hoverFont;
    valueFont.setBold(true);
    const QFontMetrics labelMetrics(hoverFont);
    const QFontMetrics valueMetrics(valueFont);
    int maxValueWidth = 0;
    int lineCount = 0;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int split = line.indexOf(QLatin1Char(' '));
        const QString value = split < 0 ? line : line.mid(split + 1).trimmed();
        maxValueWidth = qMax(maxValueWidth, valueMetrics.horizontalAdvance(value));
        ++lineCount;
    }
    constexpr int kLabelWidth = 40;
    const QSize boxSize(10 + kLabelWidth + 8 + maxValueWidth + 12,
                        8 + lineCount * (labelMetrics.height() + 2) + 6);
    QPoint topLeft = anchor.toPoint() + QPoint(16, 18);
    if (topLeft.x() + boxSize.width() > area.right() - 4) {
        topLeft.setX(anchor.toPoint().x() - boxSize.width() - 14);
    }
    if (topLeft.y() + boxSize.height() > area.bottom() - 4) {
        topLeft.setY(anchor.toPoint().y() - boxSize.height() - 14);
    }
    topLeft.setX(qBound(area.left() + 4, topLeft.x(), qMax(area.left() + 4, area.right() - boxSize.width() - 4)));
    topLeft.setY(qBound(area.top() + 4, topLeft.y(), qMax(area.top() + 4, area.bottom() - boxSize.height() - 4)));
    return QRect(topLeft, boxSize);
}

void ImageViewFrame::updateHoverCoordinate(const QPointF &widgetPos)
{
    const QPointF previousPos = m_hoverWidgetPos;
    const bool wasValid = m_hoverCoordinateValid;
    const QString previousText = m_hoverCoordinateText;
    m_hoverWidgetPos = widgetPos;

    const QRect area = imageViewportRect().toAlignedRect();
    const auto invalidate = [this, area](const QPointF &anchor, const QString &text) {
        const QRect box = hoverBoxRect(anchor, text, area);
        if (!box.isEmpty()) {
            invalidateDynamic(box);
        }
    };

    const QPoint pixel = mapWidgetToImage(widgetPos);
    if (pixel.x() < 0) {
        if (wasValid) {
            m_hoverCoordinateValid = false;
            m_hoverCoordinateText.clear();
            invalidate(previousPos, previousText);
        }
        return;
    }

    const QString text = coordinateTextAt(pixel.x(), pixel.y());
    const bool moved = previousPos != widgetPos;
    const bool textChanged = !wasValid || previousText != text;
    m_hoverCoordinateValid = true;
    m_hoverCoordinateText = text;
    if (!moved && !textChanged) {
        return;
    }
    if (wasValid) {
        invalidate(previousPos, previousText);
    }
    invalidate(widgetPos, text);
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
    return hitTestPathInRange(widgetPos, kHitThresholdPx);
}

int ImageViewFrame::hitTestPathInRange(const QPointF &widgetPos, double thresholdPx) const
{
    if (m_paths.isEmpty() || m_lastImageRect.isEmpty() || m_lastImageSize.isEmpty()) {
        return -1;
    }

    double bestDist = thresholdPx;
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
    ensureViewCache();
    if (m_viewCache.isNull()) {
        return;
    }

    QPainter painter(this);
    painter.setClipRegion(event->region());
    painter.drawPixmap(QPoint(0, 0), m_viewCache);
}

void ImageViewFrame::ensureViewCache()
{
    const qreal dpr = devicePixelRatioF();
    const QSize cachePx(qMax(1, qRound(width() * dpr)), qMax(1, qRound(height() * dpr)));
    if (!m_viewCacheDirty && m_viewCache.size() == cachePx
        && qFuzzyCompare(m_viewCache.devicePixelRatioF(), dpr)) {
        return;
    }

    m_viewCache = QPixmap(cachePx);
    m_viewCache.setDevicePixelRatio(dpr);
    m_viewCache.fill(Qt::transparent);
    QPainter painter(&m_viewCache);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect area = rect().adjusted(1, 1, -2, -2);
    const QColor canvasColor(Theme::kCanvasBg);
    painter.fillRect(rect(), canvasColor);
    painter.fillRect(area, canvasColor);

    if (m_image.isNull()) {
        const QColor gridColor(Theme::kDivider);
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

        const QColor centerColor(Theme::kTextMuted);
        QPen centerPen(centerColor);
        centerPen.setWidth(1);
        painter.setPen(centerPen);
        painter.drawLine(area.center().x(), area.top(), area.center().x(), area.bottom());
        painter.drawLine(area.left(), area.center().y(), area.right(), area.center().y());

        m_lastImageRect = QRectF();
        m_lastImageSize = QSize();
        m_viewCacheDirty = false;
        return;
    }

    const QSize imgSize = m_image.size();
    clampPanOffset();
    const QRectF dstRect = currentImageRect();
    m_lastImageRect = dstRect;
    m_lastImageSize = imgSize;

    const QRectF visible = dstRect.intersected(QRectF(area));
    if (!visible.isEmpty() && dstRect.width() > 0.0 && dstRect.height() > 0.0) {
        const qreal dpr = painter.device()->devicePixelRatioF();
        const QSizeF fitted = fittedImageSize();
        const int fitW = qMax(1, int(std::ceil(fitted.width() * dpr)));
        const int fitH = qMax(1, int(std::ceil(fitted.height() * dpr)));
        // 底图按视口的两倍准备，放大到约 2 倍仍然清晰，且不再碰原始扫描图。
        const QSize basePx(qMin(imgSize.width(), fitW * 2),
                           qMin(imgSize.height(), fitH * 2));
        ensureBasePixmap(basePx);

        const QRectF source(
            (visible.left() - dstRect.left()) / dstRect.width() * m_basePixmap.width(),
            (visible.top() - dstRect.top()) / dstRect.height() * m_basePixmap.height(),
            visible.width() / dstRect.width() * m_basePixmap.width(),
            visible.height() / dstRect.height() * m_basePixmap.height());
        painter.setRenderHint(QPainter::SmoothPixmapTransform, !m_interacting);
        painter.drawPixmap(visible, m_basePixmap, source);
    }

    m_viewCacheDirty = false;
}

void ImageViewFrame::paintDynamic(QPainter &painter)
{
    ensureViewCache();
    if (!m_viewCache.isNull()) {
        painter.drawPixmap(QPoint(0, 0), m_viewCache);
    }
    if (m_image.isNull()) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    const QRect area = rect().adjusted(1, 1, -2, -2);
    clampPanOffset();
    const QRectF dstRect = currentImageRect();
    m_lastImageRect = dstRect;
    m_lastImageSize = m_image.size();

    drawGrindPaths(painter, dstRect, area);

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
                    QColor pending(Theme::markColor());
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
        constexpr double kBitRadius = 11.25; // 原 7.5，半径增加二分之一
        for (int i = 0; i < m_toolTrail.size(); ++i) {
            const QPointF &pt = m_toolTrail.at(i);
            if (!drawBounds.contains(pt)) {
                continue;
            }
            const double age = (m_toolTrail.size() <= 1)
                ? 1.0
                : double(i + 1) / double(m_toolTrail.size());
            QColor trail(Theme::markColor());
            trail.setAlpha(int(191 * age * 0.45));
            painter.setPen(Qt::NoPen);
            painter.setBrush(trail);
            painter.drawEllipse(pt, kBitRadius * 0.85, kBitRadius * 0.85);
        }
        if (drawBounds.contains(tip)) {
            QColor bit(Theme::markColor());
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
        const QRect box = hoverBoxRect(m_hoverWidgetPos, m_hoverCoordinateText, area);
        QFont hoverFont(QStringLiteral("Microsoft YaHei"));
        hoverFont.setPixelSize(13);
        painter.setFont(hoverFont);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(31, 42, 51, 45));
        painter.drawRoundedRect(box.adjusted(1, 2, 2, 3), 4, 4);
        painter.setPen(QPen(QColor(Theme::kTextSecondary), 1));
        painter.setBrush(QColor(Theme::kPanelBg));
        painter.drawRoundedRect(box, 4, 4);

        const QStringList lines = m_hoverCoordinateText.split(QLatin1Char('\n'));
        const QFontMetrics metrics(hoverFont);
        int y = box.top() + 7;
        const int textLeft = box.left() + 10;
        const int textRight = box.right() - 10;
        for (const QString &line : lines) {
            const int split = line.indexOf(QLatin1Char(' '));
            const QString label = split < 0 ? line : line.left(split);
            const QString value = split < 0 ? QString() : line.mid(split + 1).trimmed();
            painter.setPen(QColor(Theme::kTextSecondary));
            painter.drawText(QRect(textLeft, y, 40, metrics.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             label);
            QFont valueFont = hoverFont;
            valueFont.setBold(true);
            painter.setFont(valueFont);
            painter.setPen(QColor(Theme::kTextPrimary));
            painter.drawText(QRect(textLeft + 48, y, textRight - textLeft - 48, metrics.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             value);
            painter.setFont(hoverFont);
            y += metrics.height() + 2;
        }
    }
}

void ImageViewFrame::refreshDynamic()
{
    if (m_dynamic) {
        m_dynamic->update();
    }
}

void ImageViewFrame::resizeEvent(QResizeEvent *event)
{
    m_viewCacheDirty = true;
    QFrame::resizeEvent(event);
    if (m_dynamic) {
        m_dynamic->setGeometry(rect());
        m_dynamic->raise();
    }
}

void ImageViewFrame::invalidateDynamic(const QRect &rect)
{
    if (!m_dynamic || rect.isEmpty()) {
        return;
    }
    const QRect clipped = rect.adjusted(-3, -3, 3, 3).intersected(this->rect());
    if (!clipped.isEmpty()) {
        m_dynamic->update(clipped);
    }
}

QRect ImageViewFrame::pathSegmentRect(int index) const
{
    if (index < 0 || index >= m_paths.size() || m_image.isNull()) {
        return {};
    }
    const MillingPathVM &path = m_paths.at(index);
    const QPointF a = mapImageToWidget(path.uStart, path.vStart);
    const QPointF b = mapImageToWidget(path.uEnd, path.vEnd);
    return QRectF(a, b).normalized().adjusted(-8.0, -8.0, 8.0, 8.0).toAlignedRect();
}

QRect ImageViewFrame::toolPoseDirtyRect() const
{
    if (!m_toolPoseValid || !m_toolPoseMapped || m_lastImageRect.isEmpty()
        || m_lastImageSize.isEmpty() || m_lastImageSize.width() <= 0
        || m_lastImageSize.height() <= 0) {
        return {};
    }

    const double sx = m_lastImageRect.width() / double(m_lastImageSize.width());
    const double sy = m_lastImageRect.height() / double(m_lastImageSize.height());
    QRect dirty;
    const auto addUv = [&](double u, double v, double pad) {
        const QPointF pt(m_lastImageRect.x() + u * sx, m_lastImageRect.y() + v * sy);
        dirty = dirty.united(QRectF(pt.x() - pad, pt.y() - pad, pad * 2.0, pad * 2.0).toAlignedRect());
    };
    addUv(m_toolPoseU, m_toolPoseV, 16.0);

    const int start = (m_toolTrailNext - m_toolTrailCount + kToolTrailCapacity) % kToolTrailCapacity;
    for (int i = 0; i < m_toolTrailCount; ++i) {
        const QPointF &uv = m_toolTrailUvRing[(start + i) % kToolTrailCapacity];
        addUv(uv.x(), uv.y(), 14.0);
    }
    if (m_hasActiveGrindPath && m_activeGrindPath.elementCount() > 0) {
        const int last = m_activeGrindPath.elementCount() - 1;
        const QPainterPath::Element end = m_activeGrindPath.elementAt(last);
        addUv(end.x, end.y, 6.0);
        if (last > 0) {
            const QPainterPath::Element prev = m_activeGrindPath.elementAt(last - 1);
            const QPointF a(m_lastImageRect.x() + prev.x * sx, m_lastImageRect.y() + prev.y * sy);
            const QPointF b(m_lastImageRect.x() + end.x * sx, m_lastImageRect.y() + end.y * sy);
            dirty = dirty.united(QRectF(a, b).normalized().adjusted(-6.0, -6.0, 6.0, 6.0).toAlignedRect());
        }
    }
    return dirty;
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
    applyZoom(newZoom, event->posF());
    event->accept();
}

void ImageViewFrame::applyZoom(double newZoom, const QPointF &anchorHint)
{
    const QRectF oldRect = currentImageRect();
    if (oldRect.isEmpty()) {
        return;
    }
    newZoom = qBound(kMinZoom, newZoom, kMaxZoom);
    if (qFuzzyCompare(newZoom, m_zoomFactor)) {
        return;
    }

    QPointF anchor = anchorHint;
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
    noteInteraction();
    m_viewCacheDirty = true;
    update();
    refreshDynamic();
}

void ImageViewFrame::zoomIn()
{
    if (m_image.isNull()) {
        return;
    }
    applyZoom(m_zoomFactor * kZoomStep * kZoomStep, currentImageRect().center());
}

void ImageViewFrame::zoomOut()
{
    if (m_image.isNull()) {
        return;
    }
    applyZoom(m_zoomFactor / (kZoomStep * kZoomStep), currentImageRect().center());
}

void ImageViewFrame::fitToView()
{
    m_zoomFactor = kMinZoom;
    m_panOffset = QPointF();
    clampPanOffset();
    m_viewCacheDirty = true;
    update();
    refreshDynamic();
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
            noteInteraction();
            m_lastDragPosition = event->pos();
            m_hoveredPathIndex = -1;
            m_tooltipPathIndex = -1;
            m_hoverCoordinateValid = false;
            m_hoverCoordinateText.clear();
            QToolTip::hideText();
            setCursor(Qt::ClosedHandCursor);
            m_viewCacheDirty = true;
            update();
            refreshDynamic();
        }
        event->accept();
        return;
    }

    updateHoverCoordinate(event->pos());
    const int idx = hitTestPath(event->pos());
    if (idx != m_hoveredPathIndex) {
        const int previous = m_hoveredPathIndex;
        m_hoveredPathIndex = idx;
        invalidateDynamic(pathSegmentRect(previous));
        invalidateDynamic(pathSegmentRect(idx));
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
        m_viewCacheDirty = true;
        update();
        refreshDynamic();
    }

    if (wasDrag) {
        event->accept();
        return;
    }

    int idx = hitTestPath(event->pos());
    if (idx < 0) {
        // 没精确压在路径线上时，按粒子半径再找一次，方便直接点粒子选中。
        idx = hitTestPathInRange(event->pos(), kParticleClickThresholdPx);
    }
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
    const QRect area = imageViewportRect().toAlignedRect();
    const QRect oldBox = (m_hoverCoordinateValid && !m_hoverCoordinateText.isEmpty())
        ? hoverBoxRect(m_hoverWidgetPos, m_hoverCoordinateText, area)
        : QRect();
    const int oldPath = m_hoveredPathIndex;
    m_hoveredPathIndex = -1;
    m_tooltipPathIndex = -1;
    m_hoverCoordinateValid = false;
    m_hoverCoordinateText.clear();
    QToolTip::hideText();
    invalidateDynamic(oldBox);
    invalidateDynamic(pathSegmentRect(oldPath));
}
