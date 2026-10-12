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

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *m_videoSurface{nullptr};
    QLabel *m_statusLabel{nullptr};
};

#endif // HIKLIVEVIEW_H
