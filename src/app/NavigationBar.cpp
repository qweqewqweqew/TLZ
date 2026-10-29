#include "app/NavigationBar.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int kNavWidth = 168;
constexpr int kItemHeight = 44;
constexpr int kIconSize = 20;

} // namespace

NavItemButton::NavItemButton(const QString &iconResource, const QString &text, QWidget *parent)
    : QAbstractButton(parent)
    , m_iconResource(iconResource)
{
    setText(text);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedHeight(kItemHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize NavItemButton::sizeHint() const
{
    return QSize(kNavWidth - 16, kItemHeight);
}

void NavItemButton::enterEvent(QEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void NavItemButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void NavItemButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF box = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QColor background = Qt::transparent;
    QColor foreground(Theme::kTextSecondary);
    if (isChecked()) {
        background = isDown() ? QColor(Theme::kSelectedPressedBg)
                              : m_hovered ? QColor(Theme::kSelectedHoverBg)
                                          : QColor(Theme::kSelectionBg);
        foreground = QColor(Theme::kSelectionText);
    } else if (m_hovered) {
        background = QColor(Theme::kHoverBg);
        foreground = QColor(Theme::kTextPrimary);
    } else if (isDown()) {
        background = QColor(Theme::kPressedBg);
        foreground = QColor(Theme::kTextPrimary);
    }
    if (background.alpha() > 0) {
        QPainterPath path;
        path.addRoundedRect(box, 8, 8);
        painter.fillPath(path, background);
    }

    const QPixmap icon = tintedIcon(m_iconResource, foreground, kIconSize, devicePixelRatioF());
    const int iconLeft = 14;
    painter.drawPixmap(QPointF(iconLeft, (height() - kIconSize) / 2.0), icon);

    QFont font = painter.font();
    font.setPixelSize(14);
    font.setBold(isChecked());
    painter.setFont(font);
    painter.setPen(foreground);
    const int textLeft = iconLeft + kIconSize + 10;
    painter.drawText(QRect(textLeft, 0, width() - textLeft - 8, height()),
                     Qt::AlignLeft | Qt::AlignVCenter, text());
}

NavigationBar::NavigationBar(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("navBar"));
    setFixedWidth(kNavWidth);
    setStyleSheet(QStringLiteral(
                      "QFrame#navBar { background:%1; border:1px solid %2; border-radius:8px; }"
                      "QFrame#navDivider { background:%3; border:none; }")
                      .arg(Theme::kWindowBg, Theme::kBorder, Theme::kDivider));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 12, 8, 10);
    layout->setSpacing(4);

    struct Entry {
        const char *icon;
        const char *text;
    };
    const Entry entries[PageCount] = {
        {":/img/nav_detect.svg", "检测与修磨"},
        {":/img/nav_history.svg", "历史查询"},
        {":/img/nav_report.svg", "报表统计"},
        {":/img/nav_settings.svg", "系统设置"},
    };
    for (int i = 0; i < PageCount; ++i) {
        auto *item = new NavItemButton(QString::fromLatin1(entries[i].icon),
                                       QString::fromUtf8(entries[i].text),
                                       this);
        item->setAutoExclusive(true);
        connect(item, &QAbstractButton::clicked, this, [this, i]() {
            // 点击后先恢复到当前页的选中态，真正切换由 setCurrentPage 完成。
            setCurrentPage(m_current);
            emit pageRequested(i);
        });
        layout->addWidget(item);
        m_items.append(item);
    }
    layout->addStretch(1);

    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("navDivider"));
    divider->setFixedHeight(1);
    layout->addWidget(divider);
    layout->addSpacing(4);

    auto *millingRow = new QWidget(this);
    auto *millingLayout = new QVBoxLayout(millingRow);
    millingLayout->setContentsMargins(0, 0, 0, 0);
    millingLayout->setSpacing(6);
    m_startButton = new QPushButton(QStringLiteral("开始"), millingRow);
    m_stopButton = new QPushButton(QStringLiteral("停止"), millingRow);
    for (QPushButton *button : {m_startButton, m_stopButton}) {
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setFixedHeight(36);
        millingLayout->addWidget(button, 1);
    }
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (m_millingRunning) {
            emit millingPauseRequested();
        } else {
            emit millingStartRequested();
        }
    });
    connect(m_stopButton, &QPushButton::clicked, this, &NavigationBar::millingStopRequested);
    layout->addWidget(millingRow);
    layout->addSpacing(4);
    layout->addWidget(createThemeButton());

    setUserIdentity(QString(), Role::None);
    setCurrentPage(Detection);
    applyTheme();
    Theme::watch(this, [this]() { applyTheme(); });
}

QPushButton *NavigationBar::createThemeButton()
{
    m_themeButton = new QPushButton(this);
    m_themeButton->setObjectName(QStringLiteral("navTheme"));
    m_themeButton->setCursor(Qt::PointingHandCursor);
    m_themeButton->setFocusPolicy(Qt::NoFocus);
    m_themeButton->setFixedHeight(36);
    connect(m_themeButton, &QPushButton::clicked, this, []() {
        Theme::setCurrent(Theme::current() == Theme::Id::Dark ? Theme::Id::Light : Theme::Id::Dark);
    });
    return m_themeButton;
}

void NavigationBar::setMillingRunning(bool running)
{
    m_millingRunning = running;
    applyTheme();
}

