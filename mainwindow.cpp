#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ElaComboBox.h"
#include "ElaIcon.h"
#include "ElaIconButton.h"
#include "ElaMenu.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"
#include "ElaProgressBar.h"
#include "ElaPushButton.h"
#include "ElaStatusBar.h"
#include "ElaText.h"
#include "ElaToolButton.h"
#include "Logger.h"

#include <QAction>
#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QDateTime>
#include <QTextCursor>
#include <QVBoxLayout>

namespace {

constexpr int kPanelRadius = 8;
const QColor kButtonDefault("#2D6F9F");
const QColor kButtonHover("#377FAF");
const QColor kButtonPress("#245B83");
const QColor kButtonText("#F7FBFF");

QString pillStyle(const QString &state)
{
    if (state == "ok") {
        return "background:#142A24;border:1px solid #255A46;";
    }
    if (state == "running") {
        return "background:#122B3D;border:1px solid #275D81;";
    }
    if (state == "error") {
        return "background:#301D23;border:1px solid #70313B;";
    }
    return "background:#2C2919;border:1px solid #5E5024;";
}

QString statusDotColor(const QString &state)
{
    if (state == "ok") {
        return "#2ECC71";
    }
    if (state == "running") {
        return "#3498DB";
    }
    if (state == "error") {
        return "#E74C3C";
    }
    return "#F1C40F";
}

QLabel *makeLabel(const QString &text, const QString &objectName = QString())
{
    auto *label = new ElaText(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    if (objectName == "systemTitle") {
        label->setTextPixelSize(22);
    } else if (objectName == "topTime") {
        label->setTextPixelSize(16);
    } else if (objectName == "largeValue") {
        label->setTextPixelSize(22);
    } else if (objectName == "imageMainText") {
        label->setTextPixelSize(18);
    } else if (objectName == "panelTitle") {
        label->setTextPixelSize(16);
    } else if (objectName == "sectionHint") {
        label->setTextPixelSize(12);
    } else {
        label->setTextPixelSize(14);
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

class ImageViewFrame : public QFrame
{
public:
    explicit ImageViewFrame(QWidget *parent = nullptr)
        : QFrame(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QFrame::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QRect area = rect().adjusted(1, 1, -2, -2);
        painter.fillRect(area, QColor("#0B1117"));

        QPen gridPen(QColor(60, 80, 96, 70));
        gridPen.setWidth(1);
        painter.setPen(gridPen);

        constexpr int gridSize = 48;
        for (int x = area.left() + gridSize; x < area.right(); x += gridSize) {
            painter.drawLine(x, area.top(), x, area.bottom());
        }
        for (int y = area.top() + gridSize; y < area.bottom(); y += gridSize) {
            painter.drawLine(area.left(), y, area.right(), y);
        }

        QPen centerPen(QColor(0, 229, 255, 55));
        centerPen.setWidth(1);
        painter.setPen(centerPen);
        painter.drawLine(area.center().x(), area.top(), area.center().x(), area.bottom());
        painter.drawLine(area.left(), area.center().y(), area.right(), area.center().y());
    }
};

void applyPrimaryButtonStyle(ElaPushButton *button)
{
    button->setBorderRadius(6);
    button->setLightDefaultColor(kButtonDefault);
    button->setDarkDefaultColor(kButtonDefault);
    button->setLightHoverColor(kButtonHover);
    button->setDarkHoverColor(kButtonHover);
    button->setLightPressColor(kButtonPress);
    button->setDarkPressColor(kButtonPress);
    button->setLightTextColor(kButtonText);
    button->setDarkTextColor(kButtonText);
}

void applySecondaryButtonStyle(ElaPushButton *button)
{
    button->setBorderRadius(6);
    button->setLightDefaultColor(QColor("#15202B"));
    button->setDarkDefaultColor(QColor("#15202B"));
    button->setLightHoverColor(QColor("#203040"));
    button->setDarkHoverColor(QColor("#203040"));
    button->setLightPressColor(QColor("#253A4E"));
    button->setDarkPressColor(QColor("#253A4E"));
    button->setLightTextColor(QColor("#B8C8D8"));
    button->setDarkTextColor(QColor("#B8C8D8"));
}

void applyDangerButtonStyle(ElaPushButton *button)
{
    button->setBorderRadius(6);
    button->setLightDefaultColor(QColor("#8B2B35"));
    button->setDarkDefaultColor(QColor("#8B2B35"));
    button->setLightHoverColor(QColor("#A73642"));
    button->setDarkHoverColor(QColor("#A73642"));
    button->setLightPressColor(QColor("#74242D"));
    button->setDarkPressColor(QColor("#74242D"));
    button->setLightTextColor(QColor("#FFFFFF"));
    button->setDarkTextColor(QColor("#FFFFFF"));
}

void applyIconButtonStyle(ElaIconButton *button)
{
    button->setBorderRadius(6);
    button->setLightHoverColor(kButtonDefault);
    button->setDarkHoverColor(kButtonDefault);
    button->setLightIconColor(kButtonText);
    button->setDarkIconColor(kButtonText);
    button->setLightHoverIconColor(kButtonText);
    button->setDarkHoverIconColor(kButtonText);
}

ElaIconButton *createTitleButton(ElaIconType::IconName icon, const QString &tooltip, QWidget *parent)
{
    auto *button = new ElaIconButton(icon, 17, 40, 40, parent);
    button->setFixedSize(40, 40);
    button->setToolTip(tooltip);
    button->setCursor(Qt::ArrowCursor);
    button->setBorderRadius(4);
    button->setLightIconColor(QColor("#E6EEF5"));
    button->setDarkIconColor(QColor("#E6EEF5"));
    button->setLightHoverIconColor(QColor("#FFFFFF"));
    button->setDarkHoverIconColor(QColor("#FFFFFF"));
    button->setLightHoverColor(QColor("#22303D"));
    button->setDarkHoverColor(QColor("#22303D"));
    return button;
}

ElaComboBox *createComboBox(const QStringList &items, int width, QWidget *parent)
{
    auto *combo = new ElaComboBox(parent);
    combo->addItems(items);
    combo->setFixedSize(width, 30);
    combo->setBorderRadius(5);
    return combo;
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

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 鼠标离开波形图时隐藏竖线和时间标签（定格状态保留）
    if (event->type() == QEvent::Leave) {
        if (watched == m_speedPlot && m_speedCursorLine && !m_speedCursorFrozen) {
            m_speedCursorLine->setVisible(false);
            m_speedTimeLabel->setVisible(false);
            m_speedPlot->replot();
        } else if (watched == m_torquePlot && m_torqueCursorLine && !m_torqueCursorFrozen) {
            m_torqueCursorLine->setVisible(false);
            m_torqueTimeLabel->setVisible(false);
            m_torquePlot->replot();
        }
    }

    if (watched != m_titleDragArea && watched != m_titleDragArea->parent()) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            isMaximized() ? showNormal() : showMaximized();
            updateMaximizeButtonIcon();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && !isMaximized()) {
            m_dragging = true;
            m_dragPosition = mouseEvent->globalPos() - frameGeometry().topLeft();
            return true;
        }
    }

    if (event->type() == QEvent::MouseMove && m_dragging) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        move(mouseEvent->globalPos() - m_dragPosition);
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        m_dragging = false;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateMaximizeButtonIcon()
{
    if (!m_maximizeButton) {
        return;
    }

    m_maximizeButton->setAwesome(isMaximized() ? ElaIconType::WindowRestore : ElaIconType::Square);
    m_maximizeButton->setToolTip(isMaximized() ? "还原" : "最大化");
}

void MainWindow::appendEventLog(const QString &level, const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    const QString line = QString("[%1] [%2] %3").arg(timestamp, level, message);

    if (m_eventLogEdit) {
        m_eventLogEdit->appendPlainText(line);
        m_eventLogEdit->moveCursor(QTextCursor::End);
    }

    const QByteArray levelBytes = level.toUtf8();
    const QByteArray messageBytes = message.toUtf8();
    LOG("[%s] %s", levelBytes.constData(), messageBytes.constData());
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

    auto *root = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(10, 10, 10, 8);
    rootLayout->setSpacing(8);
    setCentralWidget(root);

    setStyleSheet(QString(R"(
        QMainWindow {
            background: #16202B;
        }
        QWidget {
            font-family: "Microsoft YaHei";
            font-size: 14px;
            color: #E4EAF0;
        }
        QFrame#titleBar {
            background: #0E151D;
            border: 1px solid #283746;
            border-radius: 8px;
        }
        QFrame#controlBar {
            background: #182431;
            border: 1px solid #2E3E4E;
            border-radius: 8px;
        }
        QFrame#panel {
            background: #202B37;
            border: 1px solid #334353;
            border-radius: %1px;
        }
        QLabel#panelTitle {
            color: #F1F5F9;
            font-size: 15px;
            font-weight: 600;
        }
        QLabel#systemTitle {
            color: #F1F5F9;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#topTime {
            color: #DDE7F0;
            font-size: 16px;
            font-weight: 600;
        }
        QLabel#sectionHint {
            color: #8A9AA8;
            font-size: 12px;
        }
        QLabel#largeValue {
            color: #E6EEF5;
            font-size: 22px;
            font-weight: 600;
        }
        QLabel#metricName {
            color: #8A9AA8;
        }
        QLabel#metricValue {
            color: #E6EEF5;
            font-weight: 600;
        }
        QLabel#imageMainText {
            color: #5A6A78;
            font-size: 18px;
            font-weight: 600;
        }
        ElaComboBox {
            background: #101923;
            color: #E6EEF5;
            border: 1px solid #334353;
            border-radius: 5px;
            padding-left: 8px;
        }
        ElaPlainTextEdit {
            background: #17212C;
            color: #AABBCC;
            border: 1px solid #303C49;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #2D6F9F;
        }
        QStatusBar {
            background: #111922;
            color: #8A9AA8;
            border-top: 1px solid #2E3E4E;
        }
    )").arg(kPanelRadius));

    auto *titleBar = new QFrame(root);
    titleBar->setObjectName("titleBar");
    titleBar->setFixedHeight(58);
    titleBar->installEventFilter(this);
    m_titleDragArea = titleBar;

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 8, 0);
    titleLayout->setSpacing(10);

    auto *logoLabel = new QLabel(titleBar);
    logoLabel->setFixedSize(46, 46);
    logoLabel->setPixmap(QPixmap(":/img/logo.png").scaled(46, 46, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setStyleSheet("background:transparent;border:none;");
    titleLayout->addWidget(logoLabel);

    auto *titleTextLayout = new QVBoxLayout();
    titleTextLayout->setContentsMargins(0, 0, 0, 0);
    titleTextLayout->setSpacing(0);
    titleTextLayout->addWidget(makeLabel("铜粒子打磨系统", "systemTitle"));
    titleLayout->addLayout(titleTextLayout);
    titleLayout->addStretch();

    auto *settingsButton = new ElaToolButton(titleBar);
    settingsButton->setText("设置");
    settingsButton->setElaIcon(ElaIconType::Gear);
    settingsButton->setPopupMode(QToolButton::InstantPopup);
    settingsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    settingsButton->setFixedSize(88, 34);
    settingsButton->setCursor(Qt::PointingHandCursor);
    auto *topMenu = new ElaMenu(settingsButton);
    auto *settingsAction = topMenu->addElaIconAction(ElaIconType::Gear, "系统设置");
    settingsButton->setMenu(topMenu);
    titleLayout->addWidget(settingsButton);

    auto *minimizeButton = createTitleButton(ElaIconType::Dash, "最小化", titleBar);
    m_maximizeButton = createTitleButton(ElaIconType::Square, "最大化", titleBar);
    auto *closeButton = createTitleButton(ElaIconType::Xmark, "关闭", titleBar);
    closeButton->setLightHoverColor(QColor("#8B2B35"));
    closeButton->setDarkHoverColor(QColor("#8B2B35"));
    titleLayout->addWidget(minimizeButton);
    titleLayout->addWidget(m_maximizeButton);
    titleLayout->addWidget(closeButton);

    connect(minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeButton, &QPushButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
        updateMaximizeButtonIcon();
    });
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    updateMaximizeButtonIcon();
    rootLayout->addWidget(titleBar);

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

    auto *leftPanel = createPanel("运行状态");
    leftPanel->setMinimumWidth(420);
    auto *leftLayout = qobject_cast<QVBoxLayout *>(leftPanel->layout());

    // 顶部状态指示
    leftLayout->addWidget(createStatusPill("系统运行中", "ok"));
    leftLayout->addSpacing(8);

    // ========================================
    // 转速 + 扭矩  平行放置
    // ========================================
    {
        auto *waveRow = new QHBoxLayout();
        waveRow->setSpacing(10);

        // --- 主轴转速 ---
        {
            auto *wrap = new QVBoxLayout();
            auto *label = makeLabel("主轴转速 (RPM)");
            label->setStyleSheet("color:#00E5FF;font-size:12px;font-weight:600;");
            wrap->addWidget(label);

            m_speedPlot = new QCustomPlot(leftPanel);
            m_speedPlot->setMinimumHeight(200);
            m_speedPlot->addGraph();
            m_speedPlot->graph(0)->setPen(QPen(QColor("#00E5FF"), 2));
            m_speedPlot->graph(0)->setLineStyle(QCPGraph::lsLine);
            m_speedPlot->yAxis->setRange(0, 3500);
            m_speedPlot->xAxis->setRange(0, 30);
            // 告警线 3000 RPM
            auto *limit = new QCPItemStraightLine(m_speedPlot);
            limit->point1->setCoords(0, 3000);
            limit->point2->setCoords(1, 3000);
            limit->setPen(QPen(QColor("#E74C3C"), 1, Qt::DashLine));
            // 暗色主题
            m_speedPlot->setBackground(QColor("#202B37"));
            m_speedPlot->axisRect()->setBackground(QColor("#202B37"));
            m_speedPlot->xAxis->setBasePen(QPen(QColor("#334353")));
            m_speedPlot->xAxis->setTickPen(Qt::NoPen);
            m_speedPlot->xAxis->setSubTickPen(Qt::NoPen);
            m_speedPlot->xAxis->setTickLabels(false);
            m_speedPlot->yAxis->setBasePen(QPen(QColor("#00E5FF")));
            m_speedPlot->yAxis->setTickPen(QPen(QColor("#00E5FF")));
            m_speedPlot->yAxis->setSubTickPen(QPen(QColor("#2A3745")));
            m_speedPlot->yAxis->setTickLabelColor(QColor("#00E5FF"));
            m_speedPlot->yAxis->setLabelColor(QColor("#00E5FF"));
            m_speedPlot->xAxis->grid()->setPen(QPen(QColor("#2A3745"), 1, Qt::DotLine));
            m_speedPlot->yAxis->grid()->setPen(QPen(QColor("#2A3745"), 1, Qt::DotLine));
            m_speedPlot->yAxis->setLabel("RPM");

            // 鼠标跟随：竖线 + 时间标签
            m_speedCursorLine = new QCPItemStraightLine(m_speedPlot);
            m_speedCursorLine->point1->setCoords(0, 0);
            m_speedCursorLine->point2->setCoords(0, 3500);
            m_speedCursorLine->setPen(QPen(QColor("#00E5FF"), 1, Qt::DotLine));
            m_speedCursorLine->setVisible(false);

            m_speedTimeLabel = new QCPItemText(m_speedPlot);
            m_speedTimeLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
            m_speedTimeLabel->position->setType(QCPItemPosition::ptPlotCoords);
            m_speedTimeLabel->position->setCoords(0, 3500);
            m_speedTimeLabel->setColor(QColor("#E4EAF0"));
            m_speedTimeLabel->setFont(QFont("Microsoft YaHei", 10));
            m_speedTimeLabel->setPadding(QMargins(6, 3, 6, 3));
            m_speedTimeLabel->setBrush(QColor(32, 43, 55, 220));
            m_speedTimeLabel->setPen(QPen(QColor("#334353")));
            m_speedTimeLabel->setVisible(false);

            m_speedPlot->setMouseTracking(true);
            m_speedPlot->installEventFilter(this);
            connect(m_speedPlot, &QCustomPlot::mouseMove, this, &MainWindow::onSpeedPlotMouseMove);
            connect(m_speedPlot, &QCustomPlot::mousePress, this, &MainWindow::onSpeedPlotMousePress);

            wrap->addWidget(m_speedPlot);
            waveRow->addLayout(wrap);
        }

        // --- 主轴扭矩 ---
        {
            auto *wrap = new QVBoxLayout();
            auto *label = makeLabel("主轴扭矩 (N·m)");
            label->setStyleSheet("color:#FF6B35;font-size:12px;font-weight:600;");
            wrap->addWidget(label);

            m_torquePlot = new QCustomPlot(leftPanel);
            m_torquePlot->setMinimumHeight(200);
            m_torquePlot->addGraph();
            m_torquePlot->graph(0)->setPen(QPen(QColor("#FF6B35"), 2));
            m_torquePlot->graph(0)->setLineStyle(QCPGraph::lsLine);
            m_torquePlot->yAxis->setRange(0, 30);
            m_torquePlot->xAxis->setRange(0, 30);
            // 告警线 25 N·m
            auto *limit = new QCPItemStraightLine(m_torquePlot);
            limit->point1->setCoords(0, 25);
            limit->point2->setCoords(1, 25);
            limit->setPen(QPen(QColor("#F1C40F"), 1, Qt::DashLine));
            // 暗色主题
            m_torquePlot->setBackground(QColor("#202B37"));
            m_torquePlot->axisRect()->setBackground(QColor("#202B37"));
            m_torquePlot->xAxis->setBasePen(QPen(QColor("#334353")));
            m_torquePlot->xAxis->setTickPen(Qt::NoPen);
            m_torquePlot->xAxis->setSubTickPen(Qt::NoPen);
            m_torquePlot->xAxis->setTickLabels(false);
            m_torquePlot->yAxis->setBasePen(QPen(QColor("#FF6B35")));
            m_torquePlot->yAxis->setTickPen(QPen(QColor("#FF6B35")));
            m_torquePlot->yAxis->setSubTickPen(QPen(QColor("#2A3745")));
            m_torquePlot->yAxis->setTickLabelColor(QColor("#FF6B35"));
            m_torquePlot->yAxis->setLabelColor(QColor("#FF6B35"));
            m_torquePlot->xAxis->grid()->setPen(QPen(QColor("#2A3745"), 1, Qt::DotLine));
            m_torquePlot->yAxis->grid()->setPen(QPen(QColor("#2A3745"), 1, Qt::DotLine));
            m_torquePlot->yAxis->setLabel("N·m");

            // 鼠标跟随：竖线 + 时间标签
            m_torqueCursorLine = new QCPItemStraightLine(m_torquePlot);
            m_torqueCursorLine->point1->setCoords(0, 0);
            m_torqueCursorLine->point2->setCoords(0, 30);
            m_torqueCursorLine->setPen(QPen(QColor("#FF6B35"), 1, Qt::DotLine));
            m_torqueCursorLine->setVisible(false);

            m_torqueTimeLabel = new QCPItemText(m_torquePlot);
            m_torqueTimeLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignHCenter);
            m_torqueTimeLabel->position->setType(QCPItemPosition::ptPlotCoords);
            m_torqueTimeLabel->position->setCoords(0, 30);
            m_torqueTimeLabel->setColor(QColor("#E4EAF0"));
            m_torqueTimeLabel->setFont(QFont("Microsoft YaHei", 10));
            m_torqueTimeLabel->setPadding(QMargins(6, 3, 6, 3));
            m_torqueTimeLabel->setBrush(QColor(32, 43, 55, 220));
            m_torqueTimeLabel->setPen(QPen(QColor("#334353")));
            m_torqueTimeLabel->setVisible(false);

            m_torquePlot->setMouseTracking(true);
            m_torquePlot->installEventFilter(this);
            connect(m_torquePlot, &QCustomPlot::mouseMove, this, &MainWindow::onTorquePlotMouseMove);
            connect(m_torquePlot, &QCustomPlot::mousePress, this, &MainWindow::onTorquePlotMousePress);

            wrap->addWidget(m_torquePlot);
            waveRow->addLayout(wrap);
        }

        leftLayout->addLayout(waveRow);
    }

    leftLayout->addStretch();
    topWorkspaceLayout->addWidget(leftPanel, 1);

    auto *viewToolbar = new QWidget();
    auto *viewToolbarLayout = new QHBoxLayout(viewToolbar);
    viewToolbarLayout->setContentsMargins(0, 0, 0, 0);
    viewToolbarLayout->setSpacing(6);
    auto *view2dButton = new ElaPushButton("2D", viewToolbar);
    view2dButton->setFixedSize(66, 28);
    applyPrimaryButtonStyle(view2dButton);
    auto *view3dButton = new ElaPushButton("3D", viewToolbar);
    view3dButton->setFixedSize(66, 28);
    applySecondaryButtonStyle(view3dButton);

    viewToolbarLayout->addWidget(view2dButton);
    viewToolbarLayout->addWidget(view3dButton);

    auto *imagePanel = createPanel("图像显示", viewToolbar);
    imagePanel->setFixedWidth(780);
    auto *imageLayout = qobject_cast<QVBoxLayout *>(imagePanel->layout());

    auto *imageArea = new ImageViewFrame(imagePanel);
    imageArea->setObjectName("imageArea");
    imageArea->setFixedSize(720, 600);
    imageArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    imageArea->setStyleSheet(R"(
        QFrame#imageArea {
            background: #0B1117;
            border: 1px solid #3A4755;
            border-radius: 6px;
        }
    )");
    auto *imageAreaLayout = new QVBoxLayout(imageArea);
    imageAreaLayout->setContentsMargins(18, 18, 18, 18);
    imageAreaLayout->addStretch();
    auto *imageText = makeLabel("等待图像", "imageMainText");
    imageText->setAlignment(Qt::AlignCenter);
    imageAreaLayout->addWidget(imageText);
    imageAreaLayout->addStretch();
    imageLayout->addWidget(imageArea, 0, Qt::AlignHCenter);
    topWorkspaceLayout->addWidget(imagePanel);

    auto *bottomPanel = createPanel("报警与事件日志");
    bottomPanel->setMinimumHeight(270);
    bottomPanel->setMaximumHeight(340);
    auto *bottomLayout = qobject_cast<QVBoxLayout *>(bottomPanel->layout());
    m_eventLogEdit = new ElaPlainTextEdit(bottomPanel);
    m_eventLogEdit->setReadOnly(true);
    m_eventLogEdit->setPlaceholderText("这里显示报警和事件日志...");
    m_eventLogEdit->setMaximumBlockCount(500);
    m_eventLogEdit->setStyleSheet(R"(
        ElaPlainTextEdit {
            background: #101923;
            color: #B9C7D4;
            border: 1px solid #2F3D4A;
            border-radius: 6px;
            padding: 8px;
            font-size: 16px;
        }
    )");
    bottomLayout->addWidget(m_eventLogEdit, 1);
    leftWorkspaceLayout->addWidget(bottomPanel, 0);

    mainLayout->addWidget(leftWorkspace, 3);

    auto *rightColumn = new QWidget();
    rightColumn->setMinimumWidth(420);
    auto *rightColumnLayout = new QVBoxLayout(rightColumn);
    rightColumnLayout->setContentsMargins(0, 0, 0, 0);
    rightColumnLayout->setSpacing(10);

    auto *rightPanel = createPanel("运行数据");
    auto *rightLayout = qobject_cast<QVBoxLayout *>(rightPanel->layout());
    rightLayout->addStretch();
    rightColumnLayout->addWidget(rightPanel, 1);

    auto *taskFlowPanel = createPanel("任务流程");
    auto *taskFlowLayout = qobject_cast<QVBoxLayout *>(taskFlowPanel->layout());
    taskFlowLayout->addStretch();
    rightColumnLayout->addWidget(taskFlowPanel, 2);

    mainLayout->addWidget(rightColumn, 1);

    connect(settingsAction, &QAction::triggered, this, [this]() {
        ElaMessageBar::information(ElaMessageBarType::BottomRight, "系统设置", "设置功能后续接入。", 2000, this);
        appendEventLog("INFO", "用户打开了系统设置");
    });
    QTimer::singleShot(350, this, [this]() {
        ElaMessageBar::success(ElaMessageBarType::BottomRight, "界面初始化", "ElaWidgetTools 组件已接入。", 1800, this);
        appendEventLog("INFO", "界面初始化完成");
    });

    // 启动波形刷新定时器
    m_waveformTimer = new QTimer(this);
    connect(m_waveformTimer, &QTimer::timeout, this, &MainWindow::onWaveformUpdate);
    m_waveformTimer->start(100);  // 10Hz
}

