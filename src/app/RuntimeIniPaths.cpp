#include "app/RuntimeIniPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace {

void appendAncestorIni(QStringList *candidates, const QString &startDir, const QString &fileName)
{
    QDir dir(startDir);
    for (int i = 0; i < 8; ++i) {
        const QString configPath = QFileInfo(dir.filePath(fileName)).absoluteFilePath();
        if (QFileInfo::exists(configPath) && !candidates->contains(configPath)) {
            candidates->append(configPath);
        }
        if (!dir.cdUp()) {
            break;
        }
    }
}

QString preferIniBesideProject(const QStringList &candidates)
{
    for (const QString &path : candidates) {
        const QDir root(QFileInfo(path).absolutePath());
        if (QFileInfo::exists(root.filePath(QStringLiteral("CMakeLists.txt")))) {
            return path;
        }
    }
    return candidates.isEmpty() ? QString() : candidates.front();
}

} // namespace

QString backendParamsIniPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("backend_params.ini"));
}

QString databaseIniPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("database.ini"));
}

QString displayIniPath()
{
    // 与 HeightMap3DFrame 原来的查找顺序一致：当前目录、程序目录各向上 8 层，
    // 优先选同目录带 CMakeLists.txt 的那份。
    QStringList candidates;
    appendAncestorIni(&candidates, QDir::currentPath(), QStringLiteral("display.ini"));
    appendAncestorIni(&candidates,
                      QCoreApplication::applicationDirPath(),
                      QStringLiteral("display.ini"));
    return preferIniBesideProject(candidates);
}

QString nodeLauncherIniPath()
{
    QStringList candidates;
    appendAncestorIni(&candidates, QDir::currentPath(), QStringLiteral("node_launcher.ini"));
    appendAncestorIni(&candidates,
                      QCoreApplication::applicationDirPath(),
                      QStringLiteral("node_launcher.ini"));
    return preferIniBesideProject(candidates);
}

QString resolveNodeLauncherScript(const QString &key)
{
    const QString configPath = nodeLauncherIniPath();
    if (configPath.isEmpty()) {
        return QString();
    }

    QSettings settings(configPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    const QString configured = settings.value(QStringLiteral("Launcher/%1").arg(key))
                                   .toString()
                                   .trimmed();
    if (configured.isEmpty()) {
        return QString();
    }

    QFileInfo info(configured);
    if (info.isRelative()) {
        info.setFile(QDir(QFileInfo(configPath).absolutePath()).filePath(configured));
    }
    if (!info.exists()) {
        return QString();
    }
    return info.absoluteFilePath();
}

bool syncSettingsFile(QSettings &settings, const QString &filePath, QString *error)
{
    settings.sync();
    if (settings.status() == QSettings::NoError) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("写入失败（QSettings status=%1）：%2")
                     .arg(int(settings.status()))
                     .arg(filePath);
    }
    return false;
}
