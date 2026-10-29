#include "app/mainwindow.h"
#include "ui_mainwindow.h"

#include "app/AppStyle.h"
#include "app/NavigationBar.h"
#include "app/NotificationDrawer.h"
#include "ui/pages/ReportPage.h"
#include "auth/AuthService.h"
#include "data/Database.h"
#include "ui/dialogs/LoginDialog.h"
#include "ui/panels/DataStatisticsPanel.h"
#include "ui/panels/DeviceStatusPanel.h"
#include "ElaMessageBar.h"
#include "ui/panels/EventLogPanel.h"
#include "ui/dialogs/HistoryDialog.h"
#include "camera/HikCameraClient.h"
#include "camera/HikPreviewDialog.h"
#include "ui/panels/ImageDisplayPanel.h"
#include "ui/panels/KpiStripPanel.h"
#include "ui/panels/MachineStatusPanel.h"
#include "ui/panels/ParticleDetailPanel.h"
#include "ui/panels/PathQueuePanel.h"
#include "data/InspectionRepository.h"
#include "app/Logger.h"
#include "bridge/NodeLifecycleMonitor.h"
#include "bridge/MillingPathVM.h"
#include "bridge/PlcFeedbackVM.h"
#include "vision/PointCloudFrame.h"
#include "bridge/Ros2Bridge.h"
#include "app/RuntimeIniPaths.h"
#include "ui/settings/SettingsPage.h"
#include "app/TitleBar.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QProcess>
#include <QSizePolicy>
#include <QStringList>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QImage>
#include <QCloseEvent>
#include <QEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>
#include <QVector>

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

