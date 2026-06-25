#ifndef TELEMETRYWEBSOCKETSERVER_H
#define TELEMETRYWEBSOCKETSERVER_H

#include <QObject>

class QWebSocket;
class QWebSocketServer;

class TelemetryWebSocketServer : public QObject
{
    Q_OBJECT

public:
    explicit TelemetryWebSocketServer(QObject *parent = nullptr);
    ~TelemetryWebSocketServer();

    bool start(quint16 port = 9002);
    void stop();
    quint16 port() const;

signals:
    void telemetryReceived(double speed, double torque);
    void infoMessage(const QString &message);
    void errorMessage(const QString &message);

private:
    void onNewConnection();
    void processTextMessage(const QString &message);
    void onSocketDisconnected();
    bool parseTelemetryMessage(const QString &message, double *speed, double *torque, QString *error) const;

private:
    QWebSocketServer *m_server{nullptr};
};

#endif // TELEMETRYWEBSOCKETSERVER_H
