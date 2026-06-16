#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace {

constexpr int kPanelRadius = 6;

QString pillStyle(const QString &state)
{
    if (state == "ok") {
        return "background:#1f7a4d;color:#ffffff;border:1px solid #2aa365;";
    }
    if (state == "running") {
        return "background:#1d5f96;color:#ffffff;border:1px solid #2a7fc4;";
    }
    if (state == "warn") {
        return "background:#8a6418;color:#ffffff;border:1px solid #c08a21;";
    }
    if (state == "error") {
        return "background:#8a2d2d;color:#ffffff;border:1px solid #c94747;";
    }
    return "background:#293342;color:#d9e2ec;border:1px solid #465569;";
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
    statusBar()->hide();

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);
    setCentralWidget(root);

    setStyleSheet(QString(R"(
        QMainWindow {
            background: #101820;
        }
        QWidget {
            font-family: "Microsoft YaHei";
            font-size: 14px;
            color: #dce6f0;
        }
        QFrame#panel {
            background: #17212b;
            border: 1px solid #2d3a47;
            border-radius: %1px;
        }
        QLabel#panelTitle {
            color: #ffffff;
            font-size: 15px;
            font-weight: 600;
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
        QListWidget {
            background: transparent;
            border: none;
            outline: none;
        }
        QListWidget::item {
            padding: 4px 2px;
            color: #dce6f0;
        }
    )").arg(kPanelRadius));

    auto *topBar = createPanel("顶部状态栏");
    topBar->setFixedHeight(74);
    auto *topPanelLayout = qobject_cast<QVBoxLayout *>(topBar->layout());
    auto *topStatusLayout = new QHBoxLayout();
    topStatusLayout->setContentsMargins(0, 0, 0, 0);
    topStatusLayout->setSpacing(8);
    topStatusLayout->addWidget(createStatusPill("设备：待机", "ok"));
    topStatusLayout->addWidget(createStatusPill("后端：已连接", "ok"));
    topStatusLayout->addWidget(createStatusPill("共享内存：等待图像", "warn"));
    topStatusLayout->addWidget(createStatusPill("当前任务：TLZ-0001", "running"));
    topStatusLayout->addStretch();
    topStatusLayout->addWidget(makeLabel(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), "largeValue"));
    topPanelLayout->addLayout(topStatusLayout);
    rootLayout->addWidget(topBar);

    auto *centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(10);
    rootLayout->addLayout(centerLayout, 1);

    auto *leftPanel = createPanel("流程状态");
    leftPanel->setMinimumWidth(230);
    leftPanel->setMaximumWidth(290);
    auto *leftLayout = qobject_cast<QVBoxLayout *>(leftPanel->layout());
    leftLayout->addWidget(makeLabel("当前工序", "sectionHint"));
    leftLayout->addWidget(makeLabel("路径规划结果等待中", "largeValue"));
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
    leftLayout->addWidget(createStatusPill("PLC 在线", "ok"));
    leftLayout->addWidget(createStatusPill("相机 未接入", "warn"));
    leftLayout->addWidget(createStatusPill("算法服务 未接入", "warn"));
    centerLayout->addWidget(leftPanel);

    auto *imagePanel = createPanel("图像显示");
    auto *imageLayout = qobject_cast<QVBoxLayout *>(imagePanel->layout());
    auto *imageArea = new QFrame();
    imageArea->setObjectName("imageArea");
    imageArea->setMinimumSize(620, 430);
    imageArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageArea->setStyleSheet(R"(
        QFrame#imageArea {
            background: #0b1118;
            border: 1px solid #334252;
            border-radius: 4px;
        }
    )");
    auto *imageAreaLayout = new QVBoxLayout(imageArea);
    imageAreaLayout->setContentsMargins(18, 18, 18, 18);
    imageAreaLayout->addStretch();
    auto *imageText = makeLabel("高度图 / 缺陷 / 路径叠加显示区");
    imageText->setAlignment(Qt::AlignCenter);
    imageText->setStyleSheet("color:#6f8499;font-size:20px;font-weight:600;");
    imageAreaLayout->addWidget(imageText);
    auto *imageSubText = makeLabel("等待 ImageReady 消息后从共享内存读取图像");
    imageSubText->setAlignment(Qt::AlignCenter);
    imageSubText->setStyleSheet("color:#52677c;font-size:13px;");
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
    rightPanel->setMinimumWidth(260);
    rightPanel->setMaximumWidth(330);
    auto *rightLayout = qobject_cast<QVBoxLayout *>(rightPanel->layout());
    rightLayout->addWidget(createMetricRow("当前坐标 X", "0.000", "mm"));
    rightLayout->addWidget(createMetricRow("当前坐标 Y", "0.000", "mm"));
    rightLayout->addWidget(createMetricRow("当前坐标 Z", "0.000", "mm"));
    rightLayout->addSpacing(8);
    rightLayout->addWidget(createMetricRow("主轴转速", "0", "rpm"));
    rightLayout->addWidget(createMetricRow("进给速度", "0", "mm/min"));
    rightLayout->addWidget(createMetricRow("扭矩反馈", "0", "N.m"));
    rightLayout->addSpacing(8);
    rightLayout->addWidget(createMetricRow("粒子数量", "-", "个"));
    rightLayout->addWidget(createMetricRow("最大高度", "-", "mm"));
    rightLayout->addWidget(createMetricRow("路径段数", "-", "段"));
    rightLayout->addStretch();
    centerLayout->addWidget(rightPanel);

    auto *bottomPanel = createPanel("报警与事件");
    bottomPanel->setFixedHeight(150);
    auto *bottomLayout = qobject_cast<QVBoxLayout *>(bottomPanel->layout());
    auto *eventList = new QListWidget();
    eventList->addItem("[09:00:00] 系统启动，等待后端连接");
    eventList->addItem("[09:00:01] 后端连接状态：占位");
    eventList->addItem("[09:00:02] 图像共享内存状态：等待");
    bottomLayout->addWidget(eventList);
    rootLayout->addWidget(bottomPanel);
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
    label->setMinimumHeight(30);
    label->setContentsMargins(10, 0, 10, 0);
    label->setStyleSheet(QString("border-radius:4px;padding:4px 10px;%1").arg(pillStyle(state)));
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