QString resolveProjectRootDir()
{
    QStringList starts;
    starts << QDir::currentPath()
           << QCoreApplication::applicationDirPath();

    for (const QString &start : starts) {
        QDir dir(start);
        for (int i = 0; i < 8; ++i) {
            if (QFileInfo::exists(dir.filePath(QStringLiteral("CMakeLists.txt")))
                || QFileInfo::exists(dir.filePath(QStringLiteral("TestImage")))) {
                return dir.absolutePath();
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }
    return QString();
}

bool isRangeLikeName(const QString &name)
{
    const QString lower = name.toLower();
    return lower.contains(QStringLiteral("range"))
           || lower.contains(QStringLiteral("pointcloud"))
           || lower.contains(QStringLiteral("depth"))
           || lower.contains(QStringLiteral("_z."));
}

bool isIntensityLikeName(const QString &name)
{
    const QString lower = name.toLower();
    return lower.contains(QStringLiteral("intensity"))
           || lower.contains(QStringLiteral("texture"))
           || lower.contains(QStringLiteral("gray"))
           || lower.contains(QStringLiteral("grey"));
}

bool isImageExtension(const QString &suffix)
{
    const QString lower = suffix.toLower();
    return lower == QStringLiteral("tif")
           || lower == QStringLiteral("tiff")
           || lower == QStringLiteral("png")
           || lower == QStringLiteral("bmp")
           || lower == QStringLiteral("jpg")
           || lower == QStringLiteral("jpeg");
}

bool resolveDebugImagePaths(QString *rangePath, QString *intensityPath, QString *error)
{
    const QString root = resolveProjectRootDir();
    if (root.isEmpty()) {
        if (error) {
            *error = QStringLiteral("找不到项目根目录（需含 CMakeLists.txt 或 TestImage）");
        }
        return false;
    }

    const QDir testDir(QDir(root).filePath(QStringLiteral("TestImage")));
    if (!testDir.exists()) {
        if (error) {
            *error = QStringLiteral("项目根目录缺少 TestImage：%1")
                         .arg(QDir::toNativeSeparators(testDir.absolutePath()));
        }
        return false;
    }

    QStringList rangeNamed;
    QStringList intensityNamed;
    QStringList tiffFiles;
    QStringList otherImages;
    const QFileInfoList entries = testDir.entryInfoList(QDir::Files | QDir::Readable,
                                                        QDir::Name);
    for (const QFileInfo &info : entries) {
        if (!isImageExtension(info.suffix())) {
            continue;
        }
        const QString path = info.absoluteFilePath();
        const QString name = info.fileName();
        const QString suffix = info.suffix().toLower();
        if (isRangeLikeName(name)) {
            rangeNamed.append(path);
        } else if (isIntensityLikeName(name)) {
            intensityNamed.append(path);
        } else if (suffix == QStringLiteral("tif") || suffix == QStringLiteral("tiff")) {
            tiffFiles.append(path);
        } else {
            otherImages.append(path);
        }
    }

    QString range = rangeNamed.value(0);
    QString intensity = intensityNamed.value(0);
    if (range.isEmpty() && !tiffFiles.isEmpty()) {
        range = tiffFiles.takeFirst();
    }
    if (intensity.isEmpty()) {
        if (!otherImages.isEmpty()) {
            intensity = otherImages.takeFirst();
        } else if (!tiffFiles.isEmpty()) {
            intensity = tiffFiles.takeFirst();
        } else if (rangeNamed.size() > 1) {
            intensity = rangeNamed.at(1);
        }
    }
    if (range.isEmpty() && !otherImages.isEmpty()) {
        range = otherImages.takeFirst();
    }

    if (range.isEmpty() || intensity.isEmpty()) {
        if (error) {
            *error = QStringLiteral(
                "TestImage 中需要至少一张 Range 图和一张 Intensity 图：%1")
                         .arg(QDir::toNativeSeparators(testDir.absolutePath()));
        }
        return false;
    }

    if (rangePath) {
        *rangePath = range;
    }
    if (intensityPath) {
        *intensityPath = intensity;
    }
    return true;
}

PointCloudFrame loadDebugPointCloudFrame(const QString &rangeImagePath, QString *error)
{
    PointCloudFrame frame;
    const QByteArray pathBytes = rangeImagePath.toLocal8Bit();
    const cv::Mat source = cv::imread(pathBytes.constData(), cv::IMREAD_UNCHANGED);
    if (source.empty()) {
        if (error) {
            *error = QStringLiteral("无法读取 Range 图：%1")
                         .arg(QDir::toNativeSeparators(rangeImagePath));
        }
        return frame;
    }

    cv::Mat floatSource;
    if (source.type() == CV_32FC1) {
        floatSource = source;
    } else if (source.channels() == 1) {
        source.convertTo(floatSource, CV_32FC1);
    } else {
        if (error) {
            *error = QStringLiteral("Range 图需为单通道深度图：type=%1 path=%2")
                         .arg(source.type())
                         .arg(QDir::toNativeSeparators(rangeImagePath));
        }
        return frame;
    }

    float lo = std::numeric_limits<float>::infinity();
    float hi = -std::numeric_limits<float>::infinity();
    for (int y = 0; y < floatSource.rows; ++y) {
        const float *row = floatSource.ptr<float>(y);
        for (int x = 0; x < floatSource.cols; ++x) {
            const float value = row[x];
            if (!std::isfinite(value) || value <= 0.0f) {
                continue;
            }
            lo = std::min(lo, value);
            hi = std::max(hi, value);
        }
    }
    if (!(lo < hi)) {
        if (error) {
            *error = QStringLiteral("Range 图中没有可归一化的有效深度值：%1")
                         .arg(QDir::toNativeSeparators(rangeImagePath));
        }
        return frame;
    }

    QImage image(floatSource.cols, floatSource.rows, QImage::Format_Grayscale8);
    if (image.isNull()) {
        if (error) {
            *error = QStringLiteral("无法为 Range 图分配显示缓冲区");
        }
        return frame;
    }

    const float scale = 255.0f / (hi - lo);
    for (int y = 0; y < floatSource.rows; ++y) {
        const float *sourceRow = floatSource.ptr<float>(y);
        uchar *targetRow = image.scanLine(y);
        for (int x = 0; x < floatSource.cols; ++x) {
            const float value = sourceRow[x];
            targetRow[x] = (!std::isfinite(value) || value <= 0.0f)
                ? 0
                : static_cast<uchar>(qBound(0.0f, (value - lo) * scale, 255.0f));
        }
    }
    const quint64 byteCount =
        quint64(floatSource.cols) * quint64(floatSource.rows) * sizeof(float);
    if (byteCount > quint64(std::numeric_limits<int>::max())) {
        if (error) {
            *error = QStringLiteral("Range 图数据量超过本地缓冲区限制");
        }
        return PointCloudFrame();
    }

    frame.rangeFloat32.resize(int(byteCount));
    const std::size_t rowBytes = static_cast<std::size_t>(floatSource.cols) * sizeof(float);
    for (int y = 0; y < floatSource.rows; ++y) {
        std::memcpy(frame.rangeFloat32.data() + static_cast<std::size_t>(y) * rowBytes,
                    floatSource.ptr<float>(y),
                    rowBytes);
    }
    frame.rangePreview = image;
    frame.width = quint32(floatSource.cols);
    frame.height = quint32(floatSource.rows);
    frame.rangePixelFormat = CV_32FC1;
    frame.hasMetricGeometry = true;
    frame.xScaleMm = 0.5;
    frame.yScaleMm = 0.5;
    frame.missingValue = -1.0e6f;
    return frame;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    buildMainView();
}

MainWindow::~MainWindow()
{
    m_shuttingDown = true;
    if (m_waveformTimer) {
        m_waveformTimer->stop();
    }
    if (m_simulationProgressTimer) {
        m_simulationProgressTimer->stop();
    }
    if (m_demoPoseTimer) {
        m_demoPoseTimer->stop();
    }
    if (m_ros2Bridge) {
        m_ros2Bridge->stop();
    }
    if (m_hikCameraClient) {
        m_hikCameraClient->shutdown();
    }
    stopNodesOnExit();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_settingsPage && !m_settingsPage->confirmLeave()) {
        event->ignore();
        if (m_navBar) {
            m_navBar->setCurrentPage(NavigationBar::Settings);
        }
        return;
    }

    // 破坏性操作保护：打磨任务进行中时关闭主窗口会中断任务，需二次确认。
    const bool taskInProgress = !m_activeTaskFinished && m_activeBackendTaskId != 0;
    if (taskInProgress) {
        QMessageBox confirmDialog(
            QMessageBox::Warning,
            QStringLiteral("确认退出"),
            QStringLiteral("检测任务正在进行中，退出将中断当前任务。\n确定要退出吗？"),
            QMessageBox::Yes | QMessageBox::No,
            this);
        confirmDialog.setDefaultButton(QMessageBox::No);
        confirmDialog.setEscapeButton(confirmDialog.button(QMessageBox::No));
        confirmDialog.setButtonText(QMessageBox::Yes, QStringLiteral("退出"));
        confirmDialog.setButtonText(QMessageBox::No, QStringLiteral("取消"));
        confirmDialog.setStyleSheet(
            QStringLiteral(
                "QMessageBox { background-color: %1; }"
                "QMessageBox QLabel { color: %2; min-width: 360px; font-size: 14px; }")
                .arg(QLatin1String(Theme::kPanelBg), QLatin1String(Theme::kTextPrimary)));
        if (auto *exitButton = qobject_cast<QPushButton *>(
                confirmDialog.button(QMessageBox::Yes))) {
            exitButton->setObjectName(QStringLiteral("exitConfirmButton"));
            exitButton->setStyleSheet(
                QStringLiteral(
                    "QPushButton#exitConfirmButton {"
                    "  min-width: 90px; min-height: 34px; padding: 2px 14px;"
                    "  color: %1; background-color: %2; border: 1px solid %3;"
                    "  border-radius: 5px; font-size: 14px; font-weight: 600;"
                    "}"
                    "QPushButton#exitConfirmButton:hover { background-color: %3; }"
                    "QPushButton#exitConfirmButton:pressed { background-color: %4; }"
                    "QPushButton#exitConfirmButton:focus { border: 2px solid %3; }")
                    .arg(QLatin1String(Theme::kTextPrimary),
                         QLatin1String(Theme::kErrorBg),
                         QLatin1String(Theme::kStateError),
                         QLatin1String(Theme::kPressedBg)));
        }
        if (auto *cancelButton = qobject_cast<QPushButton *>(
                confirmDialog.button(QMessageBox::No))) {
            cancelButton->setObjectName(QStringLiteral("cancelExitButton"));
            cancelButton->setStyleSheet(
                QStringLiteral(
                    "QPushButton#cancelExitButton {"
                    "  min-width: 90px; min-height: 34px; padding: 2px 14px;"
                    "  color: %1; background-color: %2; border: 1px solid %3;"
                    "  border-radius: 5px; font-size: 14px; font-weight: 600;"
                    "}"
                    "QPushButton#cancelExitButton:hover { background-color: %4; }"
                    "QPushButton#cancelExitButton:pressed { background-color: %5; }"
                    "QPushButton#cancelExitButton:focus { border: 2px solid %3; }")
                    .arg(QLatin1String(Theme::kTextPrimary),
                         QLatin1String(Theme::kRunningBg),
                         QLatin1String(Theme::kAccent),
                         QLatin1String(Theme::kSelectionBg),
                         QLatin1String(Theme::kPressedBg)));
}
        if (confirmDialog.exec() != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    stopNodesOnExit();
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && m_titleBar) {
        m_titleBar->setMaximizedState(isMaximized());
    }
}

void MainWindow::buildMainView()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowIcon(QIcon(":/img/logo.png"));
    setWindowTitle(QStringLiteral("阴极铜板粒子检测及修磨系统"));
    resize(Theme::kDesignWidth, Theme::kDesignHeight);
    setMinimumSize(1500, 920);
    menuBar()->hide();
    Theme::watch(this, []() { applyRuntimeTheme(); });

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);
    setCentralWidget(root);

    m_titleBar = new TitleBar(root);
    rootLayout->addWidget(m_titleBar);

    // 顶栏之下：左侧导航 + 内容区；通知抽屉覆盖在内容区右侧，不参与布局。
    auto *body = new QWidget(root);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(8);
    rootLayout->addWidget(body, 1);

    m_navBar = new NavigationBar(body);
    bodyLayout->addWidget(m_navBar, 0);

    m_contentStack = new QStackedWidget(body);
    bodyLayout->addWidget(m_contentStack, 1);

    // 抽屉是顶层 Qt::Tool 浮层（不参与子控件 Z 序），所以直接挂在主窗口下。
    m_drawer = new NotificationDrawer(this);
    m_drawer->hide();
    connectTitleBar();

    // ---- 检测与修磨：KPI 条 / 粒子分布 + 右侧（选中粒子、修磨实时状态）/ 底部队列 ----
    m_monitorPage = new QWidget(m_contentStack);
    auto *monitorLayout = new QVBoxLayout(m_monitorPage);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->setSpacing(8);

    m_kpiPanel = new KpiStripPanel(m_monitorPage);
    monitorLayout->addWidget(m_kpiPanel, 0);

    // 左列只有粒子分布，吃满 KPI 以下的高度，画面按扫描图比例放大；
    // 修磨队列不再横跨底部，收到右列，把高度让出来。
    auto *middle = new QWidget(m_monitorPage);
    auto *middleLayout = new QHBoxLayout(middle);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(8);

    m_imagePanel = new ImageDisplayPanel(middle);
    m_imagePanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    middleLayout->addWidget(m_imagePanel, 1);

    // 左图与整个右侧 1:1。右侧内部也是 1:1。点云和实时画面上下各占一半，一样大。
    auto *rightColumn = new QWidget(middle);
    rightColumn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *rightColumnLayout = new QHBoxLayout(rightColumn);
    rightColumnLayout->setContentsMargins(0, 0, 0, 0);
    rightColumnLayout->setSpacing(8);

    auto *visualColumn = new QWidget(rightColumn);
    visualColumn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *visualLayout = new QVBoxLayout(visualColumn);
    visualLayout->setContentsMargins(0, 0, 0, 0);
    visualLayout->setSpacing(8);
    m_particlePanel = new ParticleDetailPanel(visualColumn);
    m_particlePanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_particlePanel->setMinimumHeight(120);
    visualLayout->addWidget(m_particlePanel, 1);
    auto *livePanel = createPanel(QStringLiteral("实时画面"), nullptr, visualColumn);
    livePanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    livePanel->setMinimumHeight(120);
    QWidget *liveView = m_imagePanel->detachLiveView();
    if (liveView) {
        liveView->setParent(livePanel);
        liveView->setMinimumSize(0, 80);
        liveView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        if (auto *liveLayout = qobject_cast<QVBoxLayout *>(livePanel->layout())) {
            liveLayout->addWidget(liveView, 1);
        }
        // 从左侧堆叠页拆出来时控件是隐藏的，不 show 的话卡片里只剩标题。
        liveView->show();
    }
    visualLayout->addWidget(livePanel, 1);
    rightColumnLayout->addWidget(visualColumn, 1);

    auto *infoColumn = new QWidget(rightColumn);
    infoColumn->setMinimumWidth(0);
    infoColumn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *infoLayout = new QVBoxLayout(infoColumn);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(8);

    m_machinePanel = new MachineStatusPanel(infoColumn);
    auto *metricsPanel = createPanel(QStringLiteral("实时数据"), nullptr, infoColumn);
    metricsPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    if (QWidget *metrics = m_machinePanel->metricColumn()) {
        metrics->setParent(metricsPanel);
        if (auto *metricsLayout = qobject_cast<QVBoxLayout *>(metricsPanel->layout())) {
            metricsLayout->addWidget(metrics);
        }
    }
    infoLayout->addWidget(metricsPanel, 0);

    connect(m_navBar, &NavigationBar::millingStartRequested,
            this, &MainWindow::startDemoMilling);
    connect(m_navBar, &NavigationBar::millingPauseRequested,
            this, &MainWindow::pauseDemoMilling);
    connect(m_navBar, &NavigationBar::millingStopRequested,
            this, &MainWindow::stopDemoMilling);
    m_queuePanel = new PathQueuePanel(infoColumn);
    m_dataStatisticsPanel = m_queuePanel->telemetry();
    infoLayout->addWidget(m_queuePanel, 1);
    m_machinePanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    infoLayout->addWidget(m_machinePanel, 0);
    rightColumnLayout->addWidget(infoColumn, 1);
    middleLayout->addWidget(rightColumn, 1);
    monitorLayout->addWidget(middle, 1);
    m_contentStack->addWidget(m_monitorPage);

    // 2D 图点选路径 → 右侧粒子卡 + 队列表；队列表点选 → 粒子卡
    connect(m_imagePanel, &ImageDisplayPanel::millingPathClicked, this, [this](int pathIndex) {
        m_particlePanel->selectPath(pathIndex);
        m_queuePanel->selectPath(pathIndex);
    });
    connect(m_queuePanel, &PathQueuePanel::pathSelected, this, [this](int pathIndex) {
        m_particlePanel->selectPath(pathIndex);
    });
    m_hikCameraClient = new HikCameraClient(this);
    connect(m_imagePanel, &ImageDisplayPanel::liveViewRequested, this,
            [this](bool active, WId targetWindow) {
                if (!m_hikCameraClient) {
                    return;
                }
                if (active) {
                    m_hikCameraClient->startPreview(targetWindow);
                } else {
                    m_hikCameraClient->stopPreview();
                }
            });
    connect(m_imagePanel, &ImageDisplayPanel::livePreviewExpandRequested, this, [this]() {
        if (!m_imagePanel || !m_hikCameraClient || m_shuttingDown) {
            return;
        }
        if (!m_hikPreviewDialog) {
            m_hikPreviewDialog = new HikPreviewDialog(this);
            connect(m_hikPreviewDialog, &HikPreviewDialog::closed, this, [this]() {
                if (m_shuttingDown || !m_hikCameraClient || !m_hikPreviewConnected) {
                    return;
                }
                m_hikCameraClient->startPreview(m_imagePanel->liveVideoWindowId());
            });
        }
        m_hikPreviewDialog->setConnectionStatus(
            m_hikPreviewConnected,
            m_hikPreviewConnected ? QStringLiteral("实时画面已连接")
                                  : QStringLiteral("正在连接海康相机…"));
        m_hikPreviewDialog->show();
        m_hikPreviewDialog->raise();
        m_hikPreviewDialog->activateWindow();
        if (m_hikPreviewConnected) {
            m_hikCameraClient->startPreview(m_hikPreviewDialog->videoWindowId());
        }
    });
    connect(m_hikCameraClient, &HikCameraClient::connectionStatusChanged, this,
            [this](bool connected, const QString &message) {
                m_hikPreviewConnected = connected;
                if (m_imagePanel) {
                    m_imagePanel->setLiveCameraStatus(connected, message);
                }
                if (m_hikPreviewDialog && m_hikPreviewDialog->isVisible()) {
                    m_hikPreviewDialog->setConnectionStatus(connected, message);
                }
                if (m_settingsPage) {
                    m_settingsPage->setHikCameraStatus(connected, message);
                }
            });
    connect(m_hikCameraClient, &HikCameraClient::parametersUpdated, this,
            [this](const HikCameraParameters &parameters) {
                if (m_settingsPage) {
                    m_settingsPage->setHikCameraParameters(parameters);
                }
            });
    connect(m_hikCameraClient, &HikCameraClient::infoMessage, this,
            [this](const QString &message) {
                appendEventLog(QStringLiteral("INFO"), message);
            });
    connect(m_hikCameraClient, &HikCameraClient::errorMessage, this,
            [this](const QString &message) {
                appendEventLog(QStringLiteral("ERROR"), message);
            });

    // 横跨主窗口底部的状态栏：左侧合并状态文字，右侧三个状态指示控件。
    m_deviceStatusPanel = new DeviceStatusPanel(root);
    rootLayout->addWidget(m_deviceStatusPanel, 0);

    QTimer::singleShot(350, this, [this]() {
        ElaMessageBar::success(ElaMessageBarType::BottomRight, "界面初始化", "ElaWidgetTools 组件已接入。", 1800, this);
        appendEventLog("INFO", "界面初始化完成");
        if (m_hikCameraClient && m_imagePanel && !m_shuttingDown) {
            m_hikCameraClient->startPreview(m_imagePanel->liveVideoWindowId());
        }
    });

    m_waveformTimer = new QTimer(this);
    connect(m_waveformTimer, &QTimer::timeout, this, &MainWindow::onWaveformUpdate);
    m_simulationProgressTimer = new QTimer(this);
    m_simulationProgressTimer->setInterval(1000);
    connect(m_simulationProgressTimer, &QTimer::timeout,
            this, &MainWindow::onSimulationProgressUpdate);

    m_demoPoseTimer = new QTimer(this);
    m_demoPoseTimer->setInterval(33);
    connect(m_demoPoseTimer, &QTimer::timeout, this, &MainWindow::onDemoPoseUpdate);

    // 历史查询页在首次进入时才创建（HistoryDialog 查库较重），占位由 showPage 处理。
    m_reportPage = new ReportPage(m_contentStack);
    connect(m_reportPage, &ReportPage::logRequested, this, &MainWindow::appendEventLog);

    m_settingsPage = new SettingsPage(m_ros2Bridge,
                                      m_nodeLifecycleMonitor,
                                      m_drawer->eventLog(),
                                      m_contentStack);
    m_contentStack->addWidget(m_settingsPage);
    connect(m_settingsPage, &SettingsPage::historyRequested, this, &MainWindow::showHistoryPage);
    connect(m_settingsPage, &SettingsPage::logRequested, this,
            [this](const QString &level, const QString &message) {
                appendEventLog(level, message);
            });
    connect(m_settingsPage, &SettingsPage::startNodesRequested, this, &MainWindow::startNodes);
    connect(m_settingsPage, &SettingsPage::stopNodesRequested, this, &MainWindow::stopNodes);
    connect(m_settingsPage, &SettingsPage::hikCameraConnectRequested, this, [this]() {
        if (m_hikCameraClient && m_imagePanel) {
            m_settingsPage->setHikCameraStatus(false, QStringLiteral("正在连接网络相机…"));
            m_hikCameraClient->startPreview(m_imagePanel->liveVideoWindowId());
        }
    });
    connect(m_settingsPage, &SettingsPage::hikCameraPreviewRequested, this, [this](WId targetWindow) {
        if (m_hikCameraClient) {
            m_settingsPage->setHikCameraStatus(false, QStringLiteral("正在连接海康相机…"));
            m_hikCameraClient->startPreview(targetWindow);
        }
    });
    connect(m_settingsPage, &SettingsPage::hikCameraDisconnectRequested, this, [this]() {
        if (m_hikCameraClient) {
            m_hikCameraClient->stopPreview();
        }
        if (m_imagePanel) {
            m_imagePanel->setLiveCameraStatus(false, QStringLiteral("已从设备状态中断开预览"));
        }
    });
    connect(m_settingsPage, &SettingsPage::hikCameraRefreshParametersRequested, this, [this]() {
        if (m_hikCameraClient) {
            m_hikCameraClient->refreshPreviewParameters();
        }
    });
    connect(m_settingsPage, &SettingsPage::hikCameraParametersUpdateRequested, this,
            [this](const HikCameraParameters &parameters) {
                if (m_hikCameraClient) {
                    m_hikCameraClient->updatePreviewParameters(parameters);
                }
            });
    connect(m_settingsPage, &SettingsPage::sickCameraParametersUpdateRequested,
            m_ros2Bridge, &Ros2Bridge::publishSickCameraParameters);
    connect(m_ros2Bridge, &Ros2Bridge::sickCameraParametersReceived,
            m_settingsPage, &SettingsPage::setSickCameraParameters);
    connect(m_ros2Bridge, &Ros2Bridge::sickCameraParameterResultReceived,
            m_settingsPage, &SettingsPage::setSickCameraParameterResult);
    connect(m_settingsPage, &SettingsPage::refreshCameraDiagnosticsRequested,
            m_ros2Bridge, &Ros2Bridge::refreshCameraStatusDiagnostics);
    m_settingsPage->setCameraTopicDiagnostics(m_ros2Bridge && m_ros2Bridge->isRunning(), -1);
    if (m_nodeLifecycleMonitor) {
        connect(m_nodeLifecycleMonitor, &NodeLifecycleMonitor::nodesRunningChanged,
                m_settingsPage, &SettingsPage::setNodesRunning);
        m_settingsPage->setNodesRunning(m_nodeLifecycleMonitor->nodesRunning());
    }

    connect(m_navBar, &NavigationBar::pageRequested, this, &MainWindow::showPage);
    connect(m_navBar, &NavigationBar::userRequested, this, &MainWindow::showUserMenu);
    refreshUserIdentity();
    m_navBar->setCurrentPage(NavigationBar::Detection);
    m_contentStack->setCurrentWidget(m_monitorPage);
    refreshCalibrationStatus();

    // 设置页先完成参数与状态信号连接，再启动 ROS，避免首次保留消息在界面创建前丢失。
    startRos2Bridge();
}

