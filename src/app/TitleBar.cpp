#include "app/TitleBar.h"

#include "ElaText.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QColor>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace {

QPushButton *createActionButton(const QString &text, int width, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setFixedSize(width, 34);
    button->setCursor(Qt::PointingHandCursor);
    button->setObjectName("titleActionButton");
    button->setStyleSheet(QStringLiteral(R"(
        QPushButton#titleActionButton {
            color: %1;
            background: %2;
            border: 1px solid %3;
            border-radius: 6px;
            font-size: 18px;
            padding: 0 10px;
        }
        QPushButton#titleActionButton:hover {
            background: %4;
        }
        QPushButton#titleActionButton:checked {
            background: %4;
            border-color: %4;
        }
        QPushButton#titleActionButton:pressed {
            background: %5;
        }
        QPushButton#titleActionButton:focus {
            border-color: %1;
        }
    )").arg(Theme::kTextPrimary,
             Theme::kRunningBg,
             Theme::kAccent,
             Theme::kAccent,
             Theme::kSelectionBg));
    return button;
}

} // namespace

TitleBar::TitleBar(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("titleBar");
    setFixedHeight(58);
    installEventFilter(this);

    auto *titleLayout = new QHBoxLayout(this);
    titleLayout->setContentsMargins(12, 0, 8, 0);
    titleLayout->setSpacing(10);

    auto *logoLabel = new QLabel(this);
    logoLabel->setFixedSize(46, 46);
    logoLabel->setPixmap(QPixmap(":/img/logo.png").scaled(46, 46, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setStyleSheet("background:transparent;border:none;");
    titleLayout->addWidget(logoLabel);

    auto *titleTextLayout = new QVBoxLayout();
    titleTextLayout->setContentsMargins(0, 0, 0, 0);
    titleTextLayout->setSpacing(0);
    auto *systemTitle = makeLabel("阴级铜表面粒子智能修整系统", "systemTitle");
    systemTitle->setWordWrap(false);
    systemTitle->setMinimumWidth(systemTitle->sizeHint().width());
    titleTextLayout->addWidget(systemTitle);
    titleLayout->addLayout(titleTextLayout);

    m_emergencyStopButton = new QPushButton("急停", this);
    m_emergencyStopButton->setObjectName("emergencyStopButton");
    m_emergencyStopButton->setFixedSize(144, 46);
    m_emergencyStopButton->setCursor(Qt::PointingHandCursor);
    m_emergencyStopButton->setToolTip("双击触发急停请求");
    m_emergencyStopButton->setAccessibleName("急停");
    m_emergencyStopButton->setAccessibleDescription("双击触发急停请求");
    const QColor emergencyColor(Theme::kStateError);
    m_emergencyStopButton->setStyleSheet(QStringLiteral(R"(
        QPushButton#emergencyStopButton {
            color: #FFFFFF;
            background: %1;
            border: 2px solid %1;
            border-radius: 6px;
            font-size: 18px;
            font-weight: 700;
        }
        QPushButton#emergencyStopButton:hover {
            background: %2;
            border-color: %2;
        }
        QPushButton#emergencyStopButton:pressed {
            background: %3;
            border-color: %3;
        }
        QPushButton#emergencyStopButton:focus {
            border-color: #FFFFFF;
        }
    )")
        .arg(emergencyColor.name(),
             emergencyColor.lighter(112).name(),
             emergencyColor.darker(118).name()));
    m_emergencyStopButton->installEventFilter(this);
    titleLayout->addStretch();

    m_settingsButton = createActionButton("设置", 108, this);
    m_startNodesButton = createActionButton("启动", 108, this);
    m_startNodesButton->setToolTip(QStringLiteral("启动算法、相机、后端节点"));
    m_homeButton = createActionButton("首页", 108, this);
    m_historyButton = createActionButton("历史记录", 108, this);
    m_homeButton->setCheckable(true);
    m_historyButton->setCheckable(true);
    m_settingsButton->setCheckable(true);
    m_homeButton->setAutoExclusive(true);
    m_historyButton->setAutoExclusive(true);
    m_settingsButton->setAutoExclusive(true);
    m_homeButton->setChecked(true);

    titleLayout->addWidget(m_homeButton);
    titleLayout->addWidget(m_historyButton);
    titleLayout->addWidget(m_settingsButton);
    titleLayout->addWidget(m_startNodesButton);
    titleLayout->addSpacing(24);
    titleLayout->addWidget(m_emergencyStopButton);

    auto *minimizeShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Escape), this);
    auto *maximizeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    auto *closeShortcut = new QShortcut(QKeySequence("Ctrl+Q"), this);
    minimizeShortcut->setContext(Qt::WindowShortcut);
    maximizeShortcut->setContext(Qt::WindowShortcut);
    closeShortcut->setContext(Qt::WindowShortcut);

    connect(m_settingsButton, &QPushButton::clicked, this, &TitleBar::settingsRequested);
    connect(m_startNodesButton, &QPushButton::clicked, this, [this]() {
        if (m_nodesRunning) {
            emit stopNodesRequested();
        } else {
            emit startNodesRequested();
        }
    });
    connect(m_homeButton, &QPushButton::clicked, this, &TitleBar::homeRequested);
    connect(m_historyButton, &QPushButton::clicked, this, &TitleBar::historyRequested);
    connect(minimizeShortcut, &QShortcut::activated, this, &TitleBar::minimizeRequested);
    connect(maximizeShortcut, &QShortcut::activated, this, &TitleBar::maximizeRestoreRequested);
    connect(closeShortcut, &QShortcut::activated, this, &TitleBar::closeRequested);
}

void TitleBar::setWorkspacePage(WorkspacePage page)
{
    QPushButton *target = nullptr;
    switch (page) {
    case WorkspacePage::Home:
        target = m_homeButton;
        break;
    case WorkspacePage::History:
        target = m_historyButton;
        break;
    case WorkspacePage::Settings:
        target = m_settingsButton;
        break;
    }
    if (target) {
        target->setChecked(true);
    }
}

void TitleBar::setMaximizedState(bool maximized)
{
    m_maximized = maximized;
}

void TitleBar::setNodesRunning(bool running)
{
    if (m_nodesRunning == running) {
        return;
    }
    m_nodesRunning = running;
    updateStartNodesButton();
}

void TitleBar::setStartNodesBusy(bool busy)
{
    if (m_startNodesBusy == busy) {
        return;
    }
    m_startNodesBusy = busy;
    updateStartNodesButton();
}

void TitleBar::updateStartNodesButton()
{
    if (!m_startNodesButton) {
        return;
    }

    if (m_nodesRunning) {
        m_startNodesButton->setText(QStringLiteral("停止"));
        m_startNodesButton->setToolTip(QStringLiteral("停止算法、相机、后端节点"));
    } else {
        m_startNodesButton->setText(QStringLiteral("启动"));
        m_startNodesButton->setToolTip(QStringLiteral("启动算法、相机、后端节点"));
    }
    m_startNodesButton->setEnabled(!m_startNodesBusy);
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_emergencyStopButton) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                emit emergencyStopRequested();
                return true;
            }
        }
        return QFrame::eventFilter(watched, event);
    }

    if (watched != this) {
        return QFrame::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit maximizeRestoreRequested();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && !m_maximized) {
            m_dragging = true;
            m_dragPosition = mouseEvent->pos();
            return true;
        }
    }

    if (event->type() == QEvent::MouseMove && m_dragging) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        emit dragRequested(mouseEvent->globalPos() - m_dragPosition);
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        m_dragging = false;
    }

    return QFrame::eventFilter(watched, event);
}