void NavigationBar::applyTheme()
{
    const auto paintAction = [](QPushButton *button, const char *fill, const QString &text,
                                const QString &iconPath) {
        if (!button) {
            return;
        }
        const QColor base(fill);
        const int hue = base.hue();
        const QColor textColor = (hue >= 25 && hue <= 70) ? QColor(0x1A, 0x1A, 0x1A) : QColor(Qt::white);
        button->setText(text);
        button->setIcon(QIcon(tintedIcon(iconPath, textColor, 16, button->devicePixelRatioF())));
        button->setIconSize(QSize(16, 16));
        button->setStyleSheet(QStringLiteral(
                                   "QPushButton { background:%1; color:%2; border:none; border-radius:6px;"
                                   " font-size:13px; font-weight:600; }"
                                   "QPushButton:hover { background:%3; }"
                                   "QPushButton:pressed { background:%4; }")
                                   .arg(base.name(),
                                        textColor.name(),
                                        base.darker(110).name(),
                                        base.darker(128).name()));
    };
    paintAction(m_startButton,
                m_millingRunning ? Theme::kStateWarning : Theme::kStateSuccess,
                m_millingRunning ? QStringLiteral("暂停") : QStringLiteral("开始"),
                m_millingRunning ? QStringLiteral(":/img/pause.svg") : QStringLiteral(":/img/play.svg"));
    paintAction(m_stopButton, Theme::kStateError, QStringLiteral("停止"), QStringLiteral(":/img/stop.svg"));
    if (m_themeButton) {
        const Theme::Id next = Theme::current() == Theme::Id::Dark ? Theme::Id::Light : Theme::Id::Dark;
        m_themeButton->setText(Theme::displayName(next));
        m_themeButton->setToolTip(QStringLiteral("切换到%1").arg(Theme::displayName(next)));
        m_themeButton->setStyleSheet(QStringLiteral(
                                         "QPushButton#navTheme { color:%1; background:%2; border:1px solid %3;"
                                         " border-radius:6px; padding:0 8px; font-size:13px; }"
                                         "QPushButton#navTheme:hover { background:%4; }"
                                         "QPushButton#navTheme:pressed { background:%5; }")
                                         .arg(Theme::kTextPrimary,
                                              Theme::kControlBg,
                                              Theme::kBorder,
                                              Theme::kHoverBg,
                                              Theme::kPressedBg));
    }
    setStyleSheet(QStringLiteral(
                      "QFrame#navBar { background:%1; border:1px solid %2; border-radius:8px; }"
                      "QFrame#navDivider { background:%3; border:none; }")
                      .arg(Theme::kWindowBg, Theme::kBorder, Theme::kDivider));
    if (m_userButton) {
        m_userButton->setStyleSheet(QStringLiteral(
                                        "QPushButton#navUser { background:transparent; border:1px solid transparent;"
                                        "  border-radius:8px; text-align:left; }"
                                        "QPushButton#navUser:hover { background:%1; border-color:%2; }"
                                        "QPushButton#navUser:pressed { background:%3; }")
                                        .arg(Theme::kHoverBg, Theme::kBorder, Theme::kPressedBg));
    }
    if (m_userName) {
        m_userName->setStyleSheet(QStringLiteral("color:%1;font-size:14px;font-weight:600;background:transparent;border:none;")
                                      .arg(Theme::kTextPrimary));
    }
    setUserIdentity(m_username, m_role);
    update();
}

void NavigationBar::setUserIdentity(const QString &username, Role role)
{
    m_username = username;
    m_role = role;
    if (!m_userButton) {
        return;
    }
    const bool loggedIn = role != Role::None && !username.isEmpty();
    QString color = QLatin1String(Theme::kStateUnknown);
    if (role == Role::Administrator) {
        color = QLatin1String(Theme::kStateWarning);
    } else if (role == Role::Technician) {
        color = QLatin1String(Theme::kAccent);
    } else if (role == Role::Operator) {
        color = QLatin1String(Theme::kStateSuccess);
    }

    m_userAvatar->setText(loggedIn ? username.left(1).toUpper() : QStringLiteral("?"));
    m_userAvatar->setStyleSheet(QStringLiteral("color:%1;background:%2;border:1px solid %1;"
                                               "border-radius:17px;font-size:15px;font-weight:700;")
                                    .arg(color, QString::fromLatin1(loggedIn ? Theme::kControlBg
                                                                             : Theme::kPanelBg)));
    const QFontMetrics metrics(m_userName->font());
    m_userName->setText(loggedIn ? metrics.elidedText(username, Qt::ElideRight, 96)
                                 : QStringLiteral("未登录"));
    m_userRole->setText(loggedIn ? Auth::roleDisplayName(role) : QStringLiteral("点击登录"));
    m_userRole->setStyleSheet(QStringLiteral("color:%1;font-size:12px;background:transparent;border:none;")
                                  .arg(loggedIn ? color : QString::fromLatin1(Theme::kTextMuted)));
    m_userButton->setToolTip(loggedIn
                                 ? QStringLiteral("%1（%2）。点击修改密码或退出登录")
                                       .arg(username, Auth::roleDisplayName(role))
                                 : QStringLiteral("登录以修改受权限控制的设置"));
}

void NavigationBar::setCurrentPage(int page)
{
    if (page < 0 || page >= m_items.size()) {
        return;
    }
    m_current = page;
    m_items.at(page)->setChecked(true);
}
