#include "ui/settings/AlarmSettingsPage.h"

#include "ElaPushButton.h"
#include "app/RuntimeIniPaths.h"
#include "app/UiHelpers.h"
#include "bridge/NodeLifecycleMonitor.h"
#include "camera/HikCameraConfig.h"
#include "data/Database.h"
#include "ui/panels/EventLogPanel.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QVBoxLayout>

AlarmSettingsPage::AlarmSettingsPage(NodeLifecycleMonitor *nodes,
                                     EventLogPanel *eventLog,
                                     QWidget *parent)
    : SettingsSubPage(parent)
    , m_nodes(nodes)
    , m_eventLog(eventLog)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *tabs = settingsTabs(this);
    root->addWidget(tabs);

    QVBoxLayout *checkTab = addSettingsTab(tabs, QStringLiteral("软件自检"), false);
    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    auto *checkButton = new ElaPushButton(QStringLiteral("重新自检"), this);
    auto *historyButton = new ElaPushButton(QStringLiteral("打开历史记录"), this);
    checkButton->setMinimumSize(112, 36);
    historyButton->setMinimumSize(128, 36);
    applyPrimaryButtonStyle(checkButton);
    applySecondaryButtonStyle(historyButton);
    buttonRow->addWidget(checkButton);
    buttonRow->addWidget(historyButton);
    buttonRow->addStretch(1);
    checkTab->addLayout(buttonRow);
    m_selfCheck = settingsReadOnlyText(this);
    checkTab->addWidget(m_selfCheck, 1);
    checkTab->addWidget(settingsSourceBanner(
        QStringLiteral("范围：工控机软件与视觉侧诊断，只读，不修改配置也不向 PLC 下发。"
                       "轴、伺服、机床报警仍由威纶通 HMI 负责。"),
        this));

    QVBoxLayout *logTab = addSettingsTab(tabs, QStringLiteral("事件日志"), false);
    m_eventSnapshot = settingsReadOnlyText(this);
    logTab->addWidget(m_eventSnapshot, 1);
    logTab->addWidget(settingsSourceBanner(
        QStringLiteral("首页「报警与事件日志」的只读快照，切到本页或点「重新自检」时刷新。"),
        this));

    QVBoxLayout *pendingTab = addSettingsTab(tabs, QStringLiteral("报警记录"));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("当前报警列表"),
        QStringLiteral("软件报警队列（工程内无报警表；轴控报警不在本程序）。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("历史报警归档"),
        QStringLiteral("报警历史表。加工历史请用「软件自检」页的「打开历史记录」。"),
        this));
    pendingTab->addStretch(1);

    connect(historyButton, &ElaPushButton::clicked, this, &AlarmSettingsPage::historyRequested);
    connect(checkButton, &ElaPushButton::clicked, this, [this]() { reloadFromSource(); });
    reloadFromSource();
}

QString AlarmSettingsPage::bindingSummary() const
{
    return QStringLiteral("自检读取数据库、海康 ini、节点脚本和首页事件日志。无独立报警表。不接管威纶通轴控报警。");
}

void AlarmSettingsPage::reloadFromSource()
{
    runSelfCheck();
    if (!m_eventSnapshot) {
        return;
    }
    const QString snapshot = m_eventLog ? m_eventLog->snapshotText().trimmed() : QString();
    m_eventSnapshot->setPlainText(snapshot.isEmpty()
                                      ? QStringLiteral("首页事件日志是空的。")
                                      : snapshot);
}

void AlarmSettingsPage::runSelfCheck()
{
    if (!m_selfCheck) {
        return;
    }

    QStringList lines;
    lines << QStringLiteral("自检时间：%1")
                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    QString health;
    if (Database::instance().healthCheck(&health)) {
        lines << QStringLiteral("数据库：已连接，SELECT 1 通过。");
    } else {
        lines << QStringLiteral("数据库：%1").arg(health.isEmpty() ? QStringLiteral("未连接") : health);
    }

    const QString hikPath = HikCameraConfig::defaultFilePath();
    QString hikError;
    const HikCameraConfig hik = HikCameraConfig::load(hikPath, &hikError);
    if (!QFileInfo::exists(hikPath)) {
        lines << QStringLiteral("海康配置：未找到 %1").arg(hikPath);
    } else if (!hikError.isEmpty()) {
        lines << QStringLiteral("海康配置：%1").arg(hikError);
    } else {
        QString validation;
        const bool ready = hik.validate(&validation);
        lines << QStringLiteral("海康配置：已读取 %1。预览校验：%2")
                     .arg(hikPath, ready ? QStringLiteral("可通过") : validation);
    }

    const QString launcher = nodeLauncherIniPath();
    lines << QStringLiteral("node_launcher.ini：%1")
                 .arg(launcher.isEmpty() ? QStringLiteral("未找到") : launcher);
    const QString startScript = resolveNodeLauncherScript(QStringLiteral("StartAll"));
    const QString stopScript = resolveNodeLauncherScript(QStringLiteral("StopAll"));
    lines << QStringLiteral("StartAll：%1").arg(startScript.isEmpty() ? QStringLiteral("不可用") : startScript);
    lines << QStringLiteral("StopAll：%1").arg(stopScript.isEmpty() ? QStringLiteral("不可用") : stopScript);

#ifdef Q_OS_WIN
    if (m_nodes) {
        lines << QStringLiteral("节点进程（copper_node / camera_node / milling_controller）：%1")
                     .arg(m_nodes->nodesRunning() ? QStringLiteral("三个都在") : QStringLiteral("未全部运行"));
    } else {
        lines << QStringLiteral("节点进程：监视器未创建。");
    }
#else
    Q_UNUSED(m_nodes);
    lines << QStringLiteral("节点进程：NodeLifecycleMonitor 只在 Windows 上枚举进程，当前平台无法确认节点是否在跑。");
#endif

    const QString displayPath = displayIniPath();
    lines << QStringLiteral("display.ini：%1")
                 .arg(displayPath.isEmpty() ? QStringLiteral("未找到，三维视图使用内置默认裁切") : displayPath);
    lines << QStringLiteral("相机实时灯和 PLC 灯在首页底部状态栏，本页不复制一套状态。");
    m_selfCheck->setPlainText(lines.join(QStringLiteral("\n")));
}
