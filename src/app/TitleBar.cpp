#include "app/TitleBar.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QColor>
#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kBarHeight = 58;

} // namespace

TitleBar::TitleBar(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("titleBar");
    setFixedHeight(kBarHeight);
    installEventFilter(this);

    auto *titleLayout = new QHBoxLayout(this);
    titleLayout->setContentsMargins(12, 0, 10, 0);
    titleLayout->setSpacing(10);

    // ---- 左：Logo + 品牌 ----
    auto *logoLabel = new QLabel(this);
    logoLabel->setFixedSize(40, 40);
    logoLabel->setPixmap(QPixmap(":/img/logo.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setStyleSheet("background:transparent;border:none;");
    titleLayout->addWidget(logoLabel);

    auto *brandColumn = new QVBoxLayout();
    brandColumn->setContentsMargins(0, 0, 0, 0);
    brandColumn->setSpacing(0);
    m_brand = new QLabel(QStringLiteral("铭准"), this);
    m_brandSub = new QLabel(QStringLiteral("MZ-VISION"), this);
    brandColumn->addStretch(1);
    brandColumn->addWidget(m_brand);
    brandColumn->addWidget(m_brandSub);
    brandColumn->addStretch(1);
    titleLayout->addLayout(brandColumn);
    titleLayout->addStretch(1);

    // ---- 中：系统名（不进布局，resizeEvent 里居中）----
    m_systemTitle = makeLabel(QStringLiteral("阴极铜板粒子检测及修磨系统"), "systemTitle");
    m_systemTitle->setParent(this);
    m_systemTitle->setWordWrap(false);
    m_systemTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_systemTitle->adjustSize();

    // ---- 右：设备状态、时间、铃铛、用户 ----
    m_devicePill = new QLabel(this);
    m_devicePill->setFixedHeight(26);
    m_devicePill->setAlignment(Qt::AlignCenter);
    titleLayout->addWidget(m_devicePill);

    m_clock = new QLabel(this);
    titleLayout->addWidget(m_clock);
    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(1000);
    connect(m_clockTimer, &QTimer::timeout, this, &TitleBar::updateClock);
    updateClock();
    m_clockTimer->start();

    titleLayout->addSpacing(2);
    titleLayout->addWidget(createBellButton());

    m_userButton = createUserChip();
    titleLayout->addWidget(m_userButton);

    setUserIdentity(QString(), Role::None);
    updateDevicePill();
    setNotificationCount(0);
    applyTheme();
    Theme::watch(this, [this]() { applyTheme(); });

    auto *minimizeShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Escape), this);
    auto *maximizeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    auto *closeShortcut = new QShortcut(QKeySequence("Ctrl+Q"), this);
    minimizeShortcut->setContext(Qt::WindowShortcut);
    maximizeShortcut->setContext(Qt::WindowShortcut);
    closeShortcut->setContext(Qt::WindowShortcut);

    connect(m_userButton, &QPushButton::clicked, this, [this]() {
        emit userRequested(m_userButton->mapToGlobal(QPoint(0, m_userButton->height() + 4)));
    });
    connect(minimizeShortcut, &QShortcut::activated, this, &TitleBar::minimizeRequested);
    connect(maximizeShortcut, &QShortcut::activated, this, &TitleBar::maximizeRestoreRequested);
    connect(closeShortcut, &QShortcut::activated, this, &TitleBar::closeRequested);
}

