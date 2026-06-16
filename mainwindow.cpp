#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ElaIconButton.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"
#include "ElaProgressBar.h"
#include "ElaStatusBar.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kPanelRadius = 8;

QString pillStyle(const QString &state)
{
    if (state == "ok") {
        return "background:#0f5132;color:#d1fae5;border:1px solid #198754;";
    }
    if (state == "running") {
        return "background:#123c69;color:#dbeafe;border:1px solid #2563eb;";
    }
    if (state == "warn") {
        return "background:#5f4313;color:#fef3c7;border:1px solid #b7791f;";
    }
    if (state == "error") {
        return "background:#651f2a;color:#fee2e2;border:1px solid #dc3545;";
    }
    return "background:#263241;color:#cbd5e1;border:1px solid #405064;";
}

QLabel *makeLabel(const QString &text, const QString &objectName = QString())
{
    auto *label = new QLabel(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    return label;
}

ElaProgressBar *makeProgressBar(int value)
{
    auto *bar = new ElaProgressBar();
    bar->setRange(0, 100);
    bar->setValue(value);
    bar->setTextVisible(false);
    bar->setFixedHeight(8);
    return bar;
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
    delete ui;
}

void MainWindow::buildMainView()
{
    setWindowTitle("铜粒子打磨系统显示端");
    resize(1360, 820);
    menuBar()->hide();
    setStatusBar(new ElaStatusBar(this));
    statusBar()->showMessage("前端已启动 | 后端等待接入 | PLC 通讯等待接入 | 共享内存等待图像");

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(12, 12, 12, 10);
    rootLayout->setSpacing(10);
    setCentralWidget(root);

    setStyleSheet(QString(R"(
        QMainWindow {
            background: #0f141b;
        }
        QWidget {
            font-family: "Microsoft YaHei";
            font-size: 14px;
            color: #dce6f0;
        }
        QFrame#panel {
            background: #161d26;
            border: 1px solid #2b3645;
            border-radius: %1px;
        }
        QLabel#panelTitle {
            color: #f8fafc;
            font-size: 15px;
            font-weight: 600;
        }
        QLabel#systemTitle {
            color: #ffffff;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#sectionHint {
            color: #8fa4b8;
            font-size: 12px;
        }
        QLabel#largeValue {
            color: #ffffff;
            font-size: 22px;
            font-weight: 600;
        }
        QLabel#metricName {
            color: #9fb2c5;
        }
        QLabel#metricValue {
            color: #ffffff;
            font-weight: 600;
        }
        QLabel#imageMainText {
            color: #7f93a8;
            font-size: 21px;
            font-weight: 600;
        }
        QLabel#imageSubText {
            color: #5f7489;
            font-size: 13px;
        }
        ElaPlainTextEdit {
            background: #0c1117;
            color: #d9e2ec;
            border: 1px solid #303c4d;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #2563eb;
        }
        QStatusBar {
            background: #151b23;
            color: #aebdca;
            border-top: 1px solid #283446;
        }
    )").arg(kPanelRadius));

    auto *topBar = createPanel("系统总览");
    topBar->setFixedHeight(82);
    auto *topPanelLayout = qobject_cast<QVBoxLayout *>(topBar->layout());
    auto *topStatusLayout = new QHBoxLayout();
    topStatusLayout->setContentsMargins(0, 0, 0, 0);
    topStatusLayout->setSpacing(8);
    topStatusLayout->addWidget(makeLabel("铜粒子打磨系统", "systemTitle"));
    topStatusLayout->addSpacing(14);
    topStatusLayout->addWidget(createStatusPill("设备：待机", "ok"));
    topStatusLayout->addWidget(createStatusPill("后端：等待接入", "warn"));
    topStatusLayout->addWidget(createStatusPill("PLC：未连接", "warn"));
    topStatusLayout->addWidget(createStatusPill("共享内存：等待图像", "idle"));
    topStatusLayout->addStretch();
    topStatusLayout->addWidget(makeLabel(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), "largeValue"));

    auto *refreshButton = new ElaIconButton(ElaIconType::ArrowsRotate, 17, 34, 34, topBar);
    refreshButton->setToolTip("刷新显示状态");
    topStatusLayout->addWidget(refreshButton);

    topPanelLayout->addLayout(topStatusLayout);
    rootLayout->addWidget(topBar);

    auto *centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(10);
    rootLayout->addLayout(centerLayout, 1);

    auto *leftPanel = createPanel("工艺流程");
    leftPanel->setMinimumWidth(240);
    leftPanel->setMaximumWidth(300);
    auto *leftLayout = qobject_cast<QVBoxLayout *>(leftPanel->layout());
    leftLayout->addWidget(makeLabel("当前工序", "sectionHint"));
    leftLayout->addWidget(makeLabel("等待路径规划结果", "largeValue"));
    leftLayout->addWidget(makeLabel("任务进度", "sectionHint"));
    leftLayout->addWidget(makeProgressBar(0));
    leftLayout->addSpacing(10);
    leftLayout->addWidget(createStepRow("等待任务", "完成", "ok"));
    leftLayout->addWidget(createStepRow("扫描采集", "待开始", "idle"));
    leftLayout->addWidget(createStepRow("图像写入", "待开始", "idle"));
    leftLayout->addWidget(createStepRow("算法识别", "待开始", "idle"));
    leftLayout->addWidget(createStepRow("路径规划", "待开始", "idle"));
    leftLayout->addWidget(createStepRow("加工执行", "待开始", "idle"));
    leftLayout->addWidget(createStepRow("复检完成", "待开始", "idle"));
    leftLayout->addStretch();
    leftLayout->addWidget(makeLabel("关键设备状态", "sectionHint"));
    leftLayout->addWidget(createStatusPill("PLC 未连接", "warn"));
    leftLayout->addWidget(createStatusPill("相机 未接入", "warn"));
    leftLayout->addWidget(createStatusPill("算法服务 未接入", "warn"));
    centerLayout->addWidget(leftPanel);

    auto *imagePanel = createPanel("3D 图像与路径显示");
    auto *imageLayout = qobject_cast<QVBoxLayout *>(imagePanel->layout());
    auto *imageArea = new QFrame();
    imageArea->setObjectName("imageArea");
    imageArea->setMinimumSize(620, 360);
    imageArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageArea->setStyleSheet(R"(
        QFrame#imageArea {
            background: #090d13;
            border: 1px solid #334252;
            border-radius: 6px;
        }
    )");
    auto *imageAreaLayout = new QVBoxLayout(imageArea);
    imageAreaLayout->setContentsMargins(18, 18, 18, 18);
    imageAreaLayout->addStretch();
    auto *imageText = makeLabel("高度图 / 缺陷点 / 路径叠加显示区", "imageMainText");
    imageText->setAlignment(Qt::AlignCenter);
    imageAreaLayout->addWidget(imageText);
    auto *imageSubText = makeLabel("等待 ImageReady 消息后从共享内存读取图像", "imageSubText");
    imageSubText->setAlignment(Qt::AlignCenter);
    imageAreaLayout->addWidget(imageSubText);
    imageAreaLayout->addStretch();
    imageLayout->addWidget(imageArea, 1);

    auto *imageInfoLayout = new QHBoxLayout();
    imageInfoLayout->addWidget(createStatusPill("FrameId：-", "idle"));
    imageInfoLayout->addWidget(createStatusPill("缺陷：-", "idle"));
    imageInfoLayout->addWidget(createStatusPill("路径段：-", "idle"));
    imageInfoLayout->addStretch();
    imageLayout->addLayout(imageInfoLayout);
    centerLayout->addWidget(imagePanel, 1);

    auto *rightPanel = createPanel("运行数据");
    rightPanel->setMinimumWidth(270);
    rightPanel->setMaximumWidth(340);
    auto *rightLayout = qobject_cast<QVBoxLayout *>(rightPanel->layout());
    rightLayout->addWidget(makeLabel("运动轴", "sectionHint"));
    rightLayout->addWidget(createMetricRow("当前坐标 X", "0.000", "mm"));
    rightLayout->addWidget(createMetricRow("当前坐标 Y", "0.000", "mm"));
    rightLayout->addWidget(createMetricRow("当前坐标 Z", "0.000", "mm"));
    rightLayout->addSpacing(8);
    rightLayout->addWidget(makeLabel("加工状态", "sectionHint"));
    rightLayout->addWidget(createMetricRow("主轴转速", "0", "rpm"));
    rightLayout->addWidget(createMetricRow("进给速度", "0", "mm/min"));
    rightLayout->addWidget(createMetricRow("扭矩反馈", "0", "N.m"));
    rightLayout->addWidget(makeProgressBar(0));
    rightLayout->addSpacing(8);
    rightLayout->addWidget(makeLabel("算法结果", "sectionHint"));
    rightLayout->addWidget(createMetricRow("粒子数量", "-", "个"));
    rightLayout->addWidget(createMetricRow("最大高度", "-", "mm"));
    rightLayout->addWidget(createMetricRow("路径段数", "-", "段"));
    rightLayout->addStretch();
    centerLayout->addWidget(rightPanel);

    auto *bottomPanel = createPanel("报警与事件日志");
    bottomPanel->setFixedHeight(230);
    auto *bottomLayout = qobject_cast<QVBoxLayout *>(bottomPanel->layout());
    auto *logToolbar = new QHBoxLayout();
    logToolbar->setContentsMargins(0, 0, 0, 0);
    logToolbar->addWidget(createStatusPill("报警：0", "ok"));
    logToolbar->addWidget(createStatusPill("事件：3", "running"));
    logToolbar->addStretch();
    auto *clearLogButton = new ElaIconButton(ElaIconType::TrashCan, 16, 32, 32, bottomPanel);
    clearLogButton->setToolTip("清空日志显示");
    logToolbar->addWidget(clearLogButton);
    bottomLayout->addLayout(logToolbar);

    auto *eventLog = new ElaPlainTextEdit(bottomPanel);
    eventLog->setReadOnly(true);
    eventLog->setPlainText(
        "[09:00:00] 系统启动，等待后端连接\n"
        "[09:00:01] PLC 通讯状态：等待接入\n"
        "[09:00:02] 图像共享内存状态：等待 ImageReady\n");
    bottomLayout->addWidget(eventLog, 1);
    rootLayout->addWidget(bottomPanel);

    connect(clearLogButton, &QPushButton::clicked, eventLog, &QPlainTextEdit::clear);
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        ElaMessageBar::information(ElaMessageBarType::BottomRight, "状态刷新", "当前为静态界面预览，后端通讯尚未接入。", 2200, this);
    });

    QTimer::singleShot(350, this, [this]() {
        ElaMessageBar::success(ElaMessageBarType::BottomRight, "界面初始化", "ElaWidgetTools 组件已接入。", 1800, this);
    });
}

