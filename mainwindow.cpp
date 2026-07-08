#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AppStyle.h"
#include "ElaMessageBar.h"
#include "ElaStatusBar.h"
#include "EventLogPanel.h"
#include "HistoryDialog.h"
#include "ImageDisplayPanel.h"
#include "Logger.h"
#include "Ros2Bridge.h"
#include "RunDataPanel.h"
#include "RunningStatusPanel.h"
#include "TeachingDialog.h"
#include "TitleBar.h"
#include "UiHelpers.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QEvent>
#include <QMenuBar>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>

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

    auto *taskFlowPanel = createPanel("任务流程", nullptr, rightColumn);
    auto *taskFlowLayout = qobject_cast<QVBoxLayout *>(taskFlowPanel->layout());
    taskFlowLayout->addStretch();
    rightColumnLayout->addWidget(taskFlowPanel, 2);

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
        m_historyDialog->show();
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
                   const QString &shmName,
                   quint64 offset,
                   quint64 dataSize,
                   quint32 width,
                   quint32 height,
                   quint32 pixelFormat) {
                appendEventLog("INFO",
                               QString("相机图像通知 type=%1 frame=%2 shm=%3 offset=%4 size=%5 %6x%7 fmt=%8")
                                   .arg(imageType)
                                   .arg(frameId)
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
                   quint64 timestampNs,
                   quint32 width,
                   quint32 height,
                   quint32 pixelFormat) {
                Q_UNUSED(width);
                Q_UNUSED(height);
                Q_UNUSED(pixelFormat);
                if (m_imagePanel) {
                    m_imagePanel->updateScanFrame(range, intensity, frameId, timestampNs);
                }
            });
    connect(m_ros2Bridge, &Ros2Bridge::algorithmResultReceived, this,
            [this](bool success, quint64 frameId, quint64 taskId, const QString &message) {
                appendEventLog(success ? "INFO" : "ERROR",
                               QString("后端结果 frame=%1 task=%2 message=%3")
                                   .arg(frameId)
                                   .arg(taskId)
                                   .arg(message));
            });
    connect(m_ros2Bridge, &Ros2Bridge::backendStateReceived, this,
            [this](const QString &state) {
                appendEventLog("INFO", QString("后端状态: %1").arg(state));
            });

    m_ros2Bridge->start();
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
