#include "vision/HeightMap3DFrame.h"

#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"

#include <QColor>
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QVector>
#include <QVector3D>
#include <QExposeEvent>
#include <QFont>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLWindow>
#include <QPainter>
#include <QPalette>
#include <QSettings>
#include <QSizePolicy>
#include <QSurfaceFormat>
#include <QVBoxLayout>
#include <QVector4D>
#include <QWheelEvent>

#include <memory>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

constexpr int kMaxDisplayPoints = 48000;
constexpr float kMinPitch = -1.45f;
constexpr float kMaxPitch = 1.45f;
constexpr float kHeightColorSpanMm = 8.0f;
constexpr int kZHistogramBins = 80;

bool isMeasuredValue(float value, float missingValue)
{
    return std::isfinite(value) && value != missingValue;
}

struct HeightMapDisplayConfig
{
    bool flipX{false};
    int cropLeftColumns{120};
    int cropRightColumns{120};
};

HeightMapDisplayConfig loadHeightMapDisplayConfig()
{
    HeightMapDisplayConfig config;
    const QString configPath = displayIniPath();
    if (configPath.isEmpty()) {
        return config;
    }

    QSettings settings(configPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    config.flipX = settings.value(QStringLiteral("HeightMap3D/FlipX"), 0).toInt() == 1;
    config.cropLeftColumns = std::max(
        0, settings.value(QStringLiteral("HeightMap3D/CropLeftColumns"), 120).toInt());
    config.cropRightColumns = std::max(
        0, settings.value(QStringLiteral("HeightMap3D/CropRightColumns"), 120).toInt());
    return config;
}

QVector3D unproject(const QPointF &position,
                    float depth,
                    const QMatrix4x4 &inverse,
                    int width,
                    int height)
{
    const float x = float(2.0 * position.x() / std::max(1, width) - 1.0);
    const float y = float(1.0 - 2.0 * position.y() / std::max(1, height));
    const QVector4D world = inverse * QVector4D(x, y, depth * 2.0f - 1.0f, 1.0f);
    return qFuzzyIsNull(world.w()) ? world.toVector3D() : world.toVector3DAffine();
}

} // namespace

class HeightMap3DFrame::Window : public QOpenGLWindow, protected QOpenGLFunctions
{
public:
    Window();
    ~Window() override;

    void setPointCloudFrame(const PointCloudFrame &frame);
    void clearPointCloud();

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void exposeEvent(QExposeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct Vertex
    {
        float x{0.0f};
        float y{0.0f};
        float z{0.0f};
        float heightNorm{0.0f};
    };

    QRect viewRect() const;
    void rebuildVertices();
    void uploadVertices();
    void resetView();
    float fittedDistance() const;
    QMatrix4x4 currentViewProjection() const;
    QVector3D cameraPosition() const;
    QVector3D cursorAnchor(const QPointF &position);
    QVector3D targetPlaneAnchor(const QPointF &position) const;

    PointCloudFrame m_frame;
    QVector<Vertex> m_vertices;
    QOpenGLBuffer m_vertexBuffer{QOpenGLBuffer::VertexBuffer};
    std::unique_ptr<QOpenGLShaderProgram> m_program;
    QMatrix4x4 m_lastViewProjection;
    QVector3D m_boundsMin;
    QVector3D m_boundsMax;
    QVector3D m_target;
    quint64 m_validPointCount{0};
    float m_sceneDiagonal{1.0f};
    float m_distance{1.0f};
    float m_yaw{-0.78f};
    float m_pitch{0.62f};
    QPoint m_lastMousePosition;
    QString m_renderError;
    bool m_dragging{false};
    bool m_userAdjustedView{false};
    bool m_glReady{false};
    bool m_uploadPending{false};
    bool m_flipX{false};
    int m_cropLeftColumns{0};
    int m_cropRightColumns{0};
};

HeightMap3DFrame::Window::Window()
    : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate)
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(0);
    format.setSamples(0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(0);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    setFormat(format);
    const HeightMapDisplayConfig displayConfig = loadHeightMapDisplayConfig();
    m_flipX = displayConfig.flipX;
    m_cropLeftColumns = displayConfig.cropLeftColumns;
    m_cropRightColumns = displayConfig.cropRightColumns;
    Theme::watch(this, [this]() { requestUpdate(); });
    setCursor(Qt::ArrowCursor);
}

