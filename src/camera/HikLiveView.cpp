#include "camera/HikLiveView.h"

#include "app/Theme.h"

#include <QLabel>
#include <QPalette>
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
    m_statusLabel->raise();
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

void HikLiveView::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    const QRect content = rect().adjusted(1, 1, -1, -1);
    m_videoSurface->setGeometry(content);
    m_statusLabel->setGeometry(content);
}
