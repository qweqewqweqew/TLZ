#ifndef HIKCAMERACLIENT_H
#define HIKCAMERACLIENT_H

#include "camera/HikCameraParameters.h"

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
    void refreshPreviewParameters();
    void updatePreviewParameters(const HikCameraParameters &parameters);
    void shutdown();

signals:
    void connectionStatusChanged(bool connected, const QString &message);
    void parametersUpdated(const HikCameraParameters &parameters);
    void infoMessage(const QString &message);
    void errorMessage(const QString &message);

private:
    QThread m_workerThread;
    HikCameraWorker *m_worker{nullptr};
    bool m_shutdown{false};
};

#endif // HIKCAMERACLIENT_H
