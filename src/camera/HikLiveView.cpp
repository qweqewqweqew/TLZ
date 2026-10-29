#include "camera/HikLiveView.h"

#include "app/Theme.h"

#include <QColor>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
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

    m_statusLabel = new QLabel(QStringLiteral("相机未连接"), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel{background:transparent;color:%1;font-size:13px;border:none;padding:8px;}")
                                     .arg(Theme::kTextMuted));

    m_videoSurface->installEventFilter(this);
    m_videoSurface->hide();
    m_statusLabel->raise();
    Theme::watch(this, [this]() {
        setStyleSheet(QStringLiteral(
                          "QFrame#hikLiveView{background:%1;border:1px solid %2;border-radius:6px;}")
                          .arg(Theme::kCanvasBg, Theme::kBorder));
        QPalette surfacePalette = m_videoSurface->palette();
        surfacePalette.setColor(QPalette::Window, QColor(Theme::kCanvasBg));
        m_videoSurface->setPalette(surfacePalette);
        if (m_statusLabel) {
            m_statusLabel->setStyleSheet(QStringLiteral(
                                             "QLabel{background:transparent;color:%1;font-size:13px;border:none;padding:8px;}")
                                             .arg(Theme::kTextMuted));
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
        if (m_videoSurface) {
            m_videoSurface->show();
            m_videoSurface->raise();
        }
        return;
    }

    // 未连接时藏起原生画面窗口，否则它会盖住状态文字，卡片看起来只剩一块空白。
    if (m_videoSurface) {
        m_videoSurface->hide();
    }
    const QString shown = message.isEmpty() || message.contains(QStringLiteral(".ini"))
                              ? QStringLiteral("相机未连接")
                              : message;
    m_statusLabel->setText(shown);
    m_statusLabel->show();
    m_statusLabel->raise();
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
    m_statusLabel->raise();
}

void HikLiveView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit expandRequested();
    }
    QFrame::mouseDoubleClickEvent(event);
}

bool HikLiveView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_videoSurface && event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit expandRequested();
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}