QFrame *MainWindow::createPanel(const QString &title, QWidget *headerWidget)
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
    if (headerWidget) {
        headerWidget->setParent(panel);
        titleRow->addWidget(headerWidget);
    }
    layout->addLayout(titleRow);

    auto *divider = new QFrame(panel);
    divider->setFixedHeight(1);
    divider->setStyleSheet("background:#303C49;border:none;");
    layout->addWidget(divider);

    return panel;
}

QWidget *MainWindow::createStatusPill(const QString &text, const QString &state)
{
    auto *status = new QWidget();
    status->setMinimumHeight(28);
    status->setStyleSheet(QString("border-radius:4px;%1").arg(pillStyle(state)));

    auto *layout = new QHBoxLayout(status);
    layout->setContentsMargins(8, 3, 10, 3);
    layout->setSpacing(6);

    auto *dot = makeLabel(QString(QChar(0x25CF)));
    dot->setFixedWidth(10);
    dot->setAlignment(Qt::AlignCenter);
    dot->setStyleSheet(QString("color:%1;font-size:12px;").arg(statusDotColor(state)));

    auto *label = makeLabel(text);
    label->setStyleSheet("color:#AABBCC;");
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(dot);
    layout->addWidget(label);
    return status;
}

QWidget *MainWindow::createStepRow(const QString &name, const QString &stateText, const QString &state)
{
    auto *row = new QWidget();
    row->setMinimumHeight(28);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *nameLabel = makeLabel(name);
    nameLabel->setStyleSheet("color:#E4EAF0;");

    auto *stateWidget = new QWidget(row);
    auto *stateLayout = new QHBoxLayout(stateWidget);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    stateLayout->setSpacing(5);

    auto *dot = makeLabel(QString(QChar(0x25CF)));
    dot->setFixedWidth(10);
    dot->setAlignment(Qt::AlignCenter);
    dot->setStyleSheet(QString("color:%1;font-size:12px;").arg(statusDotColor(state)));

    auto *stateLabel = makeLabel(stateText);
    stateLabel->setMinimumWidth(52);
    stateLabel->setStyleSheet("color:#8A9AA8;");
    stateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    stateLayout->addWidget(dot);
    stateLayout->addWidget(stateLabel);

    layout->addWidget(nameLabel);
    layout->addStretch();
    layout->addWidget(stateWidget);
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

void MainWindow::onWaveformUpdate()
{
    // 转速数据
    double speed = 2000 + 800 * qSin(m_currentT * 0.5) + (qrand() % 200);
    m_speedPlot->graph(0)->addData(m_currentT, speed);
    m_speedPlot->graph(0)->data()->removeBefore(m_currentT - 30);
    m_speedPlot->xAxis->setRange(m_currentT, 30, Qt::AlignRight);

    // 扭矩数据
    double torque = 15 + 5 * qCos(m_currentT * 0.7) + (qrand() % 100) * 0.05;
    m_torquePlot->graph(0)->addData(m_currentT, torque);
    m_torquePlot->graph(0)->data()->removeBefore(m_currentT - 30);
    m_torquePlot->xAxis->setRange(m_currentT, 30, Qt::AlignRight);

    m_speedPlot->replot();
    m_torquePlot->replot();

    m_currentT += 0.1;
}

void MainWindow::onSpeedPlotMouseMove(QMouseEvent *event)
{
    if (m_speedCursorFrozen) {
        return;  // 定格状态忽略鼠标移动
    }
    const double x = m_speedPlot->xAxis->pixelToCoord(event->pos().x());
    if (x < m_currentT - 30 || x > m_currentT) {
        m_speedCursorLine->setVisible(false);
        m_speedTimeLabel->setVisible(false);
        m_speedPlot->replot();
        return;
    }
    const QDateTime t = QDateTime::currentDateTime().addSecs(static_cast<qint64>(x - m_currentT));
    m_speedCursorLine->point1->setCoords(x, 0);
    m_speedCursorLine->point2->setCoords(x, 3500);
    m_speedCursorLine->setPen(QPen(QColor("#00E5FF"), 1, Qt::DotLine));
    m_speedCursorLine->setVisible(true);
    m_speedTimeLabel->position->setCoords(x, 3500);
    m_speedTimeLabel->setText(t.toString("HH:mm:ss"));
    m_speedTimeLabel->setVisible(true);
    m_speedPlot->replot();
}

void MainWindow::onTorquePlotMouseMove(QMouseEvent *event)
{
    if (m_torqueCursorFrozen) {
        return;  // 定格状态忽略鼠标移动
    }
    const double x = m_torquePlot->xAxis->pixelToCoord(event->pos().x());
    if (x < m_currentT - 30 || x > m_currentT) {
        m_torqueCursorLine->setVisible(false);
        m_torqueTimeLabel->setVisible(false);
        m_torquePlot->replot();
        return;
    }
    const QDateTime t = QDateTime::currentDateTime().addSecs(static_cast<qint64>(x - m_currentT));
    m_torqueCursorLine->point1->setCoords(x, 0);
    m_torqueCursorLine->point2->setCoords(x, 30);
    m_torqueCursorLine->setPen(QPen(QColor("#FF6B35"), 1, Qt::DotLine));
    m_torqueCursorLine->setVisible(true);
    m_torqueTimeLabel->position->setCoords(x, 30);
    m_torqueTimeLabel->setText(t.toString("HH:mm:ss"));
    m_torqueTimeLabel->setVisible(true);
    m_torquePlot->replot();
}

void MainWindow::onSpeedPlotMousePress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const double x = m_speedPlot->xAxis->pixelToCoord(event->pos().x());
        if (x < m_currentT - 30 || x > m_currentT) {
            return;
        }
        m_speedCursorFrozen = true;
        m_speedFrozenX = x;
        m_speedFrozenTime = QDateTime::currentDateTime().addSecs(static_cast<qint64>(x - m_currentT));
        m_speedCursorLine->point1->setCoords(x, 0);
        m_speedCursorLine->point2->setCoords(x, 3500);
        m_speedCursorLine->setPen(QPen(QColor("#00E5FF"), 2, Qt::SolidLine));  // 实线加粗
        m_speedCursorLine->setVisible(true);
        m_speedTimeLabel->position->setCoords(x, 3500);
        m_speedTimeLabel->setText(m_speedFrozenTime.toString("HH:mm:ss"));
        m_speedTimeLabel->setVisible(true);
        m_speedPlot->replot();
    } else if (event->button() == Qt::RightButton) {
        m_speedCursorFrozen = false;
        // 取消定格后，让线回到鼠标当前位置
        const double x = m_speedPlot->xAxis->pixelToCoord(event->pos().x());
        if (x < m_currentT - 30 || x > m_currentT) {
            m_speedCursorLine->setVisible(false);
            m_speedTimeLabel->setVisible(false);
        } else {
            const QDateTime t = QDateTime::currentDateTime().addSecs(static_cast<qint64>(x - m_currentT));
            m_speedCursorLine->point1->setCoords(x, 0);
            m_speedCursorLine->point2->setCoords(x, 3500);
            m_speedCursorLine->setPen(QPen(QColor("#00E5FF"), 1, Qt::DotLine));
            m_speedCursorLine->setVisible(true);
            m_speedTimeLabel->position->setCoords(x, 3500);
            m_speedTimeLabel->setText(t.toString("HH:mm:ss"));
            m_speedTimeLabel->setVisible(true);
        }
        m_speedPlot->replot();
    }
}

