#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AppStyle.h"
#include "ElaMessageBar.h"
#include "ElaStatusBar.h"
#include "EventLogPanel.h"
#include "ImageDisplayPanel.h"
#include "Logger.h"
#include "RunDataPanel.h"
#include "RunningStatusPanel.h"
#include "TelemetryWebSocketServer.h"
#include "TitleBar.h"
#include "UiHelpers.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QEvent>
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
    if (m_telemetryServer) {
        m_telemetryServer->stop();
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

    startTelemetryServer();
}

void MainWindow::connectTitleBar()
{
    connect(m_titleBar, &TitleBar::settingsRequested, this, [this]() {
        ElaMessageBar::information(ElaMessageBarType::BottomRight, "系统设置", "设置功能后续接入。", 2000, this);
        appendEventLog("INFO", "用户打开了系统设置");
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

void MainWindow::startTelemetryServer()
{
    m_telemetryServer = new TelemetryWebSocketServer(this);
    connect(m_telemetryServer, &TelemetryWebSocketServer::telemetryReceived,
            this, &MainWindow::appendTelemetrySample);
    connect(m_telemetryServer, &TelemetryWebSocketServer::infoMessage, this, [this](const QString &message) {
        appendEventLog("INFO", message);
    });
    connect(m_telemetryServer, &TelemetryWebSocketServer::errorMessage, this, [this](const QString &message) {
        appendEventLog("ERROR", message);
    });
    m_telemetryServer->start(9002);
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
