#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AppStyle.h"
#include "ElaMessageBar.h"
#include "ElaStatusBar.h"
#include "EventLogPanel.h"
#include "HistoryDialog.h"
#include "ImageDisplayPanel.h"
#include "InspectionRepository.h"
#include "Logger.h"
#include "MillingPathVM.h"
#include "PlcFeedbackVM.h"
#include "PlcPathCommandParamsVM.h"
#include "Ros2Bridge.h"
#include "RunDataPanel.h"
#include "RunningStatusPanel.h"
#include "TaskFlowPanel.h"
#include "TeachingDialog.h"
#include "TitleBar.h"
#include "UiHelpers.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QShortcut>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>
#include <QVector>

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
    if (m_ros2Bridge) {
        m_ros2Bridge->stop();
    }
    delete ui;
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
    setWindowTitle("铜粒子打磨系统");
    resize(1680, 980);
    setMinimumSize(1500, 920);
    menuBar()->hide();
    setStatusBar(new ElaStatusBar(this));
    setStyleSheet(mainWindowStyleSheet());

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(10, 10, 10, 8);
    rootLayout->setSpacing(8);
    setCentralWidget(root);

    m_titleBar = new TitleBar(root);
    rootLayout->addWidget(m_titleBar);
    connectTitleBar();

    auto *mainLayout = new QHBoxLayout();
    mainLayout->setSpacing(10);
    rootLayout->addLayout(mainLayout, 1);

    auto *leftWorkspace = new QWidget(root);
    auto *leftWorkspaceLayout = new QVBoxLayout(leftWorkspace);
    leftWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
    leftWorkspaceLayout->setSpacing(10);

    auto *topWorkspaceLayout = new QHBoxLayout();
    topWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
    topWorkspaceLayout->setSpacing(10);
    leftWorkspaceLayout->addLayout(topWorkspaceLayout, 1);

    m_statusPanel = new RunningStatusPanel(leftWorkspace);
    topWorkspaceLayout->addWidget(m_statusPanel, 1);

    m_imagePanel = new ImageDisplayPanel(leftWorkspace);
    topWorkspaceLayout->addWidget(m_imagePanel);

    m_eventLogPanel = new EventLogPanel(leftWorkspace);
    leftWorkspaceLayout->addWidget(m_eventLogPanel, 0);

    mainLayout->addWidget(leftWorkspace, 3);

    auto *rightColumn = new QWidget(root);
    rightColumn->setMinimumWidth(420);
    auto *rightColumnLayout = new QVBoxLayout(rightColumn);
    rightColumnLayout->setContentsMargins(0, 0, 0, 0);
    rightColumnLayout->setSpacing(10);

    m_runDataPanel = new RunDataPanel(rightColumn);
    rightColumnLayout->addWidget(m_runDataPanel, 1);

    m_taskFlowPanel = new TaskFlowPanel(rightColumn);
    rightColumnLayout->addWidget(m_taskFlowPanel, 2);

    mainLayout->addWidget(rightColumn, 1);

    QTimer::singleShot(350, this, [this]() {
        ElaMessageBar::success(ElaMessageBarType::BottomRight, "界面初始化", "ElaWidgetTools 组件已接入。", 1800, this);
        appendEventLog("INFO", "界面初始化完成");
    });

    m_waveformTimer = new QTimer(this);
    connect(m_waveformTimer, &QTimer::timeout, this, &MainWindow::onWaveformUpdate);

    startRos2Bridge();
}