void MainWindow::showPage(int page)
{
    if (!m_contentStack || !m_navBar) {
        return;
    }
    const bool leavingSettings = m_settingsPage && m_contentStack->currentWidget() == m_settingsPage
                                 && page != NavigationBar::Settings;
    if (leavingSettings && !m_settingsPage->confirmLeave()) {
        m_navBar->setCurrentPage(NavigationBar::Settings);
        return;
    }

    QWidget *target = nullptr;
    QString logText;
    switch (page) {
    case NavigationBar::Detection:
        target = m_monitorPage;
        logText = QStringLiteral("用户切换到「检测与修磨」");
        break;
    case NavigationBar::History:
        if (!m_historyDialog) {
            m_historyDialog = new HistoryDialog(m_contentStack);
            m_contentStack->addWidget(m_historyDialog);
        }
        m_historyDialog->reloadRecords();
        target = m_historyDialog;
        logText = QStringLiteral("用户切换到「历史查询」");
        break;
    case NavigationBar::Report:
        if (m_reportPage && m_contentStack->indexOf(m_reportPage) < 0) {
            m_contentStack->addWidget(m_reportPage);
        }
        if (m_reportPage) {
            m_reportPage->reload();
        }
        target = m_reportPage;
        logText = QStringLiteral("用户切换到「报表统计」");
        break;
    case NavigationBar::Settings:
        target = m_settingsPage;
        logText = QStringLiteral("用户切换到「系统设置」");
        break;
    default:
        break;
    }
    if (!target) {
        return;
    }
    const bool changed = m_contentStack->currentWidget() != target;
    fadeToStackedWidgetPage(m_contentStack, target, 200);
    m_navBar->setCurrentPage(page);
    if (changed) {
        appendEventLog(QStringLiteral("INFO"), logText);
    }
    // 切页会重新 show/raise 页面，抽屉需要重新回到最上层。
    if (m_drawer && m_drawer->isVisible()) {
        m_drawer->raise();
    }
}

