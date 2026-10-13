#ifndef RUNTIMEINIPATHS_H
#define RUNTIMEINIPATHS_H

#include <QString>

class QSettings;

// 与现有读取方使用同一路径，避免设置页另写一份配置。
QString backendParamsIniPath();
QString databaseIniPath();
QString displayIniPath();
QString nodeLauncherIniPath();
QString resolveNodeLauncherScript(const QString &key);

bool syncSettingsFile(QSettings &settings, const QString &filePath, QString *error);

#endif // RUNTIMEINIPATHS_H