QFrame *MainWindow::createPanel(const QString &title)
{
    auto *panel = new QFrame();
    panel->setObjectName("panel");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    auto *titleRow = new QHBoxLayout();
    auto *titleLabel = makeLabel(title, "panelTitle");
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    layout->addLayout(titleRow);

    return panel;
}

QLabel *MainWindow::createStatusPill(const QString &text, const QString &state)
{
    auto *label = makeLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumHeight(28);
    label->setContentsMargins(10, 0, 10, 0);
    label->setStyleSheet(QString("border-radius:4px;padding:3px 10px;%1").arg(pillStyle(state)));
    return label;
}

QWidget *MainWindow::createStepRow(const QString &name, const QString &stateText, const QString &state)
{
    auto *row = new QWidget();
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *nameLabel = makeLabel(name);
    auto *stateLabel = createStatusPill(stateText, state);
    stateLabel->setMinimumWidth(72);

    layout->addWidget(nameLabel);
    layout->addStretch();
    layout->addWidget(stateLabel);
    return row;
}

QWidget *MainWindow::createMetricRow(const QString &name, const QString &value, const QString &unit)
{
    auto *row = new QWidget();
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *nameLabel = makeLabel(name, "metricName");
    auto *valueLabel = makeLabel(unit.isEmpty() ? value : QString("%1 %2").arg(value, unit), "metricValue");
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(nameLabel);
    layout->addStretch();
    layout->addWidget(valueLabel);
    return row;
}
