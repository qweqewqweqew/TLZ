#include "camera/HikCameraConfig.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>

QString HikCameraConfig::defaultFilePath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/hik_camera.ini");
}

HikCameraConfig HikCameraConfig::load(const QString &filePath, QString *error)
{
    HikCameraConfig config;
    if (!QFileInfo::exists(filePath)) {
        if (error) {
            *error = QStringLiteral("未找到海康相机配置文件：%1").arg(filePath);
        }
        return config;
    }

    QSettings settings(filePath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("HikCamera"));
    config.enabled = settings.value(QStringLiteral("Enabled"), false).toBool();
    config.address = settings.value(QStringLiteral("Address")).toString().trimmed();
    config.port = settings.value(QStringLiteral("Port"), 8000).toInt();
    config.username = settings.value(QStringLiteral("Username"), QStringLiteral("admin"))
                          .toString().trimmed();
    config.password = settings.value(QStringLiteral("Password")).toString();
    config.channel = settings.value(QStringLiteral("Channel"), 1).toInt();
    config.streamType = settings.value(QStringLiteral("StreamType"), 1).toInt();
    config.linkMode = settings.value(QStringLiteral("LinkMode"), 0).toInt();
    config.loginTimeoutMs = settings.value(QStringLiteral("LoginTimeoutMs"), 3000).toInt();
    config.reconnectIntervalMs = settings.value(QStringLiteral("ReconnectIntervalMs"), 10000).toInt();
    settings.endGroup();

    if (settings.status() != QSettings::NoError && error) {
        *error = QStringLiteral("读取海康相机配置文件失败：%1").arg(filePath);
    }
    return config;
}

bool HikCameraConfig::save(const QString &filePath, QString *error) const
{
    QString message;
    if (port <= 0 || port > 65535) {
        message = QStringLiteral("海康相机端口无效");
    } else if (channel <= 0) {
        message = QStringLiteral("海康相机通道号必须大于 0");
    } else if (streamType < 0 || streamType > 10) {
        message = QStringLiteral("海康相机码流类型超出范围");
    } else if (linkMode < 0 || linkMode > 1) {
        message = QStringLiteral("海康相机传输方式仅支持 TCP 或 UDP");
    } else if (loginTimeoutMs < 100 || loginTimeoutMs > 60000) {
        message = QStringLiteral("海康相机登录超时必须在 100~60000 ms 之间");
    } else if (reconnectIntervalMs < 1000 || reconnectIntervalMs > 60000) {
        message = QStringLiteral("海康相机重连间隔必须在 1000~60000 ms 之间");
    } else if (enabled && address.trimmed().isEmpty()) {
        message = QStringLiteral("海康相机 IP 未配置");
    } else if (enabled && username.trimmed().isEmpty()) {
        message = QStringLiteral("海康相机用户名未配置");
    } else if (enabled && password.isEmpty()) {
        message = QStringLiteral("海康相机密码未配置");
    }

    if (!message.isEmpty()) {
        if (error) {
            *error = message;
        }
        return false;
    }

    QSettings settings(filePath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("HikCamera"));
    settings.setValue(QStringLiteral("Enabled"), enabled);
    settings.setValue(QStringLiteral("Address"), address.trimmed());
    settings.setValue(QStringLiteral("Port"), port);
    settings.setValue(QStringLiteral("Username"), username.trimmed());
    settings.setValue(QStringLiteral("Password"), password);
    settings.setValue(QStringLiteral("Channel"), channel);
    settings.setValue(QStringLiteral("StreamType"), streamType);
    settings.setValue(QStringLiteral("LinkMode"), linkMode);
    settings.setValue(QStringLiteral("LoginTimeoutMs"), loginTimeoutMs);
    settings.setValue(QStringLiteral("ReconnectIntervalMs"), reconnectIntervalMs);
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) {
            *error = QStringLiteral("写入海康配置失败：%1").arg(filePath);
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool HikCameraConfig::validate(QString *error) const
{
    QString message;
    if (!enabled) {
        message = QStringLiteral("海康相机功能尚未启用，请先填写 hik_camera.ini");
    } else if (address.isEmpty()) {
        message = QStringLiteral("海康相机 IP 未配置");
    } else if (port <= 0 || port > 65535) {
        message = QStringLiteral("海康相机端口无效");
    } else if (username.isEmpty()) {
        message = QStringLiteral("海康相机用户名未配置");
    } else if (password.isEmpty()) {
        message = QStringLiteral("海康相机密码未配置");
    } else if (channel <= 0) {
        message = QStringLiteral("海康相机通道号必须大于 0");
    } else if (streamType < 0 || streamType > 10) {
        message = QStringLiteral("海康相机码流类型超出范围");
    } else if (linkMode < 0 || linkMode > 1) {
        message = QStringLiteral("海康相机传输方式仅支持 TCP 或 UDP");
    } else if (loginTimeoutMs < 100 || loginTimeoutMs > 60000) {
        message = QStringLiteral("海康相机登录超时必须在 100~60000 ms 之间");
    } else if (reconnectIntervalMs < 1000 || reconnectIntervalMs > 60000) {
        message = QStringLiteral("海康相机重连间隔必须在 1000~60000 ms 之间");
    }

    if (error) {
        *error = message;
    }
    return message.isEmpty();
}