void MainWindow::updateDrawerDockArea()
{
    if (!m_drawer || !m_contentStack || !centralWidget()) {
        return;
    }
    // 抽屉贴着内容区右缘，纵向覆盖整个内容区高度。
    // 抽屉是顶层窗口，几何按屏幕坐标计算，所以这里映射到全局坐标。
    const QRect stackRect(m_contentStack->mapToGlobal(QPoint(0, 0)), m_contentStack->size());
    m_drawer->setDockArea(stackRect);
    if (m_drawer->isVisible()) {
        m_drawer->raise();
    }
}

void MainWindow::refreshCalibrationStatus()
{
    if (!m_deviceStatusPanel) {
        return;
    }
    const QString path = backendParamsIniPath();
    const bool calibrated = !path.isEmpty() && QFileInfo::exists(path);
    m_deviceStatusPanel->setCalibrationStatus(
        calibrated,
        calibrated ? QStringLiteral("标定参数：%1").arg(QDir::toNativeSeparators(path))
                   : QStringLiteral("未找到后端标定参数文件"));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateDrawerDockArea();
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    // 抽屉是顶层浮层窗口，不会自动跟随主窗口移动，需要显式重算停靠区域。
    if (m_drawer && m_drawer->isVisible()) {
        updateDrawerDockArea();
    }
}

void MainWindow::connectTitleBar()
{
    connect(m_titleBar, &TitleBar::userRequested, this, &MainWindow::showUserMenu);
    connect(m_titleBar, &TitleBar::notificationsRequested, this, [this]() {
        if (!m_drawer) {
            return;
        }
        updateDrawerDockArea();
        m_drawer->toggle();
    });
    connect(m_drawer, &NotificationDrawer::unreadChanged,
            m_titleBar, &TitleBar::setNotificationCount);
    AuthService &auth = AuthService::instance();
    connect(&auth, &AuthService::sessionChanged, this, &MainWindow::refreshUserIdentity);
    connect(&auth, &AuthService::sessionExpired, this, [this]() {
        const QString message = QStringLiteral("空闲超过 %1 分钟，已自动退出登录，设置退回只读。")
                                    .arg(AuthService::instance().sessionTimeoutMinutes());
        ElaMessageBar::warning(ElaMessageBarType::BottomRight,
                               QStringLiteral("会话已超时"),
                               message,
                               4000,
                               this);
        appendEventLog(QStringLiteral("WARN"), message);
    });
    refreshUserIdentity();
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRestoreRequested, this, &MainWindow::toggleMaximized);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::dragRequested, this, [this](const QPoint &globalTopLeft) {
        move(globalTopLeft);
    });
    m_titleBar->setMaximizedState(isMaximized());

    m_nodeLifecycleMonitor = new NodeLifecycleMonitor(this);
    connect(m_nodeLifecycleMonitor, &NodeLifecycleMonitor::nodesRunningChanged,
            m_titleBar, &TitleBar::setNodesRunning);
    m_titleBar->setNodesRunning(m_nodeLifecycleMonitor->nodesRunning());
}

void MainWindow::showUserMenu(const QPoint &anchor)
{
    AuthService &auth = AuthService::instance();
    if (!auth.isLoggedIn()) {
        LoginDialog::runLogin(this);
        return;
    }

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
                           "QMenu { background: %1; color: %2; border: 1px solid %3; font-size: 15px; }"
                           "QMenu::item { padding: 8px 28px; }"
                           "QMenu::item:selected { background: %4; color:%5; }"
                           "QMenu::item:selected:hover { background:%6; }")
                           .arg(Theme::kPanelBg,
                                Theme::kTextPrimary,
                                Theme::kBorder,
                                Theme::kSelectionBg,
                                Theme::kSelectionText,
                                Theme::kSelectedHoverBg));
    QAction *changePassword = menu.addAction(QStringLiteral("修改密码"));
    QAction *logout = menu.addAction(QStringLiteral("退出登录"));
    QAction *chosen = menu.exec(anchor);
    if (chosen == changePassword) {
        if (LoginDialog::runChangePassword(this)) {
            ElaMessageBar::success(ElaMessageBarType::BottomRight,
                                   QStringLiteral("账户"),
                                   QStringLiteral("密码已修改。"),
                                   2400,
                                   this);
        }
    } else if (chosen == logout) {
        auth.logout();
    }
}

void MainWindow::refreshUserIdentity()
{
    AuthService &auth = AuthService::instance();
    const QString identity = auth.identityText();
    if (m_titleBar) {
        m_titleBar->setUserIdentity(auth.currentUsername(), auth.currentRole());
    }
    if (m_navBar) {
        m_navBar->setUserIdentity(auth.currentUsername(), auth.currentRole());
    }
    if (identity != m_lastIdentity) {
        if (!m_lastIdentity.isEmpty()) {
            appendEventLog(QStringLiteral("INFO"), QStringLiteral("当前身份：%1").arg(identity));
        }
        m_lastIdentity = identity;
    }
}

QString MainWindow::resolveLauncherConfigPath() const
{
    return nodeLauncherIniPath();
}

QString MainWindow::resolveLauncherScript(const QString &key) const
{
    return resolveNodeLauncherScript(key);
}

void MainWindow::showHistoryPage()
{
    showPage(NavigationBar::History);
}

QString MainWindow::resolveStartAllScript() const
{
    return resolveLauncherScript(QStringLiteral("StartAll"));
}

QString MainWindow::resolveStopAllScript() const
{
    return resolveLauncherScript(QStringLiteral("StopAll"));
}

void MainWindow::stopNodesOnExit()
{
    if (m_stopScriptLaunched) {
        return;
    }
    m_stopScriptLaunched = true;

    if (m_startAllProcess && m_startAllProcess->state() != QProcess::NotRunning) {
        m_startAllProcess->kill();
        m_startAllProcess->waitForFinished(1000);
    }

    const QString script = resolveStopAllScript();
    if (script.isEmpty()) {
        appendEventLog("ERROR", "未在 node_launcher.ini 中找到可用的 StopAll 脚本路径，退出时无法停止节点");
        return;
    }

    const bool started = QProcess::startDetached(
        QStringLiteral("cmd.exe"),
        QStringList()
            << QStringLiteral("/c")
            << QDir::toNativeSeparators(script)
            << QStringLiteral("--no-pause"),
        QFileInfo(script).absolutePath());
    if (started) {
        appendEventLog("INFO",
                       QStringLiteral("正在停止节点: %1")
                           .arg(QDir::toNativeSeparators(script)));
    } else {
        appendEventLog("ERROR", "无法启动 stop_all.bat");
    }
}

void MainWindow::startNodes()
{
    runNodeLauncherScript(true);
}

void MainWindow::stopNodes()
{
    runNodeLauncherScript(false);
}

