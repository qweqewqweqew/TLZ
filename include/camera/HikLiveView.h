#ifndef HIKLIVEVIEW_H
#define HIKLIVEVIEW_H

#include <QFrame>
#include <QtGui/qwindowdefs.h>

class QLabel;

class HikLiveView : public QFrame
{
    Q_OBJECT

public:
    explicit HikLiveView(QWidget *parent = nullptr);

    WId videoWindowId();
    void setConnectionStatus(bool connected, const QString &message);

signals:
    void expandRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_videoSurface{nullptr};
    QLabel *m_statusLabel{nullptr};
};

#endif // HIKLIVEVIEW_H
