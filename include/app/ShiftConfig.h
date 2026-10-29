#ifndef SHIFTCONFIG_H
#define SHIFTCONFIG_H

#include "app/RuntimeIniPaths.h"

#include <QSettings>
#include <QString>
#include <QStringList>

// 班次名单和当前班次都在 backend_params.ini 的 [Report]。
// 名单为空时使用白班、晚班，日报才有班次列。用户改过名单后按保存的名单显示。
namespace ShiftConfig {

inline QStringList defaultNames()
{
    return {QStringLiteral("白班"), QStringLiteral("晚班")};
}

inline QStringList names()
{
    QSettings settings(backendParamsIniPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    const QStringList stored = settings.value(QStringLiteral("Report/Shifts")).toStringList();
    QStringList clean;
    for (QString name : stored) {
        name = name.trimmed();
        if (name.isEmpty() || clean.contains(name)) {
            continue;
        }
        clean.append(name);
    }
    return clean.isEmpty() ? defaultNames() : clean;
}

inline QString current()
{
    QSettings settings(backendParamsIniPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    const QString name = settings.value(QStringLiteral("Report/Shift")).toString().trimmed();
    return names().contains(name) ? name : QString();
}

inline void save(const QStringList &shiftNames, const QString &currentShift)
{
    QStringList clean;
    for (QString name : shiftNames) {
        name = name.trimmed();
        if (name.isEmpty() || clean.contains(name)) {
            continue;
        }
        clean.append(name);
    }
    const QString selected = clean.contains(currentShift.trimmed()) ? currentShift.trimmed() : QString();
    QSettings settings(backendParamsIniPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QStringLiteral("Report/Shifts"), clean);
    settings.setValue(QStringLiteral("Report/Shift"), selected);
    settings.sync();
}

inline void setCurrent(const QString &currentShift)
{
    save(names(), currentShift);
}

} // namespace ShiftConfig

#endif // SHIFTCONFIG_H