HeightMap3DFrame::Window::~Window()
{
    if (m_glReady) {
        makeCurrent();
        m_vertexBuffer.destroy();
        m_program.reset();
        doneCurrent();
    }
}

void HeightMap3DFrame::Window::setPointCloudFrame(const PointCloudFrame &frame)
{
    m_frame = frame;
    rebuildVertices();
    resetView();
    // QStackedWidget 里隐藏时 makeCurrent/upload 经常无效，标记待上传，
    // 等 paintGL / 再次显示时再灌 GPU。
    m_uploadPending = true;
    if (m_glReady && isVisible()) {
        makeCurrent();
        uploadVertices();
        doneCurrent();
    }
    update();
}

void HeightMap3DFrame::Window::clearPointCloud()
{
    setPointCloudFrame(PointCloudFrame());
}

void HeightMap3DFrame::Window::initializeGL()
{
    initializeOpenGLFunctions();
    m_glReady = true;
    const QColor canvasColor(Theme::kCanvasBg);
    glClearColor(canvasColor.redF(), canvasColor.greenF(), canvasColor.blueF(), 1.0f);
    glEnable(GL_DEPTH_TEST);
#ifdef GL_PROGRAM_POINT_SIZE
    glEnable(GL_PROGRAM_POINT_SIZE);
#endif

    m_program = std::make_unique<QOpenGLShaderProgram>();
    static const char vertexShader[] = R"(
        attribute highp vec3 aPosition;
        attribute lowp float aHeight;
        uniform highp mat4 uMvp;
        varying lowp float vHeight;
        void main()
        {
            gl_Position = uMvp * vec4(aPosition, 1.0);
            gl_PointSize = 2.0;
            vHeight = aHeight;
        }
    )";
    static const char fragmentShader[] = R"(
        #ifdef GL_ES
        precision mediump float;
        #endif
        varying lowp float vHeight;
        vec3 heightColor(float t)
        {
            t = clamp(t, 0.0, 1.0);
            vec3 c0 = vec3(0.10, 0.28, 0.62);
            vec3 c1 = vec3(0.00, 0.78, 0.82);
            vec3 c2 = vec3(0.35, 0.92, 0.28);
            vec3 c3 = vec3(1.00, 0.80, 0.12);
            vec3 c4 = vec3(0.96, 0.16, 0.14);
            float x = t * 4.0;
            if (x < 1.0) return mix(c0, c1, x);
            if (x < 2.0) return mix(c1, c2, x - 1.0);
            if (x < 3.0) return mix(c2, c3, x - 2.0);
            return mix(c3, c4, x - 3.0);
        }
        void main()
        {
            gl_FragColor = vec4(heightColor(vHeight), 1.0);
        }
    )";

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader) ||
        !m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader) ||
        !m_program->link()) {
        m_renderError = QStringLiteral("OpenGL 着色器初始化失败：%1").arg(m_program->log());
        m_program.reset();
        return;
    }

    if (!m_vertexBuffer.create()) {
        m_renderError = QStringLiteral("无法创建 OpenGL 点云缓冲区");
        return;
    }

    if (m_uploadPending || !m_vertices.isEmpty()) {
        uploadVertices();
    }
}

void HeightMap3DFrame::Window::resizeGL(int, int)
{
    if (!m_userAdjustedView && m_validPointCount > 0) {
        m_distance = fittedDistance();
    }
}

