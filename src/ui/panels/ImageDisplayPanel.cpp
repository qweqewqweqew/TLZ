#include "ui/panels/ImageDisplayPanel.h"

#include "ElaPushButton.h"
#include "camera/HikLiveView.h"
#include "vision/HeightMap3DFrame.h"
#include "vision/ImageViewFrame.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {

// 与 ImageViewFrame 里路径叠加的颜色保持一致
constexpr const char *kCutColor = "#D9A800";
constexpr const char *kMoveColor = "#27AE60";

const char *distributionCanvas()
{
    return Theme::current() == Theme::Id::Light ? "#F8FAFC" : Theme::kCanvasBg;
}

QLabel *legendItem(const QString &text, const QString &color, QWidget *parent)
{
    auto *label = new QLabel(QStringLiteral("● %1").arg(text), parent);
    label->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;border:none;").arg(color));
    label->setToolTip(text);
    return label;
}

} // namespace

ImageDisplayPanel::ImageDisplayPanel(QWidget *parent)
    : QFrame(parent)
{
    // ---- 卡头右侧：板号 + 图例 ----
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(14);
    m_boardLabel = new QLabel(header);
    m_boardLabel->setStyleSheet(QStringLiteral("color:%1;font-size:13px;background:%2;border:1px solid %3;"
                                               "border-radius:4px;padding:2px 10px;")
                                    .arg(Theme::kTextSecondary, Theme::kControlBg, Theme::kBorder));
    headerLayout->addWidget(m_boardLabel);
    headerLayout->addWidget(legendItem(QStringLiteral("切削路径"), kCutColor, header));
    headerLayout->addWidget(legendItem(QStringLiteral("空移"), kMoveColor, header));

    auto *panel = createPanel(QStringLiteral("铜板粒子分布"), header, this);
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);
    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());

    // ---- 工具栏：视图切换 | 缩放 ----
    auto *toolbar = new QWidget(panel);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(6);

    m_view2dButton = new ElaPushButton(QStringLiteral("粒子分布"), toolbar);
    m_liveViewButton = new ElaPushButton(QStringLiteral("实时画面"), toolbar);
    m_algorithmImageButton = new ElaPushButton(QStringLiteral("算法图像"), toolbar);
    m_pointCloudButton = new ElaPushButton(QStringLiteral("三维点云"), toolbar);
    auto *viewModeGroup = new QButtonGroup(this);
    viewModeGroup->setExclusive(true);
    for (ElaPushButton *button : {m_view2dButton, m_liveViewButton, m_algorithmImageButton, m_pointCloudButton}) {
        button->setFixedSize(84, 28);
        button->setCheckable(true);
        viewModeGroup->addButton(button);
        toolbarLayout->addWidget(button);
    }
    m_view2dButton->setChecked(true);

    toolbarLayout->addStretch(1);

    auto *zoomIn = makeToolButton(QStringLiteral(":/img/zoom_in.svg"), QStringLiteral("放大"), toolbar);
    auto *zoomOut = makeToolButton(QStringLiteral(":/img/zoom_out.svg"), QStringLiteral("缩小"), toolbar);
    auto *fitView = makeToolButton(QStringLiteral(":/img/fit_view.svg"), QStringLiteral("适应窗口"), toolbar);
    toolbarLayout->addWidget(zoomIn);
    toolbarLayout->addWidget(zoomOut);
    toolbarLayout->addWidget(fitView);
    panelLayout->addWidget(toolbar);

    // ---- 视图区 ----
    m_viewStack = new QStackedWidget(panel);
    m_viewStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_viewStack->setMinimumSize(480, 320);

    m_imageArea = new ImageViewFrame(m_viewStack);
    m_imageArea->setObjectName("imageArea");
    m_imageArea->setStyleSheet(QString(R"(
        QFrame#imageArea {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(distributionCanvas(), Theme::kBorder));
    m_viewStack->addWidget(m_imageArea);

    m_liveViewArea = new HikLiveView(m_viewStack);
    m_liveViewArea->setProperty("mztlzNoFade", true);
    m_viewStack->addWidget(m_liveViewArea);

    m_algorithmImageArea = new ImageViewFrame(m_viewStack);
    m_algorithmImageArea->setObjectName("algorithmImageArea");
    m_algorithmImageArea->setStyleSheet(QString(R"(
        QFrame#algorithmImageArea {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(distributionCanvas(), Theme::kBorder));
    m_viewStack->addWidget(m_algorithmImageArea);

    // 整板三维点云：HeightMap3DFrame 内嵌原生 OpenGL 窗口，
    // 不能参与 QGraphicsOpacityEffect 淡入，标记为不淡入页。
    m_pointCloudArea = new HeightMap3DFrame(m_viewStack);
    m_pointCloudArea->setProperty("mztlzNoFade", true);
    m_viewStack->addWidget(m_pointCloudArea);
    panelLayout->addWidget(m_viewStack, 1);

    // ---- 页脚：任务摘要 ----
    auto *footer = new QWidget(panel);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(2, 0, 2, 0);
    footerLayout->setSpacing(8);
    m_footerLeft = new QLabel(footer);
    m_footerRight = new QLabel(footer);
    for (QLabel *label : {m_footerLeft, m_footerRight}) {
        label->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;border:none;")
                                 .arg(Theme::kTextSecondary));
    }
    footerLayout->addWidget(m_footerLeft);
    footerLayout->addStretch(1);
    footerLayout->addWidget(m_footerRight);
    panelLayout->addWidget(footer);

    connect(m_view2dButton, &ElaPushButton::clicked, this, [this]() { setViewMode(ViewMode::View2D); });
    connect(m_liveViewButton, &ElaPushButton::clicked, this, [this]() { setViewMode(ViewMode::LiveVideo); });
    connect(m_algorithmImageButton, &ElaPushButton::clicked, this, [this]() { setViewMode(ViewMode::AlgorithmImage); });
    connect(m_pointCloudButton, &ElaPushButton::clicked, this, [this]() { setViewMode(ViewMode::PointCloud3D); });
    connect(zoomIn, &QToolButton::clicked, this, [this]() {
        if (m_mode == ViewMode::View2D) m_imageArea->zoomIn();
        else if (m_mode == ViewMode::AlgorithmImage) m_algorithmImageArea->zoomIn();
    });
    connect(zoomOut, &QToolButton::clicked, this, [this]() {
        if (m_mode == ViewMode::View2D) m_imageArea->zoomOut();
        else if (m_mode == ViewMode::AlgorithmImage) m_algorithmImageArea->zoomOut();
    });
    connect(fitView, &QToolButton::clicked, this, [this]() {
        if (m_mode == ViewMode::View2D) m_imageArea->fitToView();
        else if (m_mode == ViewMode::AlgorithmImage) m_algorithmImageArea->fitToView();
    });
    connect(m_imageArea, &ImageViewFrame::millingPathClicked,
            this, &ImageDisplayPanel::millingPathClicked);
    connect(m_liveViewArea, &HikLiveView::expandRequested,
            this, &ImageDisplayPanel::livePreviewExpandRequested);

    applyViewButtonStyles();
    clearTask();
    Theme::watch(this, [this]() {
        if (m_boardLabel) {
            m_boardLabel->setStyleSheet(QStringLiteral("color:%1;font-size:13px;background:%2;border:1px solid %3;"
                                                       "border-radius:4px;padding:2px 10px;")
                                            .arg(Theme::kTextSecondary, Theme::kControlBg, Theme::kBorder));
        }
        const auto paintFrame = [](QFrame *frame, const char *name) {
            if (!frame) {
                return;
            }
            frame->setStyleSheet(QStringLiteral("QFrame#%1 { background:%2; border:1px solid %3; border-radius:6px; }")
                                     .arg(QLatin1String(name), QLatin1String(distributionCanvas()), QLatin1String(Theme::kBorder)));
        };
        paintFrame(m_imageArea, "imageArea");
        paintFrame(m_algorithmImageArea, "algorithmImageArea");
        for (QLabel *label : {m_footerLeft, m_footerRight}) {
            if (label) {
                label->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;border:none;")
                                         .arg(Theme::kTextSecondary));
            }
        }
        for (QToolButton *button : findChildren<QToolButton *>()) {
            const QString resource = button->property("iconResource").toString();
            if (!resource.isEmpty()) {
                button->setIcon(QIcon(tintedIcon(resource, QColor(Theme::kTextSecondary), 16, devicePixelRatioF())));
            }
            button->setStyleSheet(QStringLiteral(
                                      "QToolButton { background:%1; border:1px solid %2; border-radius:6px; }"
                                      "QToolButton:hover { background:%3; }"
                                      "QToolButton:pressed { background:%4; }")
                                      .arg(Theme::kControlBg, Theme::kBorder, Theme::kHoverBg, Theme::kPressedBg));
        }
        applyViewButtonStyles();
    });
}

QToolButton *ImageDisplayPanel::makeToolButton(const QString &iconResource, const QString &tooltip, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setFixedSize(28, 28);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(tooltip);
    button->setProperty("iconResource", iconResource);
    button->setIcon(QIcon(tintedIcon(iconResource, QColor(Theme::kTextSecondary), 16, devicePixelRatioF())));
    button->setIconSize(QSize(16, 16));
    button->setStyleSheet(QStringLiteral(
                              "QToolButton { background:%1; border:1px solid %2; border-radius:6px; }"
                              "QToolButton:hover { background:%3; }"
                              "QToolButton:pressed { background:%4; }")
                              .arg(Theme::kControlBg, Theme::kBorder, Theme::kHoverBg, Theme::kPressedBg));
    return button;
}

void ImageDisplayPanel::setMillingPaths(const QVector<MillingPathVM> &paths)
{
    m_paths = paths;
    if (m_imageArea) {
        m_imageArea->setMillingPaths(paths);
    }
    refreshPathSummary();
}

void ImageDisplayPanel::clearMillingPaths()
{
    m_paths.clear();
    if (m_imageArea) {
        m_imageArea->clearMillingPaths();
    }
    refreshPathSummary();
}

void ImageDisplayPanel::setToolPose(double xMm, double yMm, double zMm, bool valid,
                                    bool leaveGrindTrace)
{
    if (m_imageArea) {
        m_imageArea->setToolPose(xMm, yMm, zMm, valid, leaveGrindTrace);
    }
}

void ImageDisplayPanel::clearGrindLayer()
{
    if (m_imageArea) {
        m_imageArea->clearGrindLayer();
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

void ImageDisplayPanel::setTaskId(quint64 taskId)
{
    m_taskId = taskId;
    m_boardLabel->setText(taskId == 0 ? QStringLiteral("板号：--")
                                      : QStringLiteral("板号：#%1").arg(taskId));
    refreshPathSummary();
}

void ImageDisplayPanel::setProgressSummary(int completedPath, int pathTotal, float progressPercent)
{
    if (pathTotal <= 0 && completedPath <= 0 && progressPercent < 0.0f) {
        m_footerLeft->setText(QStringLiteral("等待任务"));
        return;
    }
    QString text = m_taskId == 0 ? QStringLiteral("任务") : QStringLiteral("任务 #%1").arg(m_taskId);
    if (pathTotal > 0) {
        text += QStringLiteral(" · 已完成 %1/%2 条路径").arg(qBound(0, completedPath, pathTotal)).arg(pathTotal);
    }
    if (progressPercent >= 0.0f) {
        text += QStringLiteral(" · 进度 %1%").arg(int(std::lround(progressPercent)));
    }
    m_footerLeft->setText(text);
}

void ImageDisplayPanel::clearTask()
{
    m_taskId = 0;
    m_boardLabel->setText(QStringLiteral("板号：--"));
    m_footerLeft->setText(QStringLiteral("等待任务"));
    refreshPathSummary();
}

void ImageDisplayPanel::refreshPathSummary()
{
    if (m_paths.isEmpty()) {
        m_footerRight->setText(QString());
        return;
    }
    int cut = 0;
    int rapid = 0;
    double lengthPx = 0.0;
    for (const MillingPathVM &path : m_paths) {
        (path.doCut ? cut : rapid) += 1;
        lengthPx += std::hypot(double(path.uEnd - path.uStart), double(path.vEnd - path.vStart));
    }
    QString text = QStringLiteral("切削 %1 条 · 空移 %2 条").arg(cut).arg(rapid);
    if (m_pointCloudFrame.hasMetricGeometry) {
        // 像素步距按 X / Y 比例取均值估算路径长度
        const double scale = (std::abs(m_pointCloudFrame.xScaleMm) + std::abs(m_pointCloudFrame.yScaleMm)) / 2.0;
        const double lengthMm = lengthPx * scale;
        text += lengthMm >= 1000.0 ? QStringLiteral(" · 路径 %1 m").arg(lengthMm / 1000.0, 0, 'f', 2)
                                   : QStringLiteral(" · 路径 %1 mm").arg(lengthMm, 0, 'f', 0);
    }
    m_footerRight->setText(text);
}

QString ImageDisplayPanel::currentViewModeName() const
{
    switch (m_mode) {
    case ViewMode::View2D:
        return QStringLiteral("粒子分布");
    case ViewMode::LiveVideo:
        return QStringLiteral("实时画面");
    case ViewMode::AlgorithmImage:
        return QStringLiteral("算法图像");
    case ViewMode::PointCloud3D:
        return QStringLiteral("三维点云");
    }
    return QStringLiteral("未知");
}

void ImageDisplayPanel::updateScanFrame(const PointCloudFrame &frame)
{
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
    refreshPathSummary();

    // 整板三维点云随扫图一起更新；隐藏状态下 HeightMap3DFrame 会缓存待上传。
    if (m_pointCloudArea) {
        m_pointCloudArea->setPointCloudFrame(frame);
    }

    // 新扫图到达时，若停在实时/算法页，自动切回粒子分布，避免“有帧但看不见”。
    // 三维点云页是用户主动查看的，不强制切走。
    if ((m_mode == ViewMode::LiveVideo || m_mode == ViewMode::AlgorithmImage)
        && (!m_rangeImage.isNull() || !m_intensityImage.isNull())) {
        setViewMode(ViewMode::View2D);
        return;
    }
    refreshView();
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

WId ImageDisplayPanel::liveVideoWindowId()
{
    return m_liveViewArea ? m_liveViewArea->videoWindowId() : WId();
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
    const auto style = [](ElaPushButton *button, bool active) {
        button->setChecked(active);
        active ? applySelectedButtonStyle(button) : applySecondaryButtonStyle(button);
        // ElaPushButton 的颜色属性变化不会自动请求重绘，需显式刷新外观。
        button->update();
    };
    style(m_view2dButton, m_mode == ViewMode::View2D);
    style(m_liveViewButton, m_mode == ViewMode::LiveVideo);
    style(m_algorithmImageButton, m_mode == ViewMode::AlgorithmImage);
    style(m_pointCloudButton, m_mode == ViewMode::PointCloud3D);
}

void ImageDisplayPanel::refreshView()
{
    if (m_mode == ViewMode::LiveVideo) {
        if (m_viewStack && m_liveViewArea) {
            fadeToStackedWidgetPage(m_viewStack, m_liveViewArea);
        }
        return;
    }
    if (m_mode == ViewMode::PointCloud3D) {
        if (m_viewStack && m_pointCloudArea) {
            fadeToStackedWidgetPage(m_viewStack, m_pointCloudArea);
            m_pointCloudArea->setPointCloudFrame(m_pointCloudFrame);
        }
        return;
    }
    if (m_mode == ViewMode::AlgorithmImage) {
        if (m_viewStack && m_algorithmImageArea) {
            fadeToStackedWidgetPage(m_viewStack, m_algorithmImageArea);
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
        fadeToStackedWidgetPage(m_viewStack, m_imageArea);
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
