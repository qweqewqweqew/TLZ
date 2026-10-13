#ifndef HIKCAMERACONFIG_H
#define HIKCAMERACONFIG_H

#include <QString>

struct HikCameraConfig
{
    bool enabled{false};
    QString address;
    int port{8000};
    QString username{QStringLiteral("admin")};
    QString password;
    int channel{1};
    int streamType{1};
    int linkMode{0};
    int loginTimeoutMs{3000};
    int reconnectIntervalMs{10000};

    static QString defaultFilePath();
    static HikCameraConfig load(const QString &filePath, QString *error = nullptr);
    bool save(const QString &filePath, QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;
};

#endif // HIKCAMERACONFIG_H