void HeightMap3DFrame::Window::paintGL()
{
    if (m_uploadPending && m_vertexBuffer.isCreated()) {
        uploadVertices();
    }

    const bool light = Theme::current() == Theme::Id::Light;
    const QColor backdrop(Theme::kCanvasBg);
    const bool hasCloud = m_program && m_vertexBuffer.isCreated() && !m_vertices.isEmpty();

    auto drawCloud = [this]() {
        if (!m_program || !m_vertexBuffer.isCreated() || m_vertices.isEmpty()) {
            return;
        }
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
#ifdef GL_PROGRAM_POINT_SIZE
        glEnable(GL_PROGRAM_POINT_SIZE);
#endif
        m_lastViewProjection = currentViewProjection();
        m_program->bind();
        m_program->setUniformValue("uMvp", m_lastViewProjection);
        m_vertexBuffer.bind();

        const int positionLocation = m_program->attributeLocation("aPosition");
        const int heightLocation = m_program->attributeLocation("aHeight");
        m_program->enableAttributeArray(positionLocation);
        m_program->setAttributeBuffer(positionLocation,
                                      GL_FLOAT,
                                      int(offsetof(Vertex, x)),
                                      3,
                                      int(sizeof(Vertex)));
        m_program->enableAttributeArray(heightLocation);
        m_program->setAttributeBuffer(heightLocation,
                                      GL_FLOAT,
                                      int(offsetof(Vertex, heightNorm)),
                                      1,
                                      int(sizeof(Vertex)));

        glDrawArrays(GL_POINTS, 0, m_vertices.size());

        m_program->disableAttributeArray(positionLocation);
        m_program->disableAttributeArray(heightLocation);
        m_vertexBuffer.release();
        m_program->release();
    };

    if (m_dragging && hasCloud) {
        glClearColor(backdrop.redF(), backdrop.greenF(), backdrop.blueF(), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawCloud();
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(viewRect(), backdrop);
    if (light) {
        const QRect area = viewRect().adjusted(1, 1, -2, -2);
        painter.setPen(QPen(QColor(Theme::kDivider), 1));
        constexpr int gridSize = 32;
        for (int x = area.left() + gridSize; x < area.right(); x += gridSize) {
            painter.drawLine(x, area.top(), x, area.bottom());
        }
        for (int y = area.top() + gridSize; y < area.bottom(); y += gridSize) {
            painter.drawLine(area.left(), y, area.right(), y);
        }
    }
    painter.setPen(QPen(QColor(Theme::kBorder), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(viewRect().adjusted(0, 0, -1, -1));

    if (hasCloud) {
        painter.beginNativePainting();
        drawCloud();
        painter.endNativePainting();
    }

    if (!hasCloud && m_renderError.isEmpty()) {
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        QFont font = painter.font();
        font.setPixelSize(13);
        painter.setFont(font);
        painter.setPen(QColor(Theme::kTextMuted));
        painter.drawText(viewRect().adjusted(12, 12, -12, -12),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         QStringLiteral("无点云"));
        return;
    }

    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (!m_renderError.isEmpty()) {
        painter.setPen(QColor(Theme::kTextMuted));
        painter.drawText(viewRect().adjusted(20, 20, -20, -20),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         m_renderError);
        return;
    }

    const QRect barRect(12, viewRect().bottom() - 116, 10, 96);
    QLinearGradient gradient(barRect.bottomLeft(), barRect.topLeft());
    gradient.setColorAt(0.00, QColor(26, 71, 158));
    gradient.setColorAt(0.25, QColor(0, 199, 209));
    gradient.setColorAt(0.50, QColor(89, 235, 71));
    gradient.setColorAt(0.75, QColor(255, 204, 31));
    gradient.setColorAt(1.00, QColor(245, 41, 36));
    painter.fillRect(barRect, gradient);
    painter.setPen(QColor(Theme::kBorder));
    painter.drawRect(barRect.adjusted(0, 0, -1, -1));
}

void HeightMap3DFrame::Window::rebuildVertices()
{
    m_vertices.clear();
    m_validPointCount = 0;
    m_sceneDiagonal = 1.0f;
    m_renderError.clear();

    if (!m_frame.hasFloatRange()) {
        return;
    }

    const int width = int(m_frame.width);
    const int height = int(m_frame.height);
    const float *range = reinterpret_cast<const float *>(m_frame.rangeFloat32.constData());
    const bool hasEncoder = m_frame.encoderResolutionMm > 0.0 &&
        m_frame.encoderValues.size() == height;
    const qint64 firstEncoder = hasEncoder ? qint64(m_frame.encoderValues.first()) : 0;

    const int cropLeft = std::clamp(m_cropLeftColumns, 0, std::max(0, width - 1));
    const int cropRight = std::clamp(m_cropRightColumns, 0, std::max(0, width - 1));
    int columnBegin = cropLeft;
    int columnEnd = width - cropRight;
    if (columnBegin >= columnEnd) {
        columnBegin = 0;
        columnEnd = width;
    }

    QVector<float> rowY(height);
    for (int row = 0; row < height; ++row) {
        const double y = hasEncoder
            ? m_frame.yOffsetMm +
                double(qint64(m_frame.encoderValues[row]) - firstEncoder) *
                    m_frame.encoderResolutionMm
            : m_frame.yOffsetMm + double(row) * m_frame.yScaleMm;
        rowY[row] = float(y);
    }

    const float infinity = std::numeric_limits<float>::infinity();
    m_boundsMin = QVector3D(infinity, infinity, infinity);
    m_boundsMax = QVector3D(-infinity, -infinity, -infinity);
    for (int row = 0; row < height; ++row) {
        const float y = rowY[row];
        const float *rangeRow = range + std::size_t(row) * width;
        for (int column = columnBegin; column < columnEnd; ++column) {
            const float z = rangeRow[column];
            if (!isMeasuredValue(z, m_frame.missingValue)) {
                continue;
            }
            const float x = float(m_frame.xOffsetMm +
                                  double(m_flipX ? (width - 1 - column) : column) *
                                      m_frame.xScaleMm);
            m_boundsMin.setX(std::min(m_boundsMin.x(), x));
            m_boundsMin.setY(std::min(m_boundsMin.y(), y));
            m_boundsMin.setZ(std::min(m_boundsMin.z(), z));
            m_boundsMax.setX(std::max(m_boundsMax.x(), x));
            m_boundsMax.setY(std::max(m_boundsMax.y(), y));
            m_boundsMax.setZ(std::max(m_boundsMax.z(), z));
            ++m_validPointCount;
        }
    }

    if (m_validPointCount == 0) {
        m_renderError = QStringLiteral("Range 帧中没有有效测量点");
        return;
    }

    const int usableWidth = columnEnd - columnBegin;
    const double sampleRatio = double(usableWidth) * double(height) / kMaxDisplayPoints;
    const int sampleStep = std::max(1, int(std::ceil(std::sqrt(sampleRatio))));

    float zPlate = m_boundsMin.z();
    const float zRange = m_boundsMax.z() - m_boundsMin.z();
    if (zRange > 0.05f) {
        int bins[kZHistogramBins] = {};
        int bestBin = 0;
        for (int row = 0; row < height; row += sampleStep) {
            const float *rangeRow = range + std::size_t(row) * width;
            for (int column = columnBegin; column < columnEnd; column += sampleStep) {
                const float z = rangeRow[column];
                if (!isMeasuredValue(z, m_frame.missingValue)) {
                    continue;
                }
                int bin = int((z - m_boundsMin.z()) / zRange * float(kZHistogramBins - 1));
                bin = std::clamp(bin, 0, kZHistogramBins - 1);
                ++bins[bin];
                if (bins[bin] > bins[bestBin]) {
                    bestBin = bin;
                }
            }
        }
        zPlate = m_boundsMin.z() + (float(bestBin) + 0.5f) / float(kZHistogramBins) * zRange;
    }

    m_vertices.reserve(std::min(kMaxDisplayPoints,
                                ((usableWidth + sampleStep - 1) / sampleStep) *
                                    ((height + sampleStep - 1) / sampleStep)));

    for (int row = 0; row < height; row += sampleStep) {
        const float y = rowY[row];
        const float *rangeRow = range + std::size_t(row) * width;
        for (int column = columnBegin; column < columnEnd; column += sampleStep) {
            const float z = rangeRow[column];
            if (!isMeasuredValue(z, m_frame.missingValue)) {
                continue;
            }
            Vertex vertex;
            vertex.x = float(m_frame.xOffsetMm +
                             double(m_flipX ? (width - 1 - column) : column) *
                                 m_frame.xScaleMm);
            vertex.y = y;
            vertex.z = z;
            vertex.heightNorm = std::clamp((z - zPlate) / kHeightColorSpanMm, 0.0f, 1.0f);
            m_vertices.push_back(vertex);
        }
    }

    m_sceneDiagonal = std::max(1.0f, (m_boundsMax - m_boundsMin).length());
    m_uploadPending = true;
}

void HeightMap3DFrame::Window::uploadVertices()
{
    if (!m_vertexBuffer.isCreated()) {
        m_uploadPending = true;
        return;
    }
    m_vertexBuffer.bind();
    m_vertexBuffer.allocate(m_vertices.constData(), int(m_vertices.size() * sizeof(Vertex)));
    m_vertexBuffer.release();
    m_uploadPending = false;
}

float HeightMap3DFrame::Window::fittedDistance() const
{
    const float radius = std::max(1.0f, m_sceneDiagonal * 0.5f);
    const float aspect = float(std::max(1, width())) / float(std::max(1, height()));
    constexpr float kHalfFovY = 42.0f * 0.5f * 3.14159265f / 180.0f;
    const float halfX = std::atan(std::tan(kHalfFovY) * std::max(0.25f, aspect));
    const float limiting = std::min(kHalfFovY, halfX);
    return radius / std::sin(limiting) * 1.28f;
}

void HeightMap3DFrame::Window::resetView()
{
    m_target = m_validPointCount == 0
        ? QVector3D()
        : (m_boundsMin + m_boundsMax) * 0.5f;
    m_userAdjustedView = false;
    m_distance = fittedDistance();
    m_yaw = -0.78f;
    m_pitch = 0.62f;
    m_dragging = false;
    setCursor(m_vertices.isEmpty() ? Qt::ArrowCursor : Qt::OpenHandCursor);
    update();
}

QVector3D HeightMap3DFrame::Window::cameraPosition() const
{
    const float horizontal = std::cos(m_pitch);
    const QVector3D direction(horizontal * std::cos(m_yaw),
                              horizontal * std::sin(m_yaw),
                              std::sin(m_pitch));
    return m_target + direction * m_distance;
}

QMatrix4x4 HeightMap3DFrame::Window::currentViewProjection() const
{
    QMatrix4x4 projection;
    projection.perspective(42.0f,
                           float(std::max(1, width())) / float(std::max(1, height())),
                           std::max(0.01f, m_sceneDiagonal * 0.0005f),
                           std::max(100.0f, m_sceneDiagonal * 30.0f));

    QMatrix4x4 view;
    view.lookAt(cameraPosition(), m_target, QVector3D(0.0f, 0.0f, 1.0f));
    return projection * view;
}

QVector3D HeightMap3DFrame::Window::cursorAnchor(const QPointF &position)
{
    if (!m_glReady || m_vertices.isEmpty()) {
        return targetPlaneAnchor(position);
    }

    float depth = 1.0f;
    const qreal ratio = devicePixelRatio();
    const int framebufferWidth = std::max(1, int(std::round(width() * ratio)));
    const int framebufferHeight = std::max(1, int(std::round(height() * ratio)));
    const int pixelX = std::clamp(int(std::round(position.x() * ratio)),
                                  0,
                                  framebufferWidth - 1);
    const int pixelY = std::clamp(framebufferHeight - 1 -
                                      int(std::round(position.y() * ratio)),
                                  0,
                                  framebufferHeight - 1);
    makeCurrent();
    glReadPixels(pixelX,
                 pixelY,
                 1,
                 1,
                 GL_DEPTH_COMPONENT,
                 GL_FLOAT,
                 &depth);
    doneCurrent();

    bool invertible = false;
    const QMatrix4x4 inverse = m_lastViewProjection.inverted(&invertible);
    if (invertible && depth >= 0.0f && depth < 0.9999f) {
        return unproject(position, depth, inverse, width(), height());
    }
    return targetPlaneAnchor(position);
}

QVector3D HeightMap3DFrame::Window::targetPlaneAnchor(const QPointF &position) const
{
    bool invertible = false;
    const QMatrix4x4 inverse = currentViewProjection().inverted(&invertible);
    if (!invertible) {
        return m_target;
    }

    const QVector3D nearPoint = unproject(position, 0.0f, inverse, width(), height());
    const QVector3D farPoint = unproject(position, 1.0f, inverse, width(), height());
    const QVector3D ray = farPoint - nearPoint;
    const QVector3D normal = (m_target - cameraPosition()).normalized();
    const float denominator = QVector3D::dotProduct(ray, normal);
    if (qFuzzyIsNull(denominator)) {
        return m_target;
    }
    const float distance = QVector3D::dotProduct(m_target - nearPoint, normal) / denominator;
    return nearPoint + ray * distance;
}

QRect HeightMap3DFrame::Window::viewRect() const
{
    return QRect(0, 0, std::max(1, width()), std::max(1, height()));
}

void HeightMap3DFrame::Window::exposeEvent(QExposeEvent *event)
{
    QOpenGLWindow::exposeEvent(event);
    if (isExposed() && m_uploadPending && m_glReady) {
        makeCurrent();
        uploadVertices();
        doneCurrent();
        requestUpdate();
    }
}

void HeightMap3DFrame::Window::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        resetView();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && !m_vertices.isEmpty()) {
        m_dragging = true;
        m_lastMousePosition = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
        QWindow::mousePressEvent(event);
}

void HeightMap3DFrame::Window::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->pos() - m_lastMousePosition;
        m_userAdjustedView = true;
        m_yaw -= float(delta.x()) * 0.010f;
        m_pitch = std::clamp(m_pitch + float(delta.y()) * 0.010f,
                             kMinPitch,
                             kMaxPitch);
        m_lastMousePosition = event->pos();
        requestUpdate();
        event->accept();
        return;
    }
    QWindow::mouseMoveEvent(event);
}

void HeightMap3DFrame::Window::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(m_vertices.isEmpty() ? Qt::ArrowCursor : Qt::OpenHandCursor);
        update();
        event->accept();
        return;
    }
        QWindow::mouseReleaseEvent(event);
}

