#include "ui/settings/AlarmSettingsPage.h"

#include "ElaPushButton.h"
#include "app/RuntimeIniPaths.h"
#include "app/UiHelpers.h"
#include "bridge/NodeLifecycleMonitor.h"
#include "camera/HikCameraConfig.h"
#include "data/Database.h"
#include "ui/panels/EventLogPanel.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QPlainTextEdit>
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
    root->setSpacing(12);
    root->addWidget(settingsSourceBanner(
        QStringLiteral("范围：工控机软件 / 视觉侧诊断。轴、伺服、机床报警仍由威纶通 HMI 负责，本页不替换、不汇总轴控报警。"),
        this));

    auto *actions = createPanel(QStringLiteral("入口"), nullptr, this);
    auto *actionsLayout = qobject_cast<QVBoxLayout *>(actions->layout());
    auto *buttonRow = new QHBoxLayout();
    auto *historyButton = new ElaPushButton(QStringLiteral("打开历史记录"), actions);
    auto *checkButton = new ElaPushButton(QStringLiteral("重新自检"), actions);
    historyButton->setMinimumHeight(36);
    checkButton->setMinimumHeight(36);
    applyPrimaryButtonStyle(historyButton);
    applySecondaryButtonStyle(checkButton);
    buttonRow->addWidget(historyButton, 0, Qt::AlignLeft);
    buttonRow->addWidget(checkButton, 0, Qt::AlignLeft);
    buttonRow->addStretch(1);
    actionsLayout->addLayout(buttonRow);
    actionsLayout->addWidget(settingsHint(
        QStringLiteral("历史记录沿用现有历史页，不另建报警档案。"),
        actions));
    root->addWidget(actions);

    auto *checkPanel = createPanel(QStringLiteral("软件 / 节点自检"), nullptr, this);
    auto *checkLayout = qobject_cast<QVBoxLayout *>(checkPanel->layout());
    m_selfCheck = settingsReadOnlyText(checkPanel);
    checkLayout->addWidget(m_selfCheck);
    root->addWidget(checkPanel);

    auto *logPanel = createPanel(QStringLiteral("首页事件日志快照"), nullptr, this);
    auto *logLayout = qobject_cast<QVBoxLayout *>(logPanel->layout());
    logLayout->addWidget(settingsHint(
        QStringLiteral("这是首页「报警与事件日志」的只读拷贝，不是独立报警表。"),
        logPanel));
    m_eventSnapshot = settingsReadOnlyText(logPanel);
    logLayout->addWidget(m_eventSnapshot);
    root->addWidget(logPanel);

    auto *pending = createPanel(QStringLiteral("当前报警"), nullptr, this);
    auto *pendingLayout = qobject_cast<QVBoxLayout *>(pending->layout());
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("当前报警列表"),
        QStringLiteral("待对接 软件报警队列（工程内无报警表；轴控报警不在本程序）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("历史报警归档"),
        QStringLiteral("待对接 报警历史表。加工历史请用上方「打开历史记录」。"),
        pending));
    root->addWidget(pending);
    root->addStretch(1);

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
    lines << QStringLiteral("自检只读，不修改配置，也不向 PLC 下发。");

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