void MainWindow::runNodeLauncherScript(bool start)
{
    if (m_startAllProcess && m_startAllProcess->state() != QProcess::NotRunning) {
        return;
    }

    const QString script = start ? resolveStartAllScript() : resolveStopAllScript();
    if (script.isEmpty()) {
        appendEventLog("ERROR",
                       start ? QStringLiteral("未在 node_launcher.ini 中找到可用的 StartAll 脚本路径")
                             : QStringLiteral("未在 node_launcher.ini 中找到可用的 StopAll 脚本路径"));
        return;
    }

    if (!m_startAllProcess) {
        m_startAllProcess = new QProcess(this);
        m_startAllProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_startAllProcess,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this,
                [this](int exitCode, QProcess::ExitStatus status) {
                    const bool starting = m_startAllProcess
                                          && m_startAllProcess->property("mzStart").toBool();
                    if (m_titleBar) {
                        m_titleBar->setStartNodesBusy(false);
                    }
                    if (m_settingsPage) {
                        m_settingsPage->setNodesBusy(false);
                    }
                    if (m_nodeLifecycleMonitor) {
                        m_nodeLifecycleMonitor->pollNow();
                    }
                    const bool nodesUp = m_nodeLifecycleMonitor
                                         && m_nodeLifecycleMonitor->nodesRunning();
                    const bool ok = starting
                        ? (nodesUp || (status == QProcess::NormalExit && exitCode == 0))
                        : (!nodesUp || (status == QProcess::NormalExit && exitCode == 0));
                    if (ok) {
                        appendEventLog("INFO",
                                       starting ? QStringLiteral("节点启动脚本已执行完毕")
                                                : QStringLiteral("节点停止脚本已执行完毕"));
                    } else {
                        const QString detail =
                            QString::fromLocal8Bit(m_startAllProcess->readAll())
                                .replace(QLatin1Char('\r'), QString())
                                .simplified();
                        appendEventLog("ERROR",
                                       QStringLiteral("%1脚本执行失败: %2")
                                           .arg(starting ? QStringLiteral("节点启动")
                                                         : QStringLiteral("节点停止"),
                                                detail));
                    }
                });
        connect(m_startAllProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            Q_UNUSED(error);
            if (m_startAllProcess->state() != QProcess::NotRunning) {
                return;
            }
            if (m_titleBar) {
                m_titleBar->setStartNodesBusy(false);
            }
            if (m_settingsPage) {
                m_settingsPage->setNodesBusy(false);
            }
            const bool starting = m_startAllProcess
                                  && m_startAllProcess->property("mzStart").toBool();
            appendEventLog("ERROR",
                           QStringLiteral("无法启动%1脚本: %2")
                               .arg(starting ? QStringLiteral("节点启动")
                                             : QStringLiteral("节点停止"),
                                    m_startAllProcess->errorString()));
        });
    }

    m_startAllProcess->setProperty("mzStart", start);
    if (m_titleBar) {
        m_titleBar->setStartNodesBusy(true);
    }
    if (m_settingsPage) {
        m_settingsPage->setNodesBusy(true);
    }
    appendEventLog("INFO",
                   QStringLiteral("%1节点: %2")
                       .arg(start ? QStringLiteral("正在启动") : QStringLiteral("正在停止"),
                            QDir::toNativeSeparators(script)));

    m_startAllProcess->setWorkingDirectory(QFileInfo(script).absolutePath());
    m_startAllProcess->setStandardInputFile(QProcess::nullDevice());
    m_startAllProcess->start(QStringLiteral("cmd.exe"),
                             QStringList()
                                 << QStringLiteral("/c")
                                 << QDir::toNativeSeparators(script)
                                 << QStringLiteral("--no-pause"));
}

void MainWindow::startRos2Bridge()
{
    m_ros2Bridge = new Ros2Bridge(this);
    connect(m_ros2Bridge, &Ros2Bridge::infoMessage, this, [this](const QString &message) {
        appendEventLog("INFO", message);
    });
    connect(m_ros2Bridge, &Ros2Bridge::errorMessage, this, [this](const QString &message) {
        appendEventLog("ERROR", message);
    });
    connect(m_ros2Bridge, &Ros2Bridge::scanResultReceived, this,
            [this](quint8 imageType,
                   quint64 frameId,
                   quint64 taskId,
                   const QString &shmName,
                   quint64 offset,
                   quint64 dataSize,
                   quint32 width,
                   quint32 height,
                   quint32 pixelFormat) {
                appendEventLog("INFO",
                               QString("相机图像通知 type=%1 frame=%2 task=%3 shm=%4 offset=%5 size=%6 %7x%8 fmt=%9")
                                   .arg(imageType)
                                   .arg(frameId)
                                   .arg(taskId)
                                   .arg(shmName)
                                   .arg(offset)
                                   .arg(dataSize)
                                   .arg(width)
                                   .arg(height)
                                   .arg(pixelFormat));
            });
    connect(m_ros2Bridge, &Ros2Bridge::scanFrameReady, this,
            [this](const PointCloudFrame &frame) {
                if (m_debugFakeActive) {
                    appendEventLog("INFO",
                                   QStringLiteral("扫图已到达但调试演示开启，界面未刷新真机帧 frame=%1")
                                       .arg(frame.frameId));
                    return;
                }
                if (m_imagePanel) {
                    m_imagePanel->updateScanFrame(frame);
                    appendEventLog(
                        "INFO",
                        QStringLiteral("界面已刷新扫图: frame=%1 task=%2 %3x%4 mode=%5")
                            .arg(frame.frameId)
                            .arg(frame.taskId)
                            .arg(frame.width)
                            .arg(frame.height)
                            .arg(m_imagePanel->currentViewModeName()));
                }
                if (m_particlePanel) {
                    m_particlePanel->setPointCloudFrame(frame);
                }
                if (m_queuePanel) {
                    m_queuePanel->setMetricFrame(frame);
                }

                // 帧落库：消息没带 task_id 时回退到当前任务（MillingPaths 记录的任务号）
                const quint64 effectiveTaskId = frame.taskId != 0
                    ? frame.taskId
                    : m_currentBackendTaskId;
                if (effectiveTaskId == 0 || frame.frameId == 0) {
                    return;
                }
                persistScanFrame(effectiveTaskId,
                                 frame.frameId,
                                 frame.timestampNs,
                                 int(frame.width),
                                 int(frame.height),
                                 frame.rangePixelFormat,
                                 frame.intensityPixelFormat,
                                 !frame.rangePreview.isNull(),
                                 !frame.intensity.isNull());
            },
            Qt::QueuedConnection);
    connect(m_ros2Bridge, &Ros2Bridge::backendStateReceived, this,
            [this](const QString &state) {
                appendEventLog("INFO", QString("后端状态: %1").arg(state));
                if (m_deviceStatusPanel) {
                    m_deviceStatusPanel->setBackendState(state);
                }
            });
    connect(m_ros2Bridge, &Ros2Bridge::cameraStatusReceived, this,
            [this](bool connected, const QString &cameraId, const QString &message) {
                if (m_deviceStatusPanel) {
                    m_deviceStatusPanel->setCameraStatus(connected, cameraId, message);
                }
                if (m_settingsPage) {
                    const QString detail = cameraId.isEmpty() ? message : cameraId + QStringLiteral("\n") + message;
                    m_settingsPage->setCameraNodeStatus(connected, detail);
                }
            });
    connect(m_ros2Bridge, &Ros2Bridge::cameraStatusDiagnosticsChanged, this,
            [this](bool bridgeRunning, int publisherCount) {
                if (m_settingsPage) {
                    m_settingsPage->setCameraTopicDiagnostics(bridgeRunning, publisherCount);
                }
            });

    connect(m_ros2Bridge, &Ros2Bridge::millingPathsReceived, this,
            [this](quint64 taskId,
                   int pathTotal,
                   int maxParticleHeight,
                   const QString &particleBoardGrade,
                   const QString &drawimagePath,
                   const QVector<MillingPathVM> &paths) {
                m_currentBackendTaskId = taskId;
                m_activeBackendTaskId = taskId;
                m_activeTaskFinished = false;
                m_lastMillingProgress = -1.0f;
                m_lastLoggedIgnoredProgressTaskId = 0;
                if (taskId != 0) {
                    persistTaskRecord(taskId);
                }
                m_activePaths = paths;
                m_activePathTotal = pathTotal;
                m_activeCompletedPath = 0;
                m_activeBoardGrade = particleBoardGrade.trimmed();
                if (m_imagePanel) {
                    m_imagePanel->setMillingPaths(paths);
                    m_imagePanel->setAlgorithmImagePath(drawimagePath);
                    m_imagePanel->setTaskId(taskId);
                    m_imagePanel->setProgressSummary(0, pathTotal, 0.0f);
                }
                if (m_particlePanel) {
                    m_particlePanel->setMillingPaths(paths);
                    m_particlePanel->setTaskInfo(maxParticleHeight, particleBoardGrade);
                }
                if (m_queuePanel) {
                    m_queuePanel->setMillingPaths(paths);
                }
                if (m_machinePanel) {
                    m_machinePanel->setTask(taskId, pathTotal);
                }
                if (m_particlePanel) {
                    m_particlePanel->setTaskProgress(0.0f);
                }
                updateKpis();
                appendEventLog("INFO",
                               QString("任务 #%1 开始，共 %2 条路径，等级 %3")
                                   .arg(taskId)
                                   .arg(pathTotal)
                                   .arg(particleBoardGrade.isEmpty()
                                            ? QStringLiteral("--")
                                            : particleBoardGrade));

                const PendingMillingProgress pending =
                    m_pendingMillingProgress.value(taskId);
                m_pendingMillingProgress.clear();
                if (pending.valid) {
                    handleMillingProgress(pending.taskId,
                                          pending.progress,
                                          pending.finished,
                                          pending.success,
                                          pending.message);
                }
            });

    connect(m_ros2Bridge, &Ros2Bridge::millingProgressReceived, this,
            [this](quint64 taskId,
                   float progress,
                   bool finished,
                   bool success,
                   const QString &message) {
                handleMillingProgress(taskId, progress, finished, success, message);
            });

    connect(m_ros2Bridge, &Ros2Bridge::plcFeedbackReceived, this,
            [this](const PlcFeedbackVM &feedback) {
                if (m_machinePanel) {
                    m_machinePanel->setPlcFeedback(feedback);
                }
                // Waveform timer already drives the plot in debug simulation;
                // appending PLC samples too paints dense sawtooth bands.
                if (m_dataStatisticsPanel && !m_simulationRunning) {
                    m_dataStatisticsPanel->appendSpindleSample(
                        feedback.pathParams.spindleSpeed,
                        feedback.pathParams.spindleTorque);
                }
                if (m_deviceStatusPanel) {
                    m_deviceStatusPanel->setBackendOnline(
                        QStringLiteral("收到后端转发的 PLC 反馈"));
                    m_deviceStatusPanel->setPlcStatus(feedback.statusWord,
                                                     feedback.faultCode);
                }
                if (m_settingsPage) {
                    const bool fault = feedback.faultCode != 0;
                    m_settingsPage->setPlcDeviceStatus(
                        fault ? QStringLiteral("故障") : QStringLiteral("已连接"),
                        QStringLiteral("状态字：0x%1\n故障码：%2")
                            .arg(feedback.statusWord, 4, 16, QLatin1Char('0'))
                            .arg(feedback.faultCode)
                            .toUpper());
                }
                if (m_particlePanel) {
                    m_particlePanel->setPathProgress(int(feedback.completedPath),
                                                     int(feedback.currentPath));
                }
                if (m_queuePanel) {
                    m_queuePanel->setPathProgress(int(feedback.completedPath),
                                                  int(feedback.currentPath));
                }
                if (m_activeCompletedPath != int(feedback.completedPath)) {
                    m_activeCompletedPath = int(feedback.completedPath);
                    updateKpis();
                    if (m_imagePanel) {
                        m_imagePanel->setProgressSummary(m_activeCompletedPath,
                                                         m_activePathTotal,
                                                         m_lastMillingProgress);
                    }
                }
                if (m_imagePanel && !m_debugFakeActive) {
                    // Live tool tip on 2D view: same metric space as setMetricFrame hover.
                    // Demo mode drives pose itself (with cut/rapid stamp control).
                    m_imagePanel->setToolPose(double(feedback.pathParams.xPos),
                                              double(feedback.pathParams.yPos),
                                              double(feedback.pathParams.zPos),
                                              /*valid=*/true);
                    m_imagePanel->setPathProgress(int(feedback.completedPath),
                                                  int(feedback.currentPath));
                }
            });

    m_ros2Bridge->start();

    // 调试快捷键 Ctrl+T：切换演示模式。
    // 第一次按：加载相机保存的本地图像 + 示例任务数据。
    // 再按一次：清掉假图，任务条复位为"待命"。
    auto *fakeShortcut = new QShortcut(QKeySequence("Ctrl+T"), this);
    connect(fakeShortcut, &QShortcut::activated, this, &MainWindow::toggleDemoMode);

    // 调试快捷键 Ctrl+Shift+T：结束假任务（success）。
    auto *fakeFinishShortcut = new QShortcut(QKeySequence("Ctrl+Shift+T"), this);
    connect(fakeFinishShortcut, &QShortcut::activated, this, [this]() {
        if (!m_debugFakeActive) {
            return;
        }
        m_simulationProgress = 100.0f;
        setSimulationRunning(false);
        emit m_ros2Bridge->millingProgressReceived(
            9001, 100.0f, /*finished*/ true, /*success*/ true,
            QStringLiteral("调试模式：模拟完成"));
    });
}

