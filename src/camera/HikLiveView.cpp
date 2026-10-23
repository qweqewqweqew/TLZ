#include "camera/HikLiveView.h"

#include "app/Theme.h"

#include <QColor>
#include <QLabel>
#include <QPalette>
#include <QStyle>
#include <QToolButton>
#include <QResizeEvent>

HikLiveView::HikLiveView(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("hikLiveView"));
    setStyleSheet(QStringLiteral(
        "QFrame#hikLiveView{background:%1;border:1px solid %2;border-radius:6px;}")
                      .arg(Theme::kCanvasBg, Theme::kBorder));

    m_videoSurface = new QWidget(this);
    m_videoSurface->setObjectName(QStringLiteral("hikVideoSurface"));
    m_videoSurface->setAttribute(Qt::WA_NativeWindow);
    m_videoSurface->setAutoFillBackground(true);
    QPalette surfacePalette = m_videoSurface->palette();
    surfacePalette.setColor(QPalette::Window, QColor(Theme::kCanvasBg));
    m_videoSurface->setPalette(surfacePalette);

    m_statusLabel = new QLabel(QStringLiteral("海康相机尚未连接"), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel{background:%1;color:%2;font-size:16px;border:none;padding:24px;}")
                                     .arg(Theme::kCanvasBg, Theme::kTextMuted));

    m_expandButton = new QToolButton(this);
    m_expandButton->setObjectName(QStringLiteral("hikExpandButton"));
    m_expandButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    m_expandButton->setIconSize(QSize(16, 16));
    m_expandButton->setFixedSize(30, 30);
    m_expandButton->setCursor(Qt::PointingHandCursor);
    m_expandButton->setToolTip(QStringLiteral("放大预览"));
    m_expandButton->setStyleSheet(QStringLiteral(
        "QToolButton#hikExpandButton{background:%1;border:1px solid %2;border-radius:5px;}"
        "QToolButton#hikExpandButton:hover{background:%3;}"
        "QToolButton#hikExpandButton:pressed{background:%4;}")
                                     .arg(Theme::kControlBg, Theme::kBorder,
                                          Theme::kHoverBg, Theme::kPressedBg));
    connect(m_expandButton, &QToolButton::clicked, this, &HikLiveView::expandRequested);

    m_statusLabel->raise();
    m_expandButton->raise();
    Theme::watch(this, [this]() {
        setStyleSheet(QStringLiteral(
                          "QFrame#hikLiveView{background:%1;border:1px solid %2;border-radius:6px;}")
                          .arg(Theme::kCanvasBg, Theme::kBorder));
        QPalette surfacePalette = m_videoSurface->palette();
        surfacePalette.setColor(QPalette::Window, QColor(Theme::kCanvasBg));
        m_videoSurface->setPalette(surfacePalette);
        if (m_statusLabel) {
            m_statusLabel->setStyleSheet(QStringLiteral(
                                             "QLabel{background:%1;color:%2;font-size:16px;border:none;padding:24px;}")
                                             .arg(Theme::kCanvasBg, Theme::kTextMuted));
        }
        if (m_expandButton) {
            m_expandButton->setStyleSheet(QStringLiteral(
                "QToolButton#hikExpandButton{background:%1;border:1px solid %2;border-radius:5px;}"
                "QToolButton#hikExpandButton:hover{background:%3;}"
                "QToolButton#hikExpandButton:pressed{background:%4;}")
                                             .arg(Theme::kControlBg, Theme::kBorder,
                                                  Theme::kHoverBg, Theme::kPressedBg));
        }
    });
}
WId HikLiveView::videoWindowId()
{
    return m_videoSurface->winId();
}

void HikLiveView::setConnectionStatus(bool connected, const QString &message)
{
    if (connected) {
        m_statusLabel->hide();
        return;
    }

    m_statusLabel->setText(message.isEmpty()
                               ? QStringLiteral("海康相机尚未连接")
                               : message);
    m_statusLabel->show();
    m_statusLabel->raise();
}

void HikLiveView::setExpandButtonVisible(bool visible)
{
    if (m_expandButton) {
        m_expandButton->setVisible(visible);
    }
}

void HikLiveView::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    const QRect content = rect().adjusted(1, 1, -1, -1);
    const QSize videoSize = content.size().scaled(QSize(16, 9), Qt::KeepAspectRatio);
    const QRect videoRect(QPoint(content.left() + (content.width() - videoSize.width()) / 2,
                                 content.top() + (content.height() - videoSize.height()) / 2),
                          videoSize);
    m_videoSurface->setGeometry(videoRect);
    m_statusLabel->setGeometry(content);
    if (m_expandButton) {
        m_expandButton->move(content.right() - m_expandButton->width() - 10,
                             content.top() + 10);
        m_expandButton->raise();
    }
    m_statusLabel->raise();
}