void TitleBar::applyTheme()
{
    if (m_brand) {
        m_brand->setStyleSheet(QStringLiteral("color:%1;font-size:18px;font-weight:700;background:transparent;border:none;")
                                   .arg(Theme::kTitle));
    }
    if (m_brandSub) {
        m_brandSub->setStyleSheet(QStringLiteral("color:%1;font-size:10px;letter-spacing:1px;background:transparent;border:none;")
                                      .arg(Theme::kTextMuted));
    }
    if (m_clock) {
        m_clock->setStyleSheet(QStringLiteral("color:%1;font-size:14px;background:transparent;border:none;")
                                   .arg(Theme::kTextSecondary));
    }
    if (m_bellButton) {
        m_bellButton->setIcon(QIcon(tintedIcon(QStringLiteral(":/img/bell.svg"),
                                               QColor(Theme::kTextSecondary),
                                               22,
                                               devicePixelRatioF())));
        m_bellButton->setStyleSheet(QStringLiteral(
                                        "QPushButton#titleBell { background:transparent; border:1px solid transparent; border-radius:20px; }"
                                        "QPushButton#titleBell:hover { background:%1; border-color:%2; }"
                                        "QPushButton#titleBell:pressed { background:%3; }")
                                        .arg(Theme::kHoverBg, Theme::kBorder, Theme::kPressedBg));
    }
    if (m_bellBadge) {
        m_bellBadge->setStyleSheet(QStringLiteral("color:#FFFFFF;background:%1;border:none;border-radius:8px;"
                                                  "font-size:10px;font-weight:700;padding:0 3px;")
                                       .arg(Theme::kStateError));
    }
    if (m_userButton) {
        m_userButton->setStyleSheet(QStringLiteral(R"(
            QPushButton#titleUserChip {
                background: transparent;
                border: 1px solid transparent;
                border-radius: 21px;
            }
            QPushButton#titleUserChip:hover { background: %1; border-color: %2; }
            QPushButton#titleUserChip:pressed { background: %3; }
        )").arg(Theme::kHoverBg, Theme::kBorder, Theme::kPressedBg));
    }
    if (m_userName) {
        m_userName->setStyleSheet(QStringLiteral("color:%1;font-size:14px;font-weight:600;background:transparent;border:none;")
                                      .arg(Theme::kTextPrimary));
    }
    if (m_userArrow) {
        m_userArrow->setStyleSheet(QStringLiteral("color:%1;font-size:14px;background:transparent;border:none;")
                                      .arg(Theme::kTextMuted));
    }
    updateDevicePill();
    setUserIdentity(m_username, m_role);
}

QWidget *TitleBar::createBellButton()
{
    auto *host = new QWidget(this);
    host->setFixedSize(40, 40);
    host->setStyleSheet("background:transparent;border:none;");

    m_bellButton = new QPushButton(host);
    m_bellButton->setObjectName(QStringLiteral("titleBell"));
    m_bellButton->setGeometry(0, 0, 40, 40);
    m_bellButton->setCursor(Qt::PointingHandCursor);
    m_bellButton->setToolTip(QStringLiteral("报警与事件日志"));
    m_bellButton->setIcon(QIcon(tintedIcon(QStringLiteral(":/img/bell.svg"),
                                           QColor(Theme::kTextSecondary),
                                           22,
                                           devicePixelRatioF())));
    m_bellButton->setIconSize(QSize(22, 22));
    m_bellButton->setStyleSheet(QStringLiteral(
                                    "QPushButton#titleBell { background:transparent; border:1px solid transparent; border-radius:20px; }"
                                    "QPushButton#titleBell:hover { background:%1; border-color:%2; }"
                                    "QPushButton#titleBell:pressed { background:%3; }")
                                    .arg(Theme::kHoverBg, Theme::kBorder, Theme::kPressedBg));
    connect(m_bellButton, &QPushButton::clicked, this, &TitleBar::notificationsRequested);

    m_bellBadge = new QLabel(host);
    m_bellBadge->setAlignment(Qt::AlignCenter);
    m_bellBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_bellBadge->setStyleSheet(QStringLiteral("color:#FFFFFF;background:%1;border:none;border-radius:8px;"
                                              "font-size:10px;font-weight:700;padding:0 3px;")
                                   .arg(Theme::kStateError));
    m_bellBadge->hide();
    return host;
}

void TitleBar::setNotificationCount(int count)
{
    if (!m_bellBadge) {
        return;
    }
    if (count <= 0) {
        m_bellBadge->hide();
        return;
    }
    m_bellBadge->setText(count > 99 ? QStringLiteral("99+") : QString::number(count));
    m_bellBadge->adjustSize();
    const int w = qMax(16, m_bellBadge->width());
    m_bellBadge->setGeometry(40 - w - 2, 4, w, 16);
    m_bellBadge->show();
    m_bellBadge->raise();
}