void MainWindow::updateKpis()
{
    if (!m_kpiPanel) {
        return;
    }
    if (m_activePaths.isEmpty()) {
        m_kpiPanel->clear();
        return;
    }
    int detected = 0;
    int completed = 0;
    for (int i = 0; i < m_activePaths.size(); ++i) {
        if (!m_activePaths.at(i).doCut) {
            continue;
        }
        ++detected;
        if (i < m_activeCompletedPath) {
            ++completed;
        }
    }
    m_kpiPanel->setCounts(detected, detected - completed, completed);
    m_kpiPanel->setBoardGrade(m_activeBoardGrade);
}

void MainWindow::toggleDemoMode()
{
    {
        if (!m_debugFakeActive) {
            QString loadError;
            QString rangePath;
            QString intensityPath;
            if (!resolveDebugImagePaths(&rangePath, &intensityPath, &loadError)) {
                appendEventLog("ERROR",
                               QStringLiteral("进入调试演示模式失败：%1").arg(loadError));
                return;
            }

            PointCloudFrame frame = loadDebugPointCloudFrame(rangePath, &loadError);
            QImage intensity(intensityPath);
            if (!intensity.isNull()) {
                intensity = intensity.convertToFormat(QImage::Format_Grayscale8);
            }
            if (!frame.hasFloatRange() || intensity.isNull()) {
                if (loadError.isEmpty()) {
                    loadError = QStringLiteral("无法读取 Intensity 图：%1")
                                    .arg(QDir::toNativeSeparators(intensityPath));
                }
                appendEventLog("ERROR", QStringLiteral("进入调试演示模式失败：%1").arg(loadError));
                return;
            }
            if (frame.rangePreview.size() != intensity.size()) {
                appendEventLog(
                    "ERROR",
                    QStringLiteral("进入调试演示模式失败：Range 与 Intensity 尺寸不一致（%1 / %2）")
                        .arg(QDir::toNativeSeparators(rangePath),
                             QDir::toNativeSeparators(intensityPath)));
                return;
            }

            m_debugFakeActive = true;
            if (m_reportPage) {
                m_reportPage->setDemoMode(true);
            }
            frame.intensity = intensity;
            frame.frameId = 9001;
            frame.intensityPixelFormat = CV_8UC1;
            m_debugXScaleMm = frame.xScaleMm;
            m_debugYScaleMm = frame.yScaleMm;
            m_debugXOffsetMm = frame.xOffsetMm;
            m_debugYOffsetMm = frame.yOffsetMm;
            if (m_imagePanel) {
                m_imagePanel->clearGrindLayer();
                m_imagePanel->updateScanFrame(frame);
                m_imagePanel->show2D(); // grind demo is 2D-only
            }
            if (m_particlePanel) {
                m_particlePanel->setPointCloudFrame(frame);
            }
            if (m_queuePanel) {
                m_queuePanel->setMetricFrame(frame);
            }

            // Build a serpentine cut/rapids pattern across the real TestImage.
            const int w = int(frame.width);
            const int h = int(frame.height);
            const int x0 = qMax(40, w / 8);
            const int x1 = qMin(w - 40, (w * 7) / 8);
            const int y0 = qMax(40, h / 8);
            const int y1 = qMin(h - 40, (h * 5) / 8);
            const int yMid = (y0 + y1) / 2;

            QVector<MillingPathVM> paths;
            paths.push_back({ x0, y0, x1, y0, 3000.0f, 12.0f, 0.0f, true });   // cut
            paths.push_back({ x1, y0, x1, yMid, 3000.0f, 0.0f, 10.0f, false }); // rapid
            paths.push_back({ x1, yMid, x0, yMid, 2800.0f, -12.0f, 0.0f, true }); // cut
            paths.push_back({ x0, yMid, x0, y1, 2800.0f, 0.0f, 10.0f, false }); // rapid
            paths.push_back({ x0, y1, x1, y1, 3200.0f, 15.0f, 0.0f, true });   // cut
            m_debugPaths = paths;
            resetDemoPoseAnimation();

            emit m_ros2Bridge->millingPathsReceived(
                /*taskId*/ 9001,
                /*pathTotal*/ paths.size(),
                /*maxParticleHeight*/ 5,
                /*particleBoardGrade*/ QString(),
                /*drawimagePath*/ QString(),
                paths);
            emit m_ros2Bridge->millingProgressReceived(
                9001, 0.0f, /*finished*/ false, /*success*/ false, QString());

            m_currentT = 0.0;
            m_simulationProgress = 0.0f;
            setSimulationRunning(true);
            if (m_demoPoseTimer) {
                m_demoPoseTimer->start();
            }
            onDemoPoseUpdate(); // seed first pose immediately

            appendEventLog(
                "INFO",
                QStringLiteral("已加载 TestImage（%1 + %2）并进入调试演示模式（Ctrl+T 再按一次取消）")
                    .arg(QFileInfo(rangePath).fileName(),
                         QFileInfo(intensityPath).fileName()));
        } else {
            // 退出演示模式：清假图、任务条复位。
            setSimulationRunning(false);
            m_simulationProgress = 0.0f;
            if (m_demoPoseTimer) {
                m_demoPoseTimer->stop();
            }
            m_debugPaths.clear();
            if (m_imagePanel) {
                m_imagePanel->clearToolPose();
                m_imagePanel->clearGrindLayer();
                m_imagePanel->show2D();
                m_imagePanel->clearMillingPaths();
                m_imagePanel->clearAlgorithmImage();
                m_imagePanel->updateScanFrame(PointCloudFrame());
                m_imagePanel->clearTask();
            }
            if (m_particlePanel) {
                m_particlePanel->clear();
            }
            if (m_queuePanel) {
                m_queuePanel->clear();
            }
            if (m_machinePanel) {
                m_machinePanel->clear();
            }
            if (m_deviceStatusPanel) {
                m_deviceStatusPanel->clearBackendAndPlcStatus();
            }
            if (m_settingsPage) {
                m_settingsPage->setPlcDeviceStatus(QStringLiteral("待反馈"),
                                                   QStringLiteral("PLC 状态数据来源待后端补充"));
            }
            m_activePaths.clear();
            m_activePathTotal = 0;
            m_activeCompletedPath = 0;
            m_activeBoardGrade.clear();
            updateKpis();
            m_debugFakeActive = false;
            if (m_reportPage) {
                m_reportPage->setDemoMode(false);
            }
            appendEventLog("INFO", "已退出调试演示模式");
        }
    }
}

