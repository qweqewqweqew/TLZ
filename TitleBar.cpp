#include "TitleBar.h"

#include "ElaIcon.h"
#include "ElaIconButton.h"
#include "ElaMenu.h"
#include "ElaText.h"
#include "ElaToolButton.h"
#include "UiHelpers.h"

#include <QAction>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

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
    titleTextLayout->addWidget(makeLabel("铜粒子打磨系统", "systemTitle"));
    titleLayout->addLayout(titleTextLayout);
    titleLayout->addStretch();

    auto *settingsButton = new ElaToolButton(this);
    settingsButton->setText("设置");
    settingsButton->setElaIcon(ElaIconType::Gear);
    settingsButton->setPopupMode(QToolButton::InstantPopup);
    settingsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    settingsButton->setFixedSize(88, 34);
    settingsButton->setCursor(Qt::PointingHandCursor);

    auto *topMenu = new ElaMenu(settingsButton);
    auto *settingsAction = topMenu->addElaIconAction(ElaIconType::Gear, "系统设置");
    m_simulationAction = topMenu->addElaIconAction(ElaIconType::ChartLine, "开始模拟");
    settingsButton->setMenu(topMenu);
    titleLayout->addWidget(settingsButton);

    auto *minimizeButton = createTitleButton(ElaIconType::Dash, "最小化", this);
    m_maximizeButton = createTitleButton(ElaIconType::Square, "最大化", this);
    auto *closeButton = createTitleButton(ElaIconType::Xmark, "关闭", this);
    closeButton->setLightHoverColor(QColor("#8B2B35"));
    closeButton->setDarkHoverColor(QColor("#8B2B35"));

    titleLayout->addWidget(minimizeButton);
    titleLayout->addWidget(m_maximizeButton);
    titleLayout->addWidget(closeButton);

    connect(settingsAction, &QAction::triggered, this, &TitleBar::settingsRequested);
    connect(m_simulationAction, &QAction::triggered, this, [this]() {
        m_simulationRunning = !m_simulationRunning;
        updateSimulationActionText();
        emit simulationToggled(m_simulationRunning);
    });
    connect(minimizeButton, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maximizeButton, &QPushButton::clicked, this, &TitleBar::maximizeRestoreRequested);
    connect(closeButton, &QPushButton::clicked, this, &TitleBar::closeRequested);

    updateMaximizeButtonIcon();
}

void TitleBar::setMaximizedState(bool maximized)
{
    m_maximized = maximized;
    updateMaximizeButtonIcon();
}

void TitleBar::setSimulationRunning(bool running)
{
    if (m_simulationRunning == running) {
        return;
    }

    m_simulationRunning = running;
    updateSimulationActionText();
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
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

void TitleBar::updateMaximizeButtonIcon()
{
    if (!m_maximizeButton) {
        return;
    }

    m_maximizeButton->setAwesome(m_maximized ? ElaIconType::WindowRestore : ElaIconType::Square);
    m_maximizeButton->setToolTip(m_maximized ? "还原" : "最大化");
}

void TitleBar::updateSimulationActionText()
{
    if (!m_simulationAction) {
        return;
    }

    m_simulationAction->setText(m_simulationRunning ? "停止模拟" : "开始模拟");
}
