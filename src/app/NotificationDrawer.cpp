#include "app/NotificationDrawer.h"

#include "ElaPushButton.h"
#include "app/Logger.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "ui/panels/EventLogPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegion>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {

constexpr int kDrawerWidth = 460;
constexpr int kAnimationMs = 180;
constexpr int kCornerRadius = 8;

} // namespace

NotificationDrawer::NotificationDrawer(QWidget *parent)
    : QFrame(parent)
{
    // 抽屉必须是顶层浮层：作为主窗口的普通子控件时，它与内容区各页面共享父控件
    // 的 Z 序，会出现"画在最上面、鼠标却落到下面页面"的情况（点清空/关闭无反应）。
    // 设为 Qt::Tool 独立窗口后，它不再参与任何子控件的命中测试。
    // 注意：这里不能用 WA_TranslucentBackground —— 顶层透明窗口会让整棵控件树的
    // 绘制带上 alpha，下层主窗口内容直接透上来。改为不透明窗口 + 圆角蒙版。
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAutoFillBackground(true);
    QPalette drawerPalette = palette();
    drawerPalette.setColor(QPalette::Window, QColor(Theme::kPanelBg));
    setPalette(drawerPalette);
    setObjectName(QStringLiteral("notificationDrawer"));
    const auto applyButtons = [this]() {
        setStyleSheet(QStringLiteral(
                          "QPushButton#drawerClose { color:%1; background:transparent; border:none; font-size:18px; }"
                          "QPushButton#drawerClose:hover { color:%2; }")
                          .arg(Theme::kTextMuted, Theme::kTextPrimary));
    };
    applyButtons();
    Theme::watch(this, [this, applyButtons]() {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(Theme::kPanelBg));
        setPalette(pal);
        applyButtons();
        update();
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
            return;
        }
        // 动画期间可能被后续创建的兄弟控件压到下面，结束时重新置顶。
        raise();
    });

    closeButton->setFocusPolicy(Qt::NoFocus);
    clearButton->setFocusPolicy(Qt::NoFocus);
    connect(closeButton, &QPushButton::clicked, this, [this]() {
        LOG("[Drawer] 关闭按钮被点击");
        close();
    });
    connect(clearButton, &ElaPushButton::clicked, this, [this]() {
        LOG("[Drawer] 清空按钮被点击");
        if (m_log) {
            m_log->clear();
        }
        m_warnCount = 0;
        m_errorCount = 0;
        m_unread = 0;
        emit unreadChanged(0);
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
    // 顶层浮层窗口逐帧移动会每一帧都移动原生窗口，卡顿且容易闪烁。
    // 先定位到目标几何再 show，直接出现在最终位置。
    moveTo(true, false);
    show();
    raise();
    emit openChanged(true);
}

void NotificationDrawer::close()
{
    if (!m_open && !isVisible()) {
        return;
    }
    m_open = false;
    // 直接隐藏，避免动画未结束或被外部 resize 打断时看起来“点了没反应”。
    if (m_animation) {
        m_animation->stop();
    }
    hide();
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

void NotificationDrawer::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateRoundedMask();
}

void NotificationDrawer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // 自己绘制不透明圆角背景：不依赖透明窗口，避免下层内容透上来。
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor(Theme::kBorder), 1));
    painter.setBrush(QColor(Theme::kPanelBg));
    painter.drawRoundedRect(box, kCornerRadius, kCornerRadius);
}

void NotificationDrawer::updateRoundedMask()
{
    // 窗口是不透明的，用蒙版把四个圆角切掉，圆角外露出主窗口内容。
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), kCornerRadius, kCornerRadius);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}