void MainWindow::onTorquePlotMousePress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const double x = m_torquePlot->xAxis->pixelToCoord(event->pos().x());
        if (x < m_currentT - 30 || x > m_currentT) {
            return;
        }
        m_torqueCursorFrozen = true;
        m_torqueFrozenX = x;
        m_torqueFrozenTime = QDateTime::currentDateTime().addSecs(static_cast<qint64>(x - m_currentT));
        m_torqueCursorLine->point1->setCoords(x, 0);
        m_torqueCursorLine->point2->setCoords(x, 30);
        m_torqueCursorLine->setPen(QPen(QColor("#FF6B35"), 2, Qt::SolidLine));  // 实线加粗
        m_torqueCursorLine->setVisible(true);
        m_torqueTimeLabel->position->setCoords(x, 30);
        m_torqueTimeLabel->setText(m_torqueFrozenTime.toString("HH:mm:ss"));
        m_torqueTimeLabel->setVisible(true);
        m_torquePlot->replot();
    } else if (event->button() == Qt::RightButton) {
        m_torqueCursorFrozen = false;
        // 取消定格后，让线回到鼠标当前位置
        const double x = m_torquePlot->xAxis->pixelToCoord(event->pos().x());
        if (x < m_currentT - 30 || x > m_currentT) {
            m_torqueCursorLine->setVisible(false);
            m_torqueTimeLabel->setVisible(false);
        } else {
            const QDateTime t = QDateTime::currentDateTime().addSecs(static_cast<qint64>(x - m_currentT));
            m_torqueCursorLine->point1->setCoords(x, 0);
            m_torqueCursorLine->point2->setCoords(x, 30);
            m_torqueCursorLine->setPen(QPen(QColor("#FF6B35"), 1, Qt::DotLine));
            m_torqueCursorLine->setVisible(true);
            m_torqueTimeLabel->position->setCoords(x, 30);
            m_torqueTimeLabel->setText(t.toString("HH:mm:ss"));
            m_torqueTimeLabel->setVisible(true);
        }
        m_torquePlot->replot();
    }
}
