#include "app/NotificationDrawer.h"

#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "ui/panels/EventLogPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int kDrawerWidth = 460;
constexpr int kAnimationMs = 180;

} // namespace

NotificationDrawer::NotificationDrawer(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("notificationDrawer"));
    setStyleSheet(QStringLiteral(
                      "QFrame#notificationDrawer { background:%1; border:1px solid %2; border-radius:8px; }"
                      "QPushButton#drawerClose { color:%3; background:transparent; border:none; font-size:18px; }"
                      "QPushButton#drawerClose:hover { color:%4; }")
                      .arg(Theme::kPanelBg, Theme::kBorder, Theme::kTextMuted, Theme::kTextPrimary));
    Theme::watch(this, [this]() {
        setStyleSheet(QStringLiteral(
                          "QFrame#notificationDrawer { background:%1; border:1px solid %2; border-radius:8px; }"
                          "QPushButton#drawerClose { color:%3; background:transparent; border:none; font-size:18px; }"
                          "QPushButton#drawerClose:hover { color:%4; }")
                          .arg(Theme::kPanelBg, Theme::kBorder, Theme::kTextMuted, Theme::kTextPrimary));
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 12);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);
    auto *title = makeLabel(QStringLiteral("报警与事件日志"), QStringLiteral("panelTitle"));
    header->addWidget(title);
    header->addStretch(1);
    auto *clearButton = new ElaPushButton(QStringLiteral("清空"), this);
    clearButton->setFixedSize(64, 28);
    applySecondaryButtonStyle(clearButton);
    header->addWidget(clearButton);
    auto *closeButton = new QPushButton(QStringLiteral("×"), this);
    closeButton->setObjectName(QStringLiteral("drawerClose"));
    closeButton->setFixedSize(28, 28);
    closeButton->setCursor(Qt::PointingHandCursor);
    header->addWidget(closeButton);
    layout->addLayout(header);

    m_subtitle = makeLabel(QString(), QStringLiteral("sectionHint"));
    layout->addWidget(m_subtitle);

    auto *divider = new QFrame(this);
    divider->setFixedHeight(1);
    Theme::bindStyle(divider, []() {
        return QStringLiteral("background:%1;border:none;").arg(Theme::kDivider);
    });
    layout->addWidget(divider);

    m_log = new EventLogPanel(this, false);
    layout->addWidget(m_log, 1);

    m_animation = new QPropertyAnimation(this, "geometry", this);
    m_animation->setDuration(kAnimationMs);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (!m_open) {
            hide();
        }
    });

    connect(closeButton, &QPushButton::clicked, this, &NotificationDrawer::close);
    connect(clearButton, &ElaPushButton::clicked, this, [this]() {
        m_log->clear();
        m_warnCount = 0;
        m_errorCount = 0;
        m_subtitle->setText(QStringLiteral("日志已清空"));
    });

    m_subtitle->setText(QStringLiteral("本次运行：0 条警告，0 条错误"));
    hide();
}

void NotificationDrawer::appendEvent(const QString &level, const QString &message)
{
    if (m_log) {
        m_log->appendEvent(level, message);
    }
    const bool isError = level.compare(QStringLiteral("ERROR"), Qt::CaseInsensitive) == 0;
    const bool isWarn = level.compare(QStringLiteral("WARN"), Qt::CaseInsensitive) == 0
                        || level.compare(QStringLiteral("WARNING"), Qt::CaseInsensitive) == 0;
    if (isError) {
        ++m_errorCount;
    } else if (isWarn) {
        ++m_warnCount;
    }
    if (isError || isWarn) {
        m_subtitle->setText(QStringLiteral("本次运行：%1 条警告，%2 条错误").arg(m_warnCount).arg(m_errorCount));
        if (!m_open) {
            ++m_unread;
            emit unreadChanged(m_unread);
        }
    }
}

void NotificationDrawer::setDockArea(const QRect &area)
{
    m_dockArea = area;
    moveTo(m_open, false);
}

void NotificationDrawer::open()
{
    if (m_open) {
        return;
    }
    m_open = true;
    m_unread = 0;
    emit unreadChanged(0);
    show();
    raise();
    moveTo(true, true);
    emit openChanged(true);
}

void NotificationDrawer::close()
{
    if (!m_open) {
        return;
    }
    m_open = false;
    moveTo(false, true);
    emit openChanged(false);
}

void NotificationDrawer::toggle()
{
    if (m_open) {
        close();
    } else {
        open();
    }
}

void NotificationDrawer::moveTo(bool open, bool animated)
{
    if (m_dockArea.isNull()) {
        return;
    }
    const int width = qMin(kDrawerWidth, m_dockArea.width());
    const QRect shown(m_dockArea.right() - width + 1, m_dockArea.top(), width, m_dockArea.height());
    const QRect hidden(m_dockArea.right() + 1, m_dockArea.top(), width, m_dockArea.height());
    const QRect target = open ? shown : hidden;
    if (!animated) {
        m_animation->stop();
        setGeometry(target);
        if (!open) {
            hide();
        }
        return;
    }
    m_animation->stop();
    m_animation->setStartValue(geometry().isValid() && isVisible() ? geometry() : (open ? hidden : shown));
    m_animation->setEndValue(target);
    m_animation->start();
}
