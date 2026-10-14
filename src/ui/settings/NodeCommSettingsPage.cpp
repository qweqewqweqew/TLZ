#include "ui/settings/NodeCommSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "data/Database.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTabWidget>
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
    root->setSpacing(0);
    auto *tabs = settingsTabs(this);
    root->addWidget(tabs);

    QVBoxLayout *scriptTab = addSettingsTab(tabs, QStringLiteral("节点脚本"));
    auto *launcher = createPanel(QStringLiteral("启动 / 停止脚本"), nullptr, this);
    auto *launcherHost = new QWidget(launcher);
    QFormLayout *launcherForm = settingsFormLayout(launcherHost);
    m_launcherPath = settingsHint(QString(), launcherHost);
    m_startScript = settingsHint(QString(), launcherHost);
    m_stopScript = settingsHint(QString(), launcherHost);
    for (QLabel *label : {m_launcherPath, m_startScript, m_stopScript}) {
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setStyleSheet(QStringLiteral(
                                 "color:%1;font-size:13px;border:none;background:transparent;")
                                 .arg(Theme::kTextPrimary));
    }
    addSettingsField(launcherForm, QStringLiteral("配置文件"), m_launcherPath, QString(), launcherHost);
    addSettingsField(launcherForm, QStringLiteral("StartAll"), m_startScript, QString(), launcherHost);
    addSettingsField(launcherForm, QStringLiteral("StopAll"), m_stopScript, QString(), launcherHost);
    qobject_cast<QVBoxLayout *>(launcher->layout())->addWidget(launcherHost);
    scriptTab->addWidget(launcher);
    scriptTab->addWidget(settingsHint(
        QStringLiteral("本页只读。启停脚本属于危险项，不在界面里修改。"),
        this));
    scriptTab->addStretch(1);
    scriptTab->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：node_launcher.ini，与主窗口启动 / 停止按钮同一查找规则。"),
        this));

    QVBoxLayout *databaseTab = addSettingsTab(tabs, QStringLiteral("数据库"), false);
    m_databaseSummary = settingsReadOnlyText(this);
    databaseTab->addWidget(m_databaseSummary, 1);
    databaseTab->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：程序目录下的 database.ini。密码类键以掩码显示。"),
        this));

    QVBoxLayout *pendingTab = addSettingsTab(tabs, QStringLiteral("PLC 与通信"));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("PLC / UDP 地址与超时"),
        QStringLiteral("PLC IP 等仍写在 start_all.bat 内，node_launcher.ini 没有这些键。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("单个节点启停"),
        QStringLiteral("node_launcher.ini 现只有 StartAll / StopAll。"),
        this));
    pendingTab->addStretch(1);
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
        m_launcherPath->setText(existsText(launcherPath));
    }
    if (m_startScript) {
        m_startScript->setText(existsText(resolveNodeLauncherScript(QStringLiteral("StartAll"))));
    }
    if (m_stopScript) {
        m_stopScript->setText(existsText(resolveNodeLauncherScript(QStringLiteral("StopAll"))));
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
