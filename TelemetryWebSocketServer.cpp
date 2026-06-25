#include "TelemetryWebSocketServer.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QWebSocket>
#include <QWebSocketServer>

TelemetryWebSocketServer::TelemetryWebSocketServer(QObject *parent)
    : QObject(parent)
{
}

TelemetryWebSocketServer::~TelemetryWebSocketServer()
{
    stop();
}

bool TelemetryWebSocketServer::start(quint16 port)
{
    if (m_server && m_server->isListening()) {
        return true;
    }

    m_server = new QWebSocketServer(QStringLiteral("MzTLZ Telemetry Server"),
                                    QWebSocketServer::NonSecureMode,
                                    this);
    connect(m_server, &QWebSocketServer::newConnection, this, &TelemetryWebSocketServer::onNewConnection);

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        const QString error = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        emit errorMessage(QString("WebSocket 启动失败: %1").arg(error));
        return false;
    }

    emit infoMessage(QString("WebSocket 已启动: ws://127.0.0.1:%1").arg(m_server->serverPort()));
    return true;
}

void TelemetryWebSocketServer::stop()
{
    if (!m_server) {
        return;
    }

    disconnect(m_server, nullptr, this, nullptr);
    m_server->close();
    delete m_server;
    m_server = nullptr;
}

quint16 TelemetryWebSocketServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

void TelemetryWebSocketServer::onNewConnection()
{
    auto *socket = m_server->nextPendingConnection();
    connect(socket, &QWebSocket::textMessageReceived, this, &TelemetryWebSocketServer::processTextMessage);
    connect(socket, &QWebSocket::disconnected, this, &TelemetryWebSocketServer::onSocketDisconnected);

    socket->sendTextMessage(QStringLiteral("{\"status\":\"connected\"}"));
    emit infoMessage(QString("WebSocket 客户端已连接: %1").arg(socket->peerAddress().toString()));
}

void TelemetryWebSocketServer::processTextMessage(const QString &message)
{
    auto *socket = qobject_cast<QWebSocket *>(sender());
    double speed = 0;
    double torque = 0;
    QString error;

    if (!parseTelemetryMessage(message, &speed, &torque, &error)) {
        if (socket) {
            socket->sendTextMessage(QString("{\"status\":\"error\",\"message\":\"%1\"}").arg(error));
        }
        emit errorMessage(QString("WebSocket 数据无效: %1").arg(error));
        return;
    }

    emit telemetryReceived(speed, torque);
    if (socket) {
        socket->sendTextMessage(QString("{\"status\":\"ok\",\"speed\":%1,\"torque\":%2}")
                                    .arg(speed)
                                    .arg(torque));
    }
}

void TelemetryWebSocketServer::onSocketDisconnected()
{
    auto *socket = qobject_cast<QWebSocket *>(sender());
    if (!socket) {
        return;
    }

    emit infoMessage(QString("WebSocket 客户端已断开: %1").arg(socket->peerAddress().toString()));
    socket->deleteLater();
}

bool TelemetryWebSocketServer::parseTelemetryMessage(const QString &message,
                                                     double *speed,
                                                     double *torque,
                                                     QString *error) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("JSON 格式错误");
        return false;
    }

    const QJsonObject object = document.object();
    if (!object.contains("speed") || !object.value("speed").isDouble()) {
        *error = QStringLiteral("缺少数值字段 speed");
        return false;
    }
    if (!object.contains("torque") || !object.value("torque").isDouble()) {
        *error = QStringLiteral("缺少数值字段 torque");
        return false;
    }

    *speed = object.value("speed").toDouble();
    *torque = object.value("torque").toDouble();
    return true;
}