void TitleBar::updateClock()
{
    if (m_clock) {
        m_clock->setText(QDateTime::currentDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")));
    }
}

void TitleBar::updateDevicePill()
{
    if (!m_devicePill) {
        return;
    }
    QString text;
    QString color;
    QString background;
    if (m_startNodesBusy) {
        text = QStringLiteral("● 节点切换中");
        color = Theme::kStateWarning;
        background = Theme::kWarningBg;
    } else if (m_nodesRunning) {
        text = QStringLiteral("● 设备在线");
        color = Theme::kStateSuccess;
        background = Theme::kSuccessBg;
    } else {
        text = QStringLiteral("● 设备离线");
        color = Theme::kStateUnknown;
        background = Theme::kControlBg;
    }
    m_devicePill->setText(text);
    m_devicePill->setStyleSheet(QStringLiteral("color:%1;background:%2;border:1px solid %1;border-radius:13px;"
                                               "padding:0 12px;font-size:13px;")
                                    .arg(color, background));
    m_devicePill->setToolTip(m_nodesRunning ? QStringLiteral("相机 / 算法 / 后端节点运行中")
                                            : QStringLiteral("节点未运行。到 系统设置 → 设备与通信 启动"));
}

void TitleBar::centerTitle()
{
    if (!m_systemTitle) {
        return;
    }
    m_systemTitle->adjustSize();
    const int x = (width() - m_systemTitle->width()) / 2;
    const int y = (height() - m_systemTitle->height()) / 2;
    m_systemTitle->move(x, y);
}

void TitleBar::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    centerTitle();
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
    updateDevicePill();
}

void TitleBar::setStartNodesBusy(bool busy)
{
    if (m_startNodesBusy == busy) {
        return;
    }
    m_startNodesBusy = busy;
    updateDevicePill();
}

QPushButton *TitleBar::createUserChip()
{
    auto *chip = new QPushButton(this);
    chip->setObjectName(QStringLiteral("titleUserChip"));
    chip->setFixedSize(176, 42);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setStyleSheet(QStringLiteral(R"(
        QPushButton#titleUserChip {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 21px;
        }
        QPushButton#titleUserChip:hover {
            background: %1;
            border-color: %2;
        }
        QPushButton#titleUserChip:pressed {
            background: %3;
        }
    )").arg(Theme::kHoverBg, Theme::kBorder, Theme::kPressedBg));

    auto *layout = new QHBoxLayout(chip);
    layout->setContentsMargins(5, 0, 10, 0);
    layout->setSpacing(8);

    m_userAvatar = new QLabel(chip);
    m_userAvatar->setFixedSize(32, 32);
    m_userAvatar->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_userAvatar);

    auto *texts = new QVBoxLayout();
    texts->setContentsMargins(0, 0, 0, 0);
    texts->setSpacing(0);
    m_userName = new QLabel(chip);
    m_userName->setStyleSheet(QStringLiteral("color:%1;font-size:14px;font-weight:600;"
                                             "background:transparent;border:none;")
                                  .arg(Theme::kTextPrimary));
    m_userRole = new QLabel(chip);
    texts->addStretch(1);
    texts->addWidget(m_userName);
    texts->addWidget(m_userRole);
    texts->addStretch(1);
    layout->addLayout(texts, 1);

    m_userArrow = new QLabel(QStringLiteral("▾"), chip);
    layout->addWidget(m_userArrow);

    for (QLabel *label : {m_userAvatar, m_userName, m_userRole, m_userArrow}) {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    return chip;
}

void TitleBar::setUserIdentity(const QString &username, Role role)
{
    m_username = username;
    m_role = role;
    if (!m_userButton) {
        return;
    }
    const bool loggedIn = role != Role::None && !username.isEmpty();
    QString color = QLatin1String(Theme::kStateUnknown);
    if (role == Role::Administrator) {
        color = QLatin1String(Theme::kRealtime);
    } else if (role == Role::Technician) {
        color = QLatin1String(Theme::kAccent);
    } else if (role == Role::Operator) {
        color = QLatin1String(Theme::kStateSuccess);
    }

    m_userAvatar->setText(loggedIn ? username.left(1).toUpper() : QStringLiteral("?"));
    m_userAvatar->setStyleSheet(QStringLiteral("color:%1;background:%2;border:1px solid %1;"
                                               "border-radius:16px;font-size:15px;font-weight:700;")
                                    .arg(color, QString::fromLatin1(loggedIn ? Theme::kControlBg
                                                                             : Theme::kPanelBg)));

    const QFontMetrics metrics(m_userName->font());
    m_userName->setText(loggedIn ? metrics.elidedText(username, Qt::ElideRight, 100)
                                 : QStringLiteral("未登录"));
    m_userRole->setText(loggedIn ? Auth::roleDisplayName(role) : QStringLiteral("点击登录"));
    m_userRole->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;border:none;")
                                  .arg(loggedIn ? color : QString::fromLatin1(Theme::kTextMuted)));
    m_userButton->setToolTip(loggedIn
                                 ? QStringLiteral("%1（%2）。点击修改密码或退出登录")
                                       .arg(username, Auth::roleDisplayName(role))
                                 : QStringLiteral("登录以修改受权限控制的设置"));
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