void MainWindow::connectTitleBar()
{
    connect(m_titleBar, &TitleBar::settingsRequested, this, [this]() {
        ElaMessageBar::information(ElaMessageBarType::BottomRight, "系统设置", "设置功能后续接入。", 2000, this);
        appendEventLog("INFO", "用户打开了系统设置");
    });
    connect(m_titleBar, &TitleBar::teachingModuleRequested, this, [this]() {
        if (!m_teachingDialog) {
            m_teachingDialog = new TeachingDialog(m_ros2Bridge, this);
        }
        m_teachingDialog->show();
        m_teachingDialog->raise();
        m_teachingDialog->activateWindow();
    });
    connect(m_titleBar, &TitleBar::historyRequested, this, [this]() {
        if (!m_historyDialog) {
            m_historyDialog = new HistoryDialog(this);
        }
        m_historyDialog->showMaximized();
        m_historyDialog->raise();
        m_historyDialog->activateWindow();
        appendEventLog("INFO", "用户打开了历史记录");
    });
    connect(m_titleBar, &TitleBar::simulationToggled, this, &MainWindow::setSimulationRunning);
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRestoreRequested, this, &MainWindow::toggleMaximized);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::dragRequested, this, [this](const QPoint &globalTopLeft) {
        move(globalTopLeft);
    });
    m_titleBar->setMaximizedState(isMaximized());
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
            [this](const QImage &range,
                   const QImage &intensity,
                   quint64 frameId,
                   quint64 taskId,
                   quint64 timestampNs,
                   quint32 width,
                   quint32 height,
                   quint32 pixelFormat) {
                if (m_imagePanel) {
                    m_imagePanel->updateScanFrame(range, intensity, frameId, timestampNs);
                }

                // 帧落库：消息没带 task_id 时回退到当前任务（MillingPaths 记录的任务号）
                const quint64 effectiveTaskId = taskId != 0 ? taskId : m_currentBackendTaskId;
                if (effectiveTaskId == 0 || frameId == 0) {
                    return;
                }
                QString err;
                if (!InspectionRepository::saveFrameForTask(effectiveTaskId,
                                                            frameId,
                                                            timestampNs,
                                                            int(width),
                                                            int(height),
                                                            int(pixelFormat),
                                                            !range.isNull(),
                                                            !intensity.isNull(),
                                                            &err)) {
                    appendEventLog("ERROR", QString("保存扫描帧失败: %1").arg(err));
                }
            });
    connect(m_ros2Bridge, &Ros2Bridge::backendStateReceived, this,
            [this](const QString &state) {
                appendEventLog("INFO", QString("后端状态: %1").arg(state));
            });

    connect(m_ros2Bridge, &Ros2Bridge::millingPathsReceived, this,
            [this](quint64 taskId,
                   int pathTotal,
                   int maxParticleHeight,
                   bool calibrationApplied,
                   const QVector<MillingPathVM> &paths) {
                m_currentBackendTaskId = taskId;
                if (taskId != 0) {
                    QString err;
                    if (!InspectionRepository::ensureRecordForTask(taskId, &err)) {
                        appendEventLog("ERROR", QString("创建检测任务记录失败: %1").arg(err));
                    }
                }
                if (m_imagePanel) {
                    m_imagePanel->setMillingPaths(paths, calibrationApplied);
                }
                if (m_runDataPanel) {
                    m_runDataPanel->setMillingTask(taskId, pathTotal,
                                                   calibrationApplied);
                    m_runDataPanel->setMaxParticleHeight(maxParticleHeight);
                }
                appendEventLog("INFO",
                               QString("任务 #%1 开始，共 %2 条路径（%3）")
                                   .arg(taskId)
                                   .arg(pathTotal)
                                   .arg(calibrationApplied ? "已标定" : "未标定"));
            });

    connect(m_ros2Bridge, &Ros2Bridge::millingProgressReceived, this,
            [this](quint64 taskId,
                   float progress,
                   bool finished,
                   bool success,
                   const QString &message) {
                if (m_runDataPanel) {
                    m_runDataPanel->setMillingStatus(finished, success);
                    m_runDataPanel->setMillingProgress(progress);
                }
                if (finished) {
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
            });

    connect(m_ros2Bridge, &Ros2Bridge::plcFeedbackReceived, this,
            [this](const PlcFeedbackVM &feedback) {
                if (m_statusPanel) {
                    m_statusPanel->setPlcFeedback(feedback);
                }
                if (m_runDataPanel) {
                    m_runDataPanel->setPlcPathCounts(int(feedback.completedPath),
                                                     int(feedback.currentPath));
                }
            });

    connect(m_ros2Bridge, &Ros2Bridge::plcPathCommandReceived, this,
            [this](const PlcPathCommandParamsVM &command) {
                if (m_taskFlowPanel) {
                    m_taskFlowPanel->addPlcCommand(command);
                }
            });

    m_ros2Bridge->start();

    // 调试快捷键 Ctrl+T：切换演示模式。
    // 第一次按：注入一张灰底假图 + 5 条示例路径 + 任务进行中，用于纯前端预览效果。
    // 再按一次：清掉假图和路径，任务条复位为"待命"。
    auto *fakeShortcut = new QShortcut(QKeySequence("Ctrl+T"), this);
    connect(fakeShortcut, &QShortcut::activated, this, [this]() {
        if (!m_debugFakeActive) {
            // 灰底假图，尺寸覆盖示例路径坐标范围（最大 260）。
            QImage fake(400, 320, QImage::Format_Grayscale8);
            fake.fill(64);  // 深灰
            if (m_imagePanel) {
                m_imagePanel->updateScanFrame(fake, QImage(), 9001, 0);
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
                /*calibrationApplied*/ true,
                paths);
            emit m_ros2Bridge->millingProgressReceived(
                9001, 0.0f, /*finished*/ false, /*success*/ false, QString());

            PlcPathCommandParamsVM command;
            command.x = 120.50f;
            command.y = 45.00f;
            command.z = -2.50f;
            command.feedSpeed = 80.0f;
            command.feedAmount = 0.30f;
            command.spindleSpeed = 3000.0f;
            command.plungeCount = 2;
            command.plungeAmount = 1;
            command.feedDirection = 10;
            emit m_ros2Bridge->plcPathCommandReceived(command);

            m_debugFakeActive = true;
            appendEventLog("INFO", "进入调试演示模式（Ctrl+T 再按一次取消）");
        } else {
            // 退出演示模式：清路径、清假图、任务条复位。
            if (m_imagePanel) {
                m_imagePanel->clearMillingPaths();
                m_imagePanel->updateScanFrame(QImage(), QImage(), 0, 0);
            }
            if (m_runDataPanel) {
                m_runDataPanel->clearMetrics();
                m_runDataPanel->clearMillingTask();
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
        emit m_ros2Bridge->millingProgressReceived(
            9001, 100.0f, /*finished*/ true, /*success*/ true,
            QStringLiteral("调试模式：模拟完成"));
    });
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
    m_titleBar->setSimulationRunning(running);
    if (running) {
        m_waveformTimer->start(100);
        appendEventLog("INFO", "测试模拟波形已启动");
    } else {
        m_waveformTimer->stop();
        appendEventLog("INFO", "测试模拟波形已停止");
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
    m_statusPanel->appendSample(m_currentT, speed, torque);
    m_currentT += 0.1;
}
