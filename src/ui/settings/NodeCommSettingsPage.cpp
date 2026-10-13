#include "ui/settings/NodeCommSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/UiHelpers.h"
#include "data/Database.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSettings>
#include <QVBoxLayout>

namespace {

bool isSecretKey(const QString &key)
{
    const QString lower = key.toLower();
    return lower.contains(QStringLiteral("password"))
           || lower.contains(QStringLiteral("passwd"))
           || lower.contains(QStringLiteral("pwd"))
           || lower.contains(QStringLiteral("secret"))
           || lower.contains(QStringLiteral("token"));
}

QString maskSecret(const QString &value)
{
    if (value.isEmpty()) {
        return QStringLiteral("（空）");
    }
    return QStringLiteral("••••••••（已隐藏，%1 个字符）").arg(value.size());
}

QString existsText(const QString &path)
{
    if (path.isEmpty()) {
        return QStringLiteral("未找到");
    }
    return QFileInfo::exists(path)
               ? QStringLiteral("%1（文件存在）").arg(path)
               : QStringLiteral("%1（文件不存在）").arg(path);
}

} // namespace

NodeCommSettingsPage::NodeCommSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);
    root->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：node_launcher.ini（与主窗口启动/停止脚本同一查找规则）"
                       "和 database.ini（与 DatabaseBootstrap 相同，程序目录）。"
                       "本页只读。启停脚本属于危险项，不在界面里修改。"),
        this));

    auto *launcher = createPanel(QStringLiteral("节点脚本"), nullptr, this);
    auto *launcherLayout = qobject_cast<QVBoxLayout *>(launcher->layout());
    m_launcherPath = settingsHint(QString(), launcher);
    m_startScript = settingsHint(QString(), launcher);
    m_stopScript = settingsHint(QString(), launcher);
    m_launcherPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_startScript->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_stopScript->setTextInteractionFlags(Qt::TextSelectableByMouse);
    launcherLayout->addWidget(m_launcherPath);
    launcherLayout->addWidget(m_startScript);
    launcherLayout->addWidget(m_stopScript);
    launcherLayout->addWidget(settingsHint(
        QStringLiteral("计划权限：管理员可改，技术员只读。当前界面保持只读，避免误改 StartAll/StopAll。"),
        launcher));
    root->addWidget(launcher);

    auto *database = createPanel(QStringLiteral("数据库"), nullptr, this);
    auto *databaseLayout = qobject_cast<QVBoxLayout *>(database->layout());
    m_databaseSummary = settingsReadOnlyText(database);
    databaseLayout->addWidget(m_databaseSummary);
    root->addWidget(database);

    auto *pending = createPanel(QStringLiteral("未对接通信项"), nullptr, this);
    auto *pendingLayout = qobject_cast<QVBoxLayout *>(pending->layout());
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("PLC / UDP 地址与超时"),
        QStringLiteral("待对接 node_launcher.ini / 后端字段（PLC IP 等仍写在 start_all.bat 内，ini 没有这些键）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("单个节点启停"),
        QStringLiteral("待对接 node_launcher.ini（现只有 StartAll / StopAll，且本页不提供执行按钮）。"),
        pending));
    root->addWidget(pending);
    root->addStretch(1);
    reloadFromSource();
}

QString NodeCommSettingsPage::bindingSummary() const
{
    return QStringLiteral("只读摘要：node_launcher.ini、database.ini。密码类键会掩码。PLC/UDP 待对接。");
}

void NodeCommSettingsPage::reloadFromSource()
{
    const QString launcherPath = nodeLauncherIniPath();
    if (m_launcherPath) {
        m_launcherPath->setText(QStringLiteral("node_launcher.ini：%1").arg(existsText(launcherPath)));
    }
    if (m_startScript) {
        m_startScript->setText(QStringLiteral("StartAll：%1")
                                   .arg(existsText(resolveNodeLauncherScript(QStringLiteral("StartAll")))));
    }
    if (m_stopScript) {
        m_stopScript->setText(QStringLiteral("StopAll：%1")
                                  .arg(existsText(resolveNodeLauncherScript(QStringLiteral("StopAll")))));
    }

    QStringList lines;
    const QString dbIni = databaseIniPath();
    lines << QStringLiteral("database.ini：%1").arg(existsText(dbIni));
    bool sawSecret = false;
    if (QFileInfo::exists(dbIni)) {
        QSettings settings(dbIni, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        const QStringList keys = settings.allKeys();
        if (keys.isEmpty()) {
            lines << QStringLiteral("（文件里没有键）");
        }
        for (const QString &key : keys) {
            const QString raw = settings.value(key).toString();
            if (isSecretKey(key)) {
                sawSecret = true;
                lines << QStringLiteral("%1 = %2").arg(key, maskSecret(raw));
            } else {
                lines << QStringLiteral("%1 = %2").arg(key, raw);
            }
        }
    }
    if (!sawSecret) {
        lines << QStringLiteral("未发现密码类键（password/passwd/pwd/secret/token）。本分支是 SQLite 文件路径，无数据库密码。");
    }
    const QString livePath = qEnvironmentVariable("MZTLZ_DB_PATH").trimmed();
    lines << QStringLiteral("MZTLZ_DB_PATH：%1")
                 .arg(livePath.isEmpty() ? QStringLiteral("未设置（Database::open 会失败）") : livePath);
    QString health;
    const bool healthy = Database::instance().healthCheck(&health);
    lines << (healthy
                  ? QStringLiteral("连接健康检查：通过（SELECT 1）")
                  : QStringLiteral("连接健康检查：%1").arg(health.isEmpty() ? QStringLiteral("未连接") : health));
    if (m_databaseSummary) {
        m_databaseSummary->setPlainText(lines.join(QStringLiteral("\n")));
    }
}
