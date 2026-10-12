#include "ui/panels/ImageDisplayPanel.h"

#include "ElaPushButton.h"
#include "vision/HeightMap3DFrame.h"
#include "camera/HikLiveView.h"
#include "vision/ImageViewFrame.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>

ImageDisplayPanel::ImageDisplayPanel(QWidget *parent)
    : QFrame(parent)
{
    auto *viewToolbar = new QWidget(this);
    auto *viewToolbarLayout = new QHBoxLayout(viewToolbar);
    viewToolbarLayout->setContentsMargins(0, 0, 0, 0);
    viewToolbarLayout->setSpacing(6);

    // 2D 优先显示 Intensity；3D 使用 Range 生成高度曲面。
    m_view2dButton = new ElaPushButton("2D", viewToolbar);
    m_view2dButton->setFixedSize(66, 28);
    applyPrimaryButtonStyle(m_view2dButton);

    m_view3dButton = new ElaPushButton("3D", viewToolbar);
    m_view3dButton->setFixedSize(66, 28);
    applySecondaryButtonStyle(m_view3dButton);

    m_liveViewButton = new ElaPushButton(QStringLiteral("实时画面"), viewToolbar);
    m_liveViewButton->setFixedSize(90, 28);
    applySecondaryButtonStyle(m_liveViewButton);

    m_algorithmImageButton = new ElaPushButton(QStringLiteral("算法图像"), viewToolbar);
    m_algorithmImageButton->setFixedSize(90, 28);
    applySecondaryButtonStyle(m_algorithmImageButton);

    auto *viewModeGroup = new QButtonGroup(this);
    viewModeGroup->setExclusive(true);
    m_view2dButton->setCheckable(true);
    m_view3dButton->setCheckable(true);
    m_liveViewButton->setCheckable(true);
    m_algorithmImageButton->setCheckable(true);
    viewModeGroup->addButton(m_view2dButton);
    viewModeGroup->addButton(m_view3dButton);
    viewModeGroup->addButton(m_liveViewButton);
    viewModeGroup->addButton(m_algorithmImageButton);
    m_view2dButton->setChecked(true);

    viewToolbarLayout->addWidget(m_view2dButton);
    viewToolbarLayout->addWidget(m_view3dButton);
    viewToolbarLayout->addWidget(m_liveViewButton);
    viewToolbarLayout->addWidget(m_algorithmImageButton);

    auto *panel = createPanel("图像显示", viewToolbar, this);
    panel->setFixedWidth(780);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    m_viewStack = new QStackedWidget(panel);
    m_viewStack->setFixedSize(720, 600);
    m_viewStack->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_imageArea = new ImageViewFrame(m_viewStack);
    m_imageArea->setObjectName("imageArea");
    m_imageArea->setFixedSize(720, 600);
    m_imageArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_imageArea->setStyleSheet(QString(R"(
        QFrame#imageArea {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(Theme::kCanvasBg, Theme::kBorder));

    m_viewStack->addWidget(m_imageArea);

    m_heightMapArea = new HeightMap3DFrame(m_viewStack);
    m_heightMapArea->setObjectName("heightMap3dArea");
    m_heightMapArea->setFixedSize(720, 600);
    m_heightMapArea->setStyleSheet(QString(R"(
        QOpenGLWidget#heightMap3dArea {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(Theme::kCanvasBg, Theme::kBorder));
    m_viewStack->addWidget(m_heightMapArea);

    m_liveViewArea = new HikLiveView(m_viewStack);
    m_liveViewArea->setFixedSize(720, 600);
    m_liveViewArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_viewStack->addWidget(m_liveViewArea);

    m_algorithmImageArea = new ImageViewFrame(m_viewStack);
    m_algorithmImageArea->setObjectName("algorithmImageArea");
    m_algorithmImageArea->setFixedSize(720, 600);
    m_algorithmImageArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_algorithmImageArea->setStyleSheet(QString(R"(
        QFrame#algorithmImageArea {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(Theme::kCanvasBg, Theme::kBorder));
    m_viewStack->addWidget(m_algorithmImageArea);
    panelLayout->addWidget(m_viewStack, 0, Qt::AlignHCenter);

    connect(m_view2dButton, &ElaPushButton::clicked, this, [this]() {
        setViewMode(ViewMode::View2D);
    });
    connect(m_view3dButton, &ElaPushButton::clicked, this, [this]() {
        setViewMode(ViewMode::Height3D);
    });
    connect(m_liveViewButton, &ElaPushButton::clicked, this, [this]() {
        setViewMode(ViewMode::LiveVideo);
    });
    connect(m_algorithmImageButton, &ElaPushButton::clicked, this, [this]() {
        setViewMode(ViewMode::AlgorithmImage);
    });
    connect(m_imageArea, &ImageViewFrame::millingPathClicked,
            this, &ImageDisplayPanel::millingPathClicked);
}

void ImageDisplayPanel::setMillingPaths(const QVector<MillingPathVM> &paths)
{
    if (m_imageArea) {
        m_imageArea->setMillingPaths(paths);
    }
}

void ImageDisplayPanel::clearMillingPaths()
{
    if (m_imageArea) {
        m_imageArea->clearMillingPaths();
    }
}

void ImageDisplayPanel::setToolPose(double xMm, double yMm, double zMm, bool valid)
{
    if (m_imageArea) {
        m_imageArea->setToolPose(xMm, yMm, zMm, valid);
    }
}

void ImageDisplayPanel::setPathProgress(int completedPath, int currentPath)
{
    if (m_imageArea) {
        m_imageArea->setPathProgress(completedPath, currentPath);
    }
}

void ImageDisplayPanel::clearToolPose()
{
    if (m_imageArea) {
        m_imageArea->clearToolPose();
    }
}

void ImageDisplayPanel::setAlgorithmImagePath(const QString &path)
{
    m_algorithmImagePath = path.trimmed();
    m_algorithmImage = QImage();
    if (!m_algorithmImagePath.isEmpty()) {
        QImageReader reader(m_algorithmImagePath);
        reader.setAutoDetectImageFormat(true);
        const QString suffix = QFileInfo(m_algorithmImagePath).suffix().toLower();
        if (suffix.isEmpty() || suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
            reader.setFormat("jpeg");
        }
        m_algorithmImage = reader.read();
        if (m_algorithmImage.isNull()) {
            QImageReader fallback(m_algorithmImagePath, "jpeg");
            m_algorithmImage = fallback.read();
        }
    }
    if (m_mode == ViewMode::AlgorithmImage) {
        refreshView();
    }
}

void ImageDisplayPanel::clearAlgorithmImage()
{
    setAlgorithmImagePath(QString());
}

QString ImageDisplayPanel::currentViewModeName() const
{
    switch (m_mode) {
    case ViewMode::View2D:
        return QStringLiteral("2D");
    case ViewMode::Height3D:
        return QStringLiteral("3D");
    case ViewMode::LiveVideo:
        return QStringLiteral("实时画面");
    case ViewMode::AlgorithmImage:
        return QStringLiteral("算法图像");
    }
    return QStringLiteral("未知");
}

void ImageDisplayPanel::updateScanFrame(const PointCloudFrame &frame)
{
    // 先清空上一帧，再显示新的相机图像。
    m_pointCloudFrame = PointCloudFrame();
    m_rangeImage = QImage();
    m_intensityImage = QImage();
    m_lastFrameId = 0;
    m_lastTimestampNs = 0;
    if (m_imageArea) {
        m_imageArea->clearImage();
        m_imageArea->setOverlayText(QString());
    }

    m_pointCloudFrame = frame;
    m_rangeImage = frame.rangePreview;
    m_intensityImage = frame.intensity;
    m_lastFrameId = frame.frameId;
    m_lastTimestampNs = frame.timestampNs;
    // 3D 一次写入即可（内部会清旧点再建），避免隐藏控件时先清后写导致 GPU 缓冲卡住。
    if (m_heightMapArea) {
        m_heightMapArea->setPointCloudFrame(frame);
    }

    // 新扫图到达时，若停在实时/算法页，自动切回 2D，避免“有帧但看不见”。
    if (m_mode == ViewMode::LiveVideo || m_mode == ViewMode::AlgorithmImage) {
        if (!m_rangeImage.isNull() || !m_intensityImage.isNull()) {
            setViewMode(ViewMode::View2D);
            return;
        }
    }

    // 3D 依赖 Range；仅有 Intensity 时回到可显示的 2D 模式。
    if (m_mode == ViewMode::Height3D && m_rangeImage.isNull() && !m_intensityImage.isNull()) {
        setViewMode(ViewMode::View2D);
        return;
    }
    refreshView();
}

void ImageDisplayPanel::showHeight3D()
{
    setViewMode(ViewMode::Height3D);
}

void ImageDisplayPanel::show2D()
{
    setViewMode(ViewMode::View2D);
}

void ImageDisplayPanel::setLiveCameraStatus(bool connected, const QString &message)
{
    if (m_liveViewArea) {
        m_liveViewArea->setConnectionStatus(connected, message);
    }
}

void ImageDisplayPanel::setViewMode(ViewMode mode)
{
    if (m_mode == mode) {
        return;
    }

    const bool leavingLiveView = m_mode == ViewMode::LiveVideo;
    m_mode = mode;
    applyViewButtonStyles();
    refreshView();

    if (leavingLiveView) {
        emit liveViewRequested(false, WId());
    }
    if (m_mode == ViewMode::LiveVideo && m_liveViewArea) {
        m_liveViewArea->setConnectionStatus(false, QStringLiteral("正在连接海康相机…"));
        emit liveViewRequested(true, m_liveViewArea->videoWindowId());
    }
}

void ImageDisplayPanel::applyViewButtonStyles()
{
    const bool show2D = m_mode == ViewMode::View2D;
    const bool show3D = m_mode == ViewMode::Height3D;
    const bool showLive = m_mode == ViewMode::LiveVideo;
    const bool showAlgorithm = m_mode == ViewMode::AlgorithmImage;
    m_view2dButton->setChecked(show2D);
    m_view3dButton->setChecked(show3D);
    m_liveViewButton->setChecked(showLive);
    m_algorithmImageButton->setChecked(showAlgorithm);

    show2D ? applyPrimaryButtonStyle(m_view2dButton)
           : applySecondaryButtonStyle(m_view2dButton);
    show3D ? applyPrimaryButtonStyle(m_view3dButton)
           : applySecondaryButtonStyle(m_view3dButton);
    showLive ? applyPrimaryButtonStyle(m_liveViewButton)
             : applySecondaryButtonStyle(m_liveViewButton);
    showAlgorithm ? applyPrimaryButtonStyle(m_algorithmImageButton)
                  : applySecondaryButtonStyle(m_algorithmImageButton);
    // ElaPushButton 的颜色属性变化不会自动请求重绘，需显式刷新外观。
    m_view2dButton->update();
    m_view3dButton->update();
    m_liveViewButton->update();
    m_algorithmImageButton->update();
}

void ImageDisplayPanel::refreshView()
{
    if (m_mode == ViewMode::LiveVideo) {
        if (m_viewStack && m_liveViewArea) {
            m_viewStack->setCurrentWidget(m_liveViewArea);
        }
        return;
    }
    if (m_mode == ViewMode::Height3D) {
        if (m_viewStack && m_heightMapArea) {
            m_viewStack->setCurrentWidget(m_heightMapArea);
            // 切回 3D 时强制重绘；若上次在后台更新失败，paintGL 会补上传。
            m_heightMapArea->update();
        }
        return;
    }
    if (m_mode == ViewMode::AlgorithmImage) {
        if (m_viewStack && m_algorithmImageArea) {
            m_viewStack->setCurrentWidget(m_algorithmImageArea);
            m_algorithmImageArea->setImage(m_algorithmImage);
            if (m_algorithmImage.isNull()) {
                m_algorithmImageArea->setOverlayText(m_algorithmImagePath.isEmpty()
                    ? QStringLiteral("算法图像  未收到")
                    : QStringLiteral("算法图像  无法读取  %1").arg(m_algorithmImagePath));
            } else {
                const QString fileName = QFileInfo(m_algorithmImagePath).fileName();
                m_algorithmImageArea->setOverlayText(
                    QStringLiteral("算法图像  %1  %2x%3")
                        .arg(fileName.isEmpty() ? m_algorithmImagePath : fileName)
                        .arg(m_algorithmImage.width())
                        .arg(m_algorithmImage.height()));
            }
        }
        return;
    }

    if (m_viewStack && m_imageArea) {
        m_viewStack->setCurrentWidget(m_imageArea);
    }
    const bool showingIntensity = !m_intensityImage.isNull();
    const QImage &shown = showingIntensity ? m_intensityImage : m_rangeImage;
    m_imageArea->setImage(shown);
    m_imageArea->setMetricFrame(m_pointCloudFrame);

    if (shown.isNull()) {
        m_imageArea->setOverlayText(QString());
        return;
    }

    const QString kind = showingIntensity ? QStringLiteral("强度图")
                                          : QStringLiteral("深度图");
    QString overlay = QString("%1  frame=%2  %3x%4")
                          .arg(kind)
                          .arg(m_lastFrameId)
                          .arg(shown.width())
                          .arg(shown.height());
    if (m_lastTimestampNs != 0) {
        const qint64 ms = qint64(m_lastTimestampNs / 1000000ULL);
        const QString ts = QDateTime::fromMSecsSinceEpoch(ms).toString("HH:mm:ss.zzz");
        overlay += QString("  %1").arg(ts);
    }
    // 同文案也刷新一次，避免早退导致用户误以为画面未更新。
    m_imageArea->setOverlayText(QString());
    m_imageArea->setOverlayText(overlay);
}
