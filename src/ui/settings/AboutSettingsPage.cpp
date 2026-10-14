#include "ui/settings/AboutSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "camera/HikCameraConfig.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QFileInfo>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QTabWidget>
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
    root->setSpacing(0);
    auto *tabs = settingsTabs(this);
    root->addWidget(tabs);

    QVBoxLayout *versionTab = addSettingsTab(tabs, QStringLiteral("版本信息"));
    auto *version = createPanel(QStringLiteral("软件"), nullptr, this);
    auto *versionHost = new QWidget(version);
    QFormLayout *versionForm = settingsFormLayout(versionHost);
    const auto addInfo = [versionForm, versionHost](const QString &name, const QString &value) {
        auto *label = settingsHint(value, versionHost);
        label->setStyleSheet(QStringLiteral(
                                 "color:%1;font-size:14px;border:none;background:transparent;")
                                 .arg(Theme::kTextPrimary));
        addSettingsField(versionForm, name, label, QString(), versionHost);
    };
    addInfo(QStringLiteral("名称"), QStringLiteral("阴级铜表面粒子智能修整系统（MzTLZ）"));
    addInfo(QStringLiteral("版本号"), QStringLiteral("未定义（CMake project 没有 VERSION）"));
    addInfo(QStringLiteral("范围"), QStringLiteral("打磨机构上位机，不含整线画面和威纶通 HMI"));
    qobject_cast<QVBoxLayout *>(version->layout())->addWidget(versionHost);
    versionTab->addWidget(version);
    versionTab->addWidget(settingsPendingBlock(
        QStringLiteral("节点版本"),
        QStringLiteral("算法 / 相机 / 后端节点没有版本 topic 或 ini 键。"),
        this));
    versionTab->addStretch(1);

    QVBoxLayout *pathsTab = addSettingsTab(tabs, QStringLiteral("配置路径"), false);
    m_paths = settingsReadOnlyText(this);
    pathsTab->addWidget(m_paths, 1);
    pathsTab->addWidget(settingsSourceBanner(
        QStringLiteral("各子页实际读写的配置文件，查找规则与运行时一致。"),
        this));
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
