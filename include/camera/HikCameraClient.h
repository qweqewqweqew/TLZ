#ifndef HIKCAMERACLIENT_H
#define HIKCAMERACLIENT_H

#include <QObject>
#include <QThread>
#include <QtGui/qwindowdefs.h>

class HikCameraWorker;

class HikCameraClient : public QObject
{
    Q_OBJECT

public:
    explicit HikCameraClient(QObject *parent = nullptr);
    ~HikCameraClient() override;

    void startPreview(WId targetWindow);
    void stopPreview();
    void shutdown();

signals:
    void connectionStatusChanged(bool connected, const QString &message);
    void infoMessage(const QString &message);
    void errorMessage(const QString &message);

private:
    QThread m_workerThread;
    HikCameraWorker *m_worker{nullptr};
    bool m_shutdown{false};
};

#endif // HIKCAMERACLIENT_H