void HeightMap3DFrame::Window::wheelEvent(QWheelEvent *event)
{
    if (m_vertices.isEmpty() || event->angleDelta().y() == 0) {
        QWindow::wheelEvent(event);
        return;
    }

    m_userAdjustedView = true;
    const QVector3D anchor = cursorAnchor(event->posF());
    const float steps = float(event->angleDelta().y()) / 120.0f;
    const float requestedFactor = std::pow(0.82f, steps);
    const float newDistance = std::clamp(m_distance * requestedFactor,
                                         m_sceneDiagonal * 0.005f,
                                         m_sceneDiagonal * 20.0f);
    const float actualFactor = newDistance / m_distance;
    m_target = anchor + (m_target - anchor) * actualFactor;
    m_distance = newDistance;
    update();
    event->accept();
}

HeightMap3DFrame::HeightMap3DFrame(QWidget *parent)
    : QWidget(parent)
{
    m_window = new Window();
    QWidget *container = QWidget::createWindowContainer(m_window, this);
    container->setFocusPolicy(Qt::StrongFocus);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(container);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);
    Theme::watch(this, [this]() {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(Theme::kCanvasBg));
        setPalette(pal);
    });
}

void HeightMap3DFrame::setPointCloudFrame(const PointCloudFrame &frame)
{
    m_window->setPointCloudFrame(frame);
}

void HeightMap3DFrame::clearPointCloud()
{
    m_window->clearPointCloud();
}