void MainWindow::setDatabaseReady(bool ready)
{
    m_databaseReady = ready;
    if (m_databaseReady) {
        AuthService &auth = AuthService::instance();
        QString authError;
        if (!auth.attachDatabase(QString::fromLatin1(Database::connectionName()), &authError)) {
            appendEventLog(QStringLiteral("ERROR"),
                           QStringLiteral("账户功能不可用，受控设置保持锁定: %1").arg(authError));
        } else if (!auth.hasAccounts()) {
            appendEventLog(QStringLiteral("WARN"), QStringLiteral("系统中还没有账户，请创建第一个管理员"));
            QTimer::singleShot(0, this, [this]() { LoginDialog::runLogin(this); });
        }
        flushPendingDatabaseWrites();
    }
}

void MainWindow::persistTaskRecord(quint64 taskId)
{
    // Demo shortcut (Ctrl+T) uses task 9001 — never write fake tasks into SQLite.
    if (m_debugFakeActive || taskId == 9001) {
        return;
    }
    if (!m_databaseReady) {
        m_pendingDatabaseTaskIds.insert(taskId);
        LOG("[DB] 数据库初始化中，暂存任务记录: task=%llu",
            static_cast<unsigned long long>(taskId));
        return;
    }

    QString error;
    if (!InspectionRepository::ensureRecordForTask(taskId, &error)) {
        appendEventLog("ERROR", QString("创建检测任务记录失败: %1").arg(error));
    }
}

void MainWindow::persistScanFrame(quint64 taskId,
                                  quint64 frameId,
                                  quint64 timestampNs,
                                  int width,
                                  int height,
                                  int rangePixelFormat,
                                  int intensityPixelFormat,
                                  bool hasRange,
                                  bool hasIntensity)
{
    // Demo frames must not hit SQLite.
    if (m_debugFakeActive || taskId == 9001) {
        return;
    }

    if (!m_databaseReady) {
        m_pendingDatabaseTaskIds.insert(taskId);
        m_pendingDatabaseFrames.append({taskId,
                                        frameId,
                                        timestampNs,
                                        width,
                                        height,
                                        rangePixelFormat,
                                        intensityPixelFormat,
                                        hasRange,
                                        hasIntensity});
        LOG("[DB] 数据库初始化中，暂存扫描帧: task=%llu frame=%llu",
            static_cast<unsigned long long>(taskId),
            static_cast<unsigned long long>(frameId));
        return;
    }

    QString error;
    if (!InspectionRepository::saveFrameForTask(taskId,
                                                frameId,
                                                timestampNs,
                                                width,
                                                height,
                                                rangePixelFormat,
                                                intensityPixelFormat,
                                                hasRange,
                                                hasIntensity,
                                                &error)) {
        appendEventLog("ERROR", QString("保存扫描帧失败: %1").arg(error));
    }
}

void MainWindow::flushPendingDatabaseWrites()
{
    const QSet<quint64> pendingTaskIds = m_pendingDatabaseTaskIds;
    const QVector<PendingScanFrame> pendingFrames = m_pendingDatabaseFrames;
    m_pendingDatabaseTaskIds.clear();
    m_pendingDatabaseFrames.clear();

    for (quint64 taskId : pendingTaskIds) {
        persistTaskRecord(taskId);
    }
    for (const PendingScanFrame &frame : pendingFrames) {
        persistScanFrame(frame.taskId,
                         frame.frameId,
                         frame.timestampNs,
                         frame.width,
                         frame.height,
                         frame.rangePixelFormat,
                         frame.intensityPixelFormat,
                         frame.hasRange,
                         frame.hasIntensity);
    }

    if (!pendingTaskIds.isEmpty() || !pendingFrames.isEmpty()) {
        LOG("[DB] 启动期缓存补写完成: tasks=%d frames=%d",
            pendingTaskIds.size(), pendingFrames.size());
    }
}

void MainWindow::appendEventLog(const QString &level, const QString &message)
{
    if (m_drawer) {
        m_drawer->appendEvent(level, message);
        // 新日志可能触发抽屉内部布局重算，保持它在最上层。
        if (m_drawer->isVisible()) {
            m_drawer->raise();
        }
    }

    const QByteArray levelBytes = level.toUtf8();
    const QByteArray messageBytes = message.toUtf8();
    LOG("[%s] %s", levelBytes.constData(), messageBytes.constData());
}

void MainWindow::handleMillingProgress(quint64 taskId,
                                       float progress,
                                       bool finished,
                                       bool success,
                                       const QString &message)
{
    if (taskId == 0) {
        LOG("[ROS][Progress] ignored progress with invalid task_id=0");
        return;
    }

    if (taskId != m_activeBackendTaskId) {
        const bool belongsToFinishedTask =
            m_activeTaskFinished && taskId == m_currentBackendTaskId;
        if (!belongsToFinishedTask) {
            PendingMillingProgress &pending = m_pendingMillingProgress[taskId];
            if (!pending.valid || !pending.finished || finished) {
                if (pending.valid && !pending.finished && !finished &&
                    progress < pending.progress) {
                    progress = pending.progress;
                }
                pending = {taskId, progress, finished, success, message, true};
            }
        }

        if (m_lastLoggedIgnoredProgressTaskId != taskId) {
            LOG("[ROS][Progress] %s task progress: incoming_task=%llu active_task=%llu "
                "displayed_task=%llu finished=%d progress=%.2f",
                belongsToFinishedTask ? "ignored completed" : "deferred non-active",
                static_cast<unsigned long long>(taskId),
                static_cast<unsigned long long>(m_activeBackendTaskId),
                static_cast<unsigned long long>(m_currentBackendTaskId),
                finished ? 1 : 0,
                double(progress));
            m_lastLoggedIgnoredProgressTaskId = taskId;
        }
        return;
    }

    applyMillingProgress(taskId, progress, finished, success, message);
}

void MainWindow::applyMillingProgress(quint64 taskId,
                                      float progress,
                                      bool finished,
                                      bool success,
                                      const QString &message)
{
    if (!finished && m_lastMillingProgress >= 0.0f && progress < m_lastMillingProgress) {
        LOG("[ROS][Progress] ignored regressive progress: task=%llu current=%.2f incoming=%.2f",
            static_cast<unsigned long long>(taskId),
            double(m_lastMillingProgress),
            double(progress));
        return;
    }

    if (m_machinePanel) {
        m_machinePanel->setTaskProgress(progress);
    }
    if (m_particlePanel) {
        m_particlePanel->setTaskProgress(progress);
    }
    m_lastMillingProgress = progress;
    if (m_imagePanel) {
        m_imagePanel->setProgressSummary(m_activeCompletedPath, m_activePathTotal, progress);
    }

    if (!finished) {
        return;
    }

    m_activeTaskFinished = true;
    m_activeBackendTaskId = 0;
    if (m_machinePanel) {
        m_machinePanel->setTaskFinished(success);
    }
    if (success && m_particlePanel) {
        m_particlePanel->setTaskProgress(100.0f);
    }
    if (success && !m_activePaths.isEmpty()) {
        // 任务成功结束：所有路径记为完成，KPI 与队列状态收口
        m_activeCompletedPath = m_activePaths.size();
        updateKpis();
        if (m_queuePanel) {
            m_queuePanel->setPathProgress(m_activeCompletedPath, 0);
        }
        if (m_particlePanel) {
            m_particlePanel->setPathProgress(m_activeCompletedPath, 0);
        }
        if (m_imagePanel) {
            m_imagePanel->setProgressSummary(m_activeCompletedPath, m_activePathTotal, progress);
        }
    }
    if (m_imagePanel) {
        m_imagePanel->clearToolPose(); // drop bit disc + trail when task ends
    }

    const QString level = success ? QStringLiteral("INFO")
                                  : QStringLiteral("ERROR");
    const QString head = success ? QStringLiteral("任务 #%1 已完成")
                                 : QStringLiteral("任务 #%1 失败");
    QString text = head.arg(taskId);
    if (!message.isEmpty()) {
        text += QStringLiteral("：") + message;
    }
    appendEventLog(level, text);
}

