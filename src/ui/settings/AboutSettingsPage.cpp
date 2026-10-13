#include "ui/settings/AboutSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/UiHelpers.h"
#include "camera/HikCameraConfig.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QFileInfo>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace {

QString pathLine(const QString &name, const QString &path)
{
    if (path.isEmpty()) {
        return QStringLiteral("%1：未解析到文件").arg(name);
    }
    return QStringLiteral("%1：%2（%3）")
        .arg(name, path, QFileInfo::exists(path) ? QStringLiteral("存在") : QStringLiteral("尚不存在"));
}

} // namespace

AboutSettingsPage::AboutSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);
    root->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：编译进界面的说明，加上各子页已经使用的 ini 路径。不另建版本配置。"),
        this));

    auto *version = createPanel(QStringLiteral("版本"), nullptr, this);
    auto *versionLayout = qobject_cast<QVBoxLayout *>(version->layout());
    versionLayout->addWidget(settingsHint(
        QStringLiteral("软件：阴级铜表面粒子智能修整系统（MzTLZ）。"),
        version));
    versionLayout->addWidget(settingsHint(
        QStringLiteral("版本号：CMake project(MzTLZ) 没有 VERSION，界面不编造版本号。"),
        version));
    versionLayout->addWidget(settingsHint(
        QStringLiteral("设置信息架构：settings-ia v0.1，增量在 baseline-pre-settings-ia 之上。"),
        version));
    versionLayout->addWidget(settingsHint(
        QStringLiteral("范围：打磨机构上位机。不含整线画面，也不包含威纶通 HMI。"),
        version));
    root->addWidget(version);

    auto *paths = createPanel(QStringLiteral("配置路径"), nullptr, this);
    auto *pathsLayout = qobject_cast<QVBoxLayout *>(paths->layout());
    m_paths = settingsReadOnlyText(paths);
    pathsLayout->addWidget(m_paths);
    root->addWidget(paths);

    auto *pending = createPanel(QStringLiteral("节点版本"), nullptr, this);
    auto *pendingLayout = qobject_cast<QVBoxLayout *>(pending->layout());
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("算法 / 相机 / 后端节点版本"),
        QStringLiteral("待对接 节点版本上报（没有版本 topic 或 ini 键）。"),
        pending));
    root->addWidget(pending);
    root->addStretch(1);
    reloadFromSource();
}

QString AboutSettingsPage::bindingSummary() const
{
    return QStringLiteral("关于页只展示路径和说明。软件版本号未在工程中定义。节点版本待对接。");
}

void AboutSettingsPage::reloadFromSource()
{
    if (!m_paths) {
        return;
    }
    const QStringList lines = {
        pathLine(QStringLiteral("backend_params.ini"), backendParamsIniPath()),
        pathLine(QStringLiteral("hik_camera.ini"), HikCameraConfig::defaultFilePath()),
        pathLine(QStringLiteral("display.ini"), displayIniPath()),
        pathLine(QStringLiteral("node_launcher.ini"), nodeLauncherIniPath()),
        pathLine(QStringLiteral("database.ini"), databaseIniPath()),
    };
    m_paths->setPlainText(lines.join(QStringLiteral("\n")));
}
