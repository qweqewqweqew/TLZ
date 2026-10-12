#include "app/mainwindow.h"
#include "ui_mainwindow.h"

#include "app/AppStyle.h"
#include "ui/panels/DataStatisticsPanel.h"
#include "ui/panels/DeviceStatusPanel.h"
#include "ElaMessageBar.h"
#include "ui/panels/EventLogPanel.h"
#include "ui/dialogs/HistoryDialog.h"
#include "camera/HikCameraClient.h"
#include "ui/panels/ImageDisplayPanel.h"
#include "data/InspectionRepository.h"
#include "app/Logger.h"
#include "bridge/NodeLifecycleMonitor.h"
#include "bridge/MillingPathVM.h"
#include "bridge/PlcFeedbackVM.h"
#include "vision/PointCloudFrame.h"
#include "bridge/Ros2Bridge.h"
#include "ui/panels/RunDataPanel.h"
#include "ui/panels/RunningStatusPanel.h"
#include "ui/panels/TaskFlowPanel.h"
#include "ui/dialogs/BackendParamsDialog.h"
#include "app/TitleBar.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QImage>
#include <QCloseEvent>
#include <QEvent>
#include <QKeySequence>
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

void appendAncestorLauncherConfigs(QStringList *candidates, const QString &startDir)
{
    QDir dir(startDir);
    for (int i = 0; i < 8; ++i) {
        const QString configPath = QFileInfo(dir.filePath(QStringLiteral("node_launcher.ini")))
                                       .absoluteFilePath();
        if (QFileInfo::exists(configPath) && !candidates->contains(configPath)) {
            candidates->append(configPath);
        }
        if (!dir.cdUp()) {
            break;
        }
    }
}

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
    if (m_waveformTimer) {
        m_waveformTimer->stop();
    }
    if (m_simulationProgressTimer) {
        m_simulationProgressTimer->stop();
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
        confirmDialog.setStyleSheet(R"(
            QMessageBox {
                background-color: #202B37;
            }
            QMessageBox QLabel {
                color: #E6EEF5;
                min-width: 360px;
                font-size: 14px;
            }
        )");
        if (auto *exitButton = qobject_cast<QPushButton *>(
                confirmDialog.button(QMessageBox::Yes))) {
            exitButton->setObjectName(QStringLiteral("exitConfirmButton"));
            exitButton->setStyleSheet(R"(
                QPushButton#exitConfirmButton {
                    min-width: 90px;
                    min-height: 34px;
                    padding: 2px 14px;
                    color: #FFFFFF;
                    background-color: #8F2D2D;
                    border: 1px solid #D05A5A;
                    border-radius: 5px;
                    font-size: 14px;
                    font-weight: 600;
                }
                QPushButton#exitConfirmButton:hover {
                    background-color: #A53A3A;
                }
                QPushButton#exitConfirmButton:pressed {
                    background-color: #742323;
                }
                QPushButton#exitConfirmButton:focus {
                    border: 2px solid #F09A9A;
                }
            )");
        }
        if (auto *cancelButton = qobject_cast<QPushButton *>(
                confirmDialog.button(QMessageBox::No))) {
            cancelButton->setObjectName(QStringLiteral("cancelExitButton"));
            cancelButton->setStyleSheet(R"(
                QPushButton#cancelExitButton {
                    min-width: 90px;
                    min-height: 34px;
                    padding: 2px 14px;
                    color: #FFFFFF;
                    background-color: #245B83;
                    border: 1px solid #4B9BD3;
                    border-radius: 5px;
                    font-size: 14px;
                    font-weight: 600;
                }
                QPushButton#cancelExitButton:hover {
                    background-color: #2D6F9F;
                }
                QPushButton#cancelExitButton:pressed {
                    background-color: #1E4B6B;
                }
                QPushButton#cancelExitButton:focus {
                    border: 2px solid #79C2F2;
                }
            )");
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
    setWindowTitle("阴级铜表面粒子智能修整系统");
    resize(1680, 980);
    setMinimumSize(1500, 920);
    menuBar()->hide();
    setStyleSheet(mainWindowStyleSheet());

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(10, 10, 10, 8);
    rootLayout->setSpacing(8);
    setCentralWidget(root);

    m_titleBar = new TitleBar(root);
    rootLayout->addWidget(m_titleBar);
    connectTitleBar();

    m_contentStack = new QStackedWidget(root);
    rootLayout->addWidget(m_contentStack, 1);

    m_monitorPage = new QWidget(m_contentStack);
    auto *mainLayout = new QHBoxLayout(m_monitorPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);

    auto *leftWorkspace = new QWidget(m_monitorPage);
    auto *leftWorkspaceLayout = new QGridLayout(leftWorkspace);
    leftWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
    leftWorkspaceLayout->setHorizontalSpacing(10);
    leftWorkspaceLayout->setVerticalSpacing(10);

    m_statusPanel = new RunningStatusPanel(leftWorkspace);
    leftWorkspaceLayout->addWidget(m_statusPanel, 0, 0);

    m_imagePanel = new ImageDisplayPanel(leftWorkspace);
    leftWorkspaceLayout->addWidget(m_imagePanel, 0, 1);

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
    connect(m_hikCameraClient, &HikCameraClient::connectionStatusChanged, this,
            [this](bool connected, const QString &message) {
                if (m_imagePanel) {
                    m_imagePanel->setLiveCameraStatus(connected, message);
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

    m_eventLogPanel = new EventLogPanel(leftWorkspace);
    leftWorkspaceLayout->addWidget(m_eventLogPanel, 1, 0);

    m_dataStatisticsPanel = new DataStatisticsPanel(leftWorkspace);
    leftWorkspaceLayout->addWidget(m_dataStatisticsPanel, 1, 1);

    leftWorkspaceLayout->setColumnStretch(0, 1);
    leftWorkspaceLayout->setColumnStretch(1, 0);
    leftWorkspaceLayout->setRowStretch(0, 1);
    leftWorkspaceLayout->setRowStretch(1, 0);

    mainLayout->addWidget(leftWorkspace, 3);

    auto *rightColumn = new QWidget(m_monitorPage);
    rightColumn->setMinimumWidth(420);
    auto *rightColumnLayout = new QVBoxLayout(rightColumn);
    rightColumnLayout->setContentsMargins(0, 0, 0, 0);
    rightColumnLayout->setSpacing(10);

    m_runDataPanel = new RunDataPanel(rightColumn);
    rightColumnLayout->addWidget(m_runDataPanel, 2);

    m_taskFlowPanel = new TaskFlowPanel(rightColumn);
    rightColumnLayout->addWidget(m_taskFlowPanel, 5);

    connect(m_imagePanel, &ImageDisplayPanel::millingPathClicked, this,
            [this](int pathIndex) {
                if (m_taskFlowPanel) {
                    m_taskFlowPanel->selectPath(pathIndex);
                }
            });

    mainLayout->addWidget(rightColumn, 1);
    m_contentStack->addWidget(m_monitorPage);
    m_settingsPage = new QWidget(m_contentStack);
    m_settingsPage->setObjectName("settingsPage");
    m_settingsPage->setStyleSheet(QStringLiteral("QWidget#settingsPage{background:%1;}")
                                      .arg(Theme::kWindowBg));
    auto *settingsLayout = new QVBoxLayout(m_settingsPage);
    settingsLayout->setContentsMargins(20, 16, 20, 16);
    settingsLayout->setSpacing(16);
    settingsLayout->addStretch(1);
    m_contentStack->addWidget(m_settingsPage);

    // 横跨主窗口底部的状态栏：左侧合并状态文字，右侧三个状态指示控件。
    m_deviceStatusPanel = new DeviceStatusPanel(root);
    rootLayout->addWidget(m_deviceStatusPanel, 0);

    QTimer::singleShot(350, this, [this]() {
        ElaMessageBar::success(ElaMessageBarType::BottomRight, "界面初始化", "ElaWidgetTools 组件已接入。", 1800, this);
        appendEventLog("INFO", "界面初始化完成");
    });

    m_waveformTimer = new QTimer(this);
    connect(m_waveformTimer, &QTimer::timeout, this, &MainWindow::onWaveformUpdate);
    m_simulationProgressTimer = new QTimer(this);
    m_simulationProgressTimer->setInterval(1000);
    connect(m_simulationProgressTimer, &QTimer::timeout,
            this, &MainWindow::onSimulationProgressUpdate);

    startRos2Bridge();
    if (auto *settingsLayout = qobject_cast<QVBoxLayout *>(m_settingsPage->layout())) {
        auto *cardGrid = new QWidget(m_settingsPage);
        auto *cardGridLayout = new QHBoxLayout(cardGrid);
        cardGridLayout->setContentsMargins(0, 0, 0, 0);
        cardGridLayout->setSpacing(16);
        cardGridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

        m_backendParamsDialog = new BackendParamsDialog(m_ros2Bridge, cardGrid);
        cardGridLayout->addWidget(m_backendParamsDialog, 0, Qt::AlignTop);
        cardGridLayout->addStretch(1);

        settingsLayout->insertWidget(0, cardGrid, 0, Qt::AlignLeft | Qt::AlignTop);
    }
}

void MainWindow::connectTitleBar()
{
    connect(m_titleBar, &TitleBar::settingsRequested, this, [this]() {
        if (!m_contentStack || !m_settingsPage) {
            return;
        }

        const bool pageChanged = m_contentStack->currentWidget() != m_settingsPage;
        m_contentStack->setCurrentWidget(m_settingsPage);
        m_titleBar->setWorkspacePage(TitleBar::WorkspacePage::Settings);
        if (pageChanged) {
            appendEventLog("INFO", "用户打开了设置页面");
        }
    });
    connect(m_titleBar, &TitleBar::homeRequested, this, [this]() {
        if (!m_contentStack || !m_monitorPage) {
            return;
        }

        const bool pageChanged = m_contentStack->currentWidget() != m_monitorPage;
        m_contentStack->setCurrentWidget(m_monitorPage);
        m_titleBar->setWorkspacePage(TitleBar::WorkspacePage::Home);
        if (pageChanged) {
            appendEventLog("INFO", "用户返回了首页");
        }
    });
    connect(m_titleBar, &TitleBar::historyRequested, this, [this]() {
        if (!m_contentStack || !m_monitorPage) {
            return;
        }

        const bool historyVisible = m_historyDialog
                                    && m_contentStack->currentWidget() == m_historyDialog;
        if (historyVisible) {
            m_titleBar->setWorkspacePage(TitleBar::WorkspacePage::History);
            return;
        }

        if (!m_historyDialog) {
            m_historyDialog = new HistoryDialog(m_contentStack);
            m_contentStack->addWidget(m_historyDialog);
        }
        m_historyDialog->reloadRecords();
        m_contentStack->setCurrentWidget(m_historyDialog);
        m_titleBar->setWorkspacePage(TitleBar::WorkspacePage::History);
        appendEventLog("INFO", "用户打开了历史记录");
    });
    connect(m_titleBar, &TitleBar::startNodesRequested, this, &MainWindow::startNodes);
    connect(m_titleBar, &TitleBar::stopNodesRequested, this, &MainWindow::stopNodes);
    connect(m_titleBar, &TitleBar::emergencyStopRequested, this, [this]() {
        ElaMessageBar::error(
            ElaMessageBarType::BottomRight,
            "急停请求已触发",
            "请立即确认设备与 PLC 状态。",
            3500,
            this);
        appendEventLog("ERROR", "操作员双击触发急停请求");
    });
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

QString MainWindow::resolveLauncherConfigPath() const
{
    QStringList candidates;
    appendAncestorLauncherConfigs(&candidates, QDir::currentPath());
    appendAncestorLauncherConfigs(&candidates, QCoreApplication::applicationDirPath());

    for (const QString &path : candidates) {
        const QDir root(QFileInfo(path).absolutePath());
        if (QFileInfo::exists(root.filePath(QStringLiteral("CMakeLists.txt")))) {
            return path;
        }
    }
    if (!candidates.isEmpty()) {
        return candidates.front();
    }
    return QString();
}

QString MainWindow::resolveLauncherScript(const QString &key) const
{
    const QString configPath = resolveLauncherConfigPath();
    if (configPath.isEmpty()) {
        return QString();
    }

    QSettings settings(configPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    const QString configured = settings.value(QStringLiteral("Launcher/%1").arg(key))
                                   .toString()
                                   .trimmed();
    if (configured.isEmpty()) {
        return QString();
    }

    QFileInfo info(configured);
    if (info.isRelative()) {
        info.setFile(QDir(QFileInfo(configPath).absolutePath()).filePath(configured));
    }
    if (!info.exists()) {
        return QString();
    }
    return info.absoluteFilePath();
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
            });
    connect(m_ros2Bridge, &Ros2Bridge::cameraStatusReceived, this,
            [this](bool connected, const QString &cameraId, const QString &message) {
                if (m_deviceStatusPanel) {
                    m_deviceStatusPanel->setCameraStatus(connected, cameraId, message);
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
                if (m_imagePanel) {
                    m_imagePanel->setMillingPaths(paths);
                    m_imagePanel->setAlgorithmImagePath(drawimagePath);
                }
                if (m_taskFlowPanel) {
                    m_taskFlowPanel->setMillingPaths(paths);
                }
                if (m_runDataPanel) {
                    m_runDataPanel->setMillingTask(taskId, pathTotal);
                    m_runDataPanel->setMaxParticleHeight(maxParticleHeight);
                    m_runDataPanel->setParticleBoardGrade(particleBoardGrade);
                    int cutCount = 0;
                    int emptyCount = 0;
                    for (const MillingPathVM &path : paths) {
                        if (path.doCut) {
                            ++cutCount;
                        } else {
                            ++emptyCount;
                        }
                    }
                    m_runDataPanel->setCutEmptyPathCounts(cutCount, emptyCount);
                }
                if (m_statusPanel) {
                    m_statusPanel->setTaskProgress(0.0f);
                }
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
                if (m_statusPanel) {
                    m_statusPanel->setPlcFeedback(feedback);
                }
                if (m_dataStatisticsPanel) {
                    m_dataStatisticsPanel->appendSpindleSample(
                        feedback.pathParams.spindleSpeed,
                        feedback.pathParams.spindleTorque);
                }
                if (m_deviceStatusPanel) {
                    m_deviceStatusPanel->setBackendOnline(
                        QStringLiteral("已收到后端发布的 PLC 反馈"));
                    m_deviceStatusPanel->setPlcStatus(feedback.statusWord,
                                                      feedback.faultCode);
                }
                if (m_runDataPanel) {
                    m_runDataPanel->setPlcPathCounts(int(feedback.completedPath),
                                                     int(feedback.currentPath));
                }
                if (m_taskFlowPanel) {
                    m_taskFlowPanel->setActivePath(int(feedback.currentPath));
                }
            });

    m_ros2Bridge->start();

    // 调试快捷键 Ctrl+T：切换演示模式。
    // 第一次按：加载相机保存的本地图像 + 示例任务数据。
    // 再按一次：清掉假图，任务条复位为"待命"。
    auto *fakeShortcut = new QShortcut(QKeySequence("Ctrl+T"), this);
    connect(fakeShortcut, &QShortcut::activated, this, [this]() {
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
            frame.intensity = intensity;
            frame.frameId = 9001;
            frame.intensityPixelFormat = CV_8UC1;
            if (m_imagePanel) {
                m_imagePanel->updateScanFrame(frame);
                m_imagePanel->showHeight3D();
            }

            QVector<MillingPathVM> paths;
            paths.push_back({ 80,  60, 260,  60, 3000.0f, 12.0f,  0.0f, true  });
            paths.push_back({260,  60, 260, 160, 3000.0f,  0.0f, 10.0f, false });
            paths.push_back({260, 160,  80, 160, 2800.0f, -12.0f, 0.0f, true  });
            paths.push_back({ 80, 160,  80, 260, 2800.0f,  0.0f, 10.0f, false });
            paths.push_back({ 80, 260, 260, 260, 3200.0f, 15.0f,  0.0f, true  });

            emit m_ros2Bridge->millingPathsReceived(
                /*taskId*/ 9001,
                /*pathTotal*/ paths.size(),
                /*maxParticleHeight*/ 5,
                /*particleBoardGrade*/ QString(),
                /*drawimagePath*/ QString(),
                paths);
            emit m_ros2Bridge->millingProgressReceived(
                9001, 0.0f, /*finished*/ false, /*success*/ false, QString());

            PlcFeedbackVM feedback;
            feedback.statusWord = 12;
            feedback.completedPath = 1;
            feedback.currentPath = 2;
            feedback.pathParams.xPos = 128.40f;
            feedback.pathParams.yPos = 64.25f;
            feedback.pathParams.zPos = -2.50f;
            feedback.pathParams.xSpeed = 12.00f;
            feedback.pathParams.ySpeed = 8.50f;
            feedback.pathParams.zSpeed = -1.25f;
            feedback.pathParams.spindleSpeed = 2860.0f;
            feedback.pathParams.spindleTorque = 16.8f;
            emit m_ros2Bridge->plcFeedbackReceived(feedback);

            m_currentT = 0.0;
            m_simulationProgress = 0.0f;
            setSimulationRunning(true);

            appendEventLog(
                "INFO",
                QStringLiteral("已加载 TestImage（%1 + %2）并进入调试演示模式（Ctrl+T 再按一次取消）")
                    .arg(QFileInfo(rangePath).fileName(),
                         QFileInfo(intensityPath).fileName()));
        } else {
            // 退出演示模式：清假图、任务条复位。
            setSimulationRunning(false);
            m_simulationProgress = 0.0f;
            if (m_imagePanel) {
                m_imagePanel->show2D();
                m_imagePanel->clearMillingPaths();
                m_imagePanel->clearAlgorithmImage();
                m_imagePanel->updateScanFrame(PointCloudFrame());
            }
            if (m_runDataPanel) {
                m_runDataPanel->clearMetrics();
                m_runDataPanel->clearMillingTask();
            }
            if (m_statusPanel) {
                m_statusPanel->clearPlcFeedback();
                m_statusPanel->clearTaskProgress();
            }
            if (m_deviceStatusPanel) {
                m_deviceStatusPanel->clearBackendAndPlcStatus();
            }
            if (m_taskFlowPanel) {
                m_taskFlowPanel->clearCommands();
            }
            m_debugFakeActive = false;
            appendEventLog("INFO", "已退出调试演示模式");
        }
    });

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

void MainWindow::setDatabaseReady(bool ready)
{
    m_databaseReady = ready;
    if (m_databaseReady) {
        flushPendingDatabaseWrites();
    }
}

void MainWindow::persistTaskRecord(quint64 taskId)
{
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
    if (m_eventLogPanel) {
        m_eventLogPanel->appendEvent(level, message);
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

    if (m_runDataPanel) {
        m_runDataPanel->setMillingStatus(finished, success);
    }
    if (m_statusPanel) {
        m_statusPanel->setTaskProgress(progress);
    }
    m_lastMillingProgress = progress;

    if (!finished) {
        return;
    }

    m_activeTaskFinished = true;
    m_activeBackendTaskId = 0;

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
    if (running) {
        m_waveformTimer->start(100);
        m_simulationProgressTimer->start();
        appendEventLog("INFO", "调试模拟数据已启动");
    } else {
        m_waveformTimer->stop();
        m_simulationProgressTimer->stop();
        appendEventLog("INFO", "调试模拟数据已停止");
    }
}

void MainWindow::onWaveformUpdate()
{
    const double speed = 2000 + 800 * qSin(m_currentT * 0.5) + (qrand() % 200);
    const double torque = 15 + 5 * qCos(m_currentT * 0.7) + (qrand() % 100) * 0.05;
    appendTelemetrySample(speed, torque);
}

void MainWindow::appendTelemetrySample(double speed, double torque)
{
    if (m_statusPanel) {
        m_statusPanel->appendSample(m_currentT, speed, torque);
    }
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