void MainWindow::toggleMaximized()
{
    isMaximized() ? showNormal() : showMaximized();
    if (m_titleBar) {
        m_titleBar->setMaximizedState(isMaximized());
    }
}

void MainWindow::setSimulationRunning(bool running)
{
    if (m_simulationRunning == running) {
        return;
    }

    m_simulationRunning = running;
    if (m_navBar) {
        m_navBar->setMillingRunning(running);
    }
    if (running) {
        m_waveformTimer->start(100);
        m_simulationProgressTimer->start();
        appendEventLog("INFO", "调试模拟数据已启动");
    } else {
        m_waveformTimer->stop();
        m_simulationProgressTimer->stop();
        if (m_demoPoseTimer) {
            m_demoPoseTimer->stop();
        }
        appendEventLog("INFO", "调试模拟数据已停止");
    }
}

void MainWindow::onWaveformUpdate()
{
    // Smooth simulated curves: no large random jumps that paint as sawtooth bands.
    const double speed = 2000.0 + 180.0 * qSin(m_currentT * 0.35)
                         + 40.0 * qSin(m_currentT * 1.1);
    const double torque = 15.0 + 2.2 * qCos(m_currentT * 0.45)
                          + 0.6 * qSin(m_currentT * 0.9);
    appendTelemetrySample(speed, torque);
}

void MainWindow::appendTelemetrySample(double speed, double torque)
{
    if (m_dataStatisticsPanel) {
        m_dataStatisticsPanel->appendSpindleSample(speed, torque);
    }
    m_currentT += 0.1;
}

void MainWindow::onSimulationProgressUpdate()
{
    if (!m_debugFakeActive || !m_ros2Bridge) {
        setSimulationRunning(false);
        return;
    }

    m_simulationProgress = qMin(100.0f, m_simulationProgress + 2.0f);
    const bool finished = m_simulationProgress >= 100.0f;
    emit m_ros2Bridge->millingProgressReceived(
        9001,
        m_simulationProgress,
        finished,
        finished,
        finished ? QStringLiteral("调试模式：模拟完成") : QString());

    if (finished) {
        setSimulationRunning(false);
    }
}

void MainWindow::resetDemoPoseAnimation()
{
    m_demoPathIndex = 0;
    m_demoPathT = 0.0;
}

void MainWindow::onDemoPoseUpdate()
{
    if (!m_debugFakeActive || m_debugPaths.isEmpty() || !m_imagePanel) {
        if (m_demoPoseTimer) {
            m_demoPoseTimer->stop();
        }
        return;
    }

    if (m_demoPathIndex >= m_debugPaths.size()) {
        // Hold on last cut tip; progress timer will finish the task.
        return;
    }

    const MillingPathVM &seg = m_debugPaths.at(m_demoPathIndex);
    const double u = seg.uStart + (seg.uEnd - seg.uStart) * m_demoPathT;
    const double v = seg.vStart + (seg.vEnd - seg.vStart) * m_demoPathT;
    const double xMm = m_debugXOffsetMm + u * m_debugXScaleMm;
    const double yMm = m_debugYOffsetMm + v * m_debugYScaleMm;
    const double zMm = seg.doCut ? -2.5 : 5.0;
    const bool leaveTrace = seg.doCut;

    m_imagePanel->setToolPose(xMm, yMm, zMm, /*valid=*/true, leaveTrace);
    m_imagePanel->setPathProgress(m_demoPathIndex, m_demoPathIndex + 1);

    if (m_ros2Bridge) {
        PlcFeedbackVM feedback;
        feedback.statusWord = 12;
        feedback.completedPath = quint16(m_demoPathIndex);
        feedback.currentPath = quint16(m_demoPathIndex + 1);
        feedback.pathParams.xPos = float(xMm);
        feedback.pathParams.yPos = float(yMm);
        feedback.pathParams.zPos = float(zMm);
        feedback.pathParams.xSpeed = seg.xSpeed;
        feedback.pathParams.ySpeed = seg.ySpeed;
        feedback.pathParams.zSpeed = leaveTrace ? -1.25f : 0.0f;
        // Smooth time-sampled fake curves (not flat steps that paint as bands).
        const double phase = m_demoPathIndex * 1.7 + m_demoPathT * 6.28318530718;
        if (leaveTrace) {
            feedback.pathParams.spindleSpeed =
                float(2780.0 + 90.0 * std::sin(phase) + 35.0 * std::sin(phase * 2.3));
            feedback.pathParams.spindleTorque =
                float(15.6 + 1.8 * std::cos(phase * 0.9) + 0.7 * std::sin(phase * 3.1));
        } else {
            feedback.pathParams.spindleSpeed =
                float(580.0 + 40.0 * std::sin(phase * 0.5));
            feedback.pathParams.spindleTorque =
                float(1.8 + 0.35 * std::cos(phase * 0.7));
        }
        emit m_ros2Bridge->plcFeedbackReceived(feedback);
    }

    // Advance ~14 image pixels per tick along the segment.
    const double segLen = std::hypot(double(seg.uEnd - seg.uStart),
                                     double(seg.vEnd - seg.vStart));
    const double stepT = (segLen > 1.0) ? (14.0 / segLen) : 1.0;
    m_demoPathT += stepT;
    if (m_demoPathT >= 1.0) {
        m_demoPathT = 0.0;
        ++m_demoPathIndex;
    }
}

void MainWindow::startDemoMilling()
{
    if (!m_debugFakeActive) {
        appendEventLog(QStringLiteral("WARN"),
                       QStringLiteral("请先按 Ctrl+T 进入演示模式后再控制打磨演示"));
        return;
    }

    // 上一轮已经跑完：先复位到起点，再开始新一轮。
    if (m_simulationProgress >= 100.0f
        || (m_demoPathIndex >= m_debugPaths.size() && m_demoPathT <= 0.0)) {
        resetDemoPoseAnimation();
        m_currentT = 0.0;
        m_simulationProgress = 0.0f;
        m_lastMillingProgress = -1.0f;
        m_activeBackendTaskId = 9001;
        m_activeTaskFinished = false;
        if (m_imagePanel) {
            m_imagePanel->clearGrindLayer();
            m_imagePanel->clearToolPose();
            m_imagePanel->setPathProgress(0, 0);
        }
        if (m_ros2Bridge) {
            emit m_ros2Bridge->millingProgressReceived(9001, 0.0f, false, false, QString());
        }
    }

    if (m_demoPoseTimer) {
        m_demoPoseTimer->start();
    }
    setSimulationRunning(true);
    appendEventLog(QStringLiteral("INFO"), QStringLiteral("演示打磨已开始"));
}

void MainWindow::pauseDemoMilling()
{
    if (!m_debugFakeActive) {
        return;
    }
    if (m_demoPoseTimer) {
        m_demoPoseTimer->stop();
    }
    setSimulationRunning(false);
    appendEventLog(QStringLiteral("INFO"), QStringLiteral("演示打磨已暂停在当前进度"));
}

void MainWindow::stopDemoMilling()
{
    if (!m_debugFakeActive) {
        return;
    }
    if (m_demoPoseTimer) {
        m_demoPoseTimer->stop();
    }
    setSimulationRunning(false);
    resetDemoPoseAnimation();
    m_currentT = 0.0;
    m_simulationProgress = 0.0f;
    // 允许进度从 0 重新开始（否则会被“进度回退”保护拦住，进度条停在旧值）。
    m_lastMillingProgress = -1.0f;
    m_activeTaskFinished = false;
    m_activeBackendTaskId = 9001;
    m_activeCompletedPath = 0;
    m_activePathTotal = m_debugPaths.size();
    if (m_machinePanel) {
        m_machinePanel->setTask(9001, m_debugPaths.size());
    }
    if (m_particlePanel) {
        m_particlePanel->setTaskProgress(0.0f);
        m_particlePanel->setPathProgress(0, 0);
    }
    if (m_queuePanel) {
        m_queuePanel->setPathProgress(0, 0);
    }
    if (m_imagePanel) {
        m_imagePanel->clearGrindLayer();
        m_imagePanel->clearToolPose();
        m_imagePanel->setPathProgress(0, 0);
        m_imagePanel->setProgressSummary(0, m_debugPaths.size(), 0.0f);
    }
    if (m_ros2Bridge) {
        emit m_ros2Bridge->millingProgressReceived(9001, 0.0f, false, false, QString());
    }
    appendEventLog(QStringLiteral("INFO"), QStringLiteral("演示打磨已停止并复位，等待新的开始"));
}

