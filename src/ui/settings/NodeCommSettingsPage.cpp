#include "ui/settings/NodeCommSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "data/Database.h"
#include "ui/settings/SettingsFormWidgets.h"

#include "ElaPushButton.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSettings>
#include <QHash>
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

struct NodeReadiness
{
    bool ready{false};
    QStringList missing;
};

QString expandBatchVariable(const QString &value, const QHash<QString, QString> &variables)
{
    QString expanded = value.trimmed();
    for (int pass = 0; pass < 6; ++pass) {
        QString next;
        int cursor = 0;
        bool changed = false;
        const QRegularExpression expression(QStringLiteral("%([A-Za-z0-9_]+)%"));
        QRegularExpressionMatchIterator iterator = expression.globalMatch(expanded);
        while (iterator.hasNext()) {
            const QRegularExpressionMatch match = iterator.next();
            next += expanded.mid(cursor, match.capturedStart() - cursor);
            const QString key = match.captured(1).toUpper();
            if (variables.contains(key)) {
                next += variables.value(key);
                changed = true;
            } else {
                next += match.captured(0);
            }
            cursor = match.capturedEnd();
        }
        next += expanded.mid(cursor);
        expanded = next;
        if (!changed) {
            break;
        }
    }
    return QDir::cleanPath(expanded.remove('"').trimmed());
}

QHash<QString, QString> readBatchVariables(const QString &script, QStringList *warnings)
{
    QHash<QString, QString> variables;
    const QRegularExpression setExpression(
        QStringLiteral("^\\s*set\\s+\\\"([A-Za-z0-9_]+)=(.*)\\\"\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    const QStringList lines = script.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = setExpression.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        variables.insert(match.captured(1).toUpper(), match.captured(2).trimmed());
    }
    for (auto iterator = variables.begin(); iterator != variables.end(); ++iterator) {
        const QString before = iterator.value();
        iterator.value() = expandBatchVariable(before, variables);
        if (iterator.value() == before && before.contains('%')) {
            warnings->append(QStringLiteral("变量 %1 未能完全展开：%2")
                                 .arg(iterator.key(), before));
        }
    }
    return variables;
}

NodeReadiness checkBatchFiles(const QString &script,
                              const QHash<QString, QString> &variables,
                              const QStringList &requiredVariables)
{
    NodeReadiness result;
    if (script.isEmpty()) {
        result.missing << QStringLiteral("未读取启动脚本");
        return result;
    }
    const QRegularExpression guardExpression(
        QStringLiteral("if\\s+not\\s+exist\\s+\\\"([^\\\"]+)\\\""),
        QRegularExpression::CaseInsensitiveOption);
    QStringList conditions;
    QRegularExpressionMatchIterator guardIterator = guardExpression.globalMatch(script);
    while (guardIterator.hasNext()) {
        conditions << guardIterator.next().captured(1);
    }
    for (const QString &variable : requiredVariables) {
        const QString key = variable.toUpper();
        if (!variables.contains(key)) {
            result.missing << QStringLiteral("脚本缺少变量 %1").arg(variable);
            continue;
        }
        QString condition;
        const QString reference = QStringLiteral("%%%1%%").arg(variable);
        for (const QString &candidate : conditions) {
            if (candidate.contains(reference, Qt::CaseInsensitive)) {
                condition = candidate;
                break;
            }
        }
        if (condition.isEmpty()) {
            result.missing << QStringLiteral("脚本未校验 %1").arg(variable);
            continue;
        }
        const QString path = expandBatchVariable(condition, variables);
        if (path.isEmpty() || path.contains('%')) {
            result.missing << QStringLiteral("%1 检查路径未能展开").arg(variable);
            continue;
        }
        if (!QFileInfo::exists(path)) {
            result.missing << QStringLiteral("文件不存在：%1").arg(QDir::toNativeSeparators(path));
        }
    }
    result.ready = result.missing.isEmpty();
    return result;
}

QFrame *makeReadinessCard(const QString &title,
                          QLabel **status,
                          QLabel **detail,
                          QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("nodeReadinessCard"));
    Theme::bindStyle(card, []() {
        return QStringLiteral("QFrame#nodeReadinessCard { background:%1; border:1px solid %2;"
                              " border-radius:6px; }")
            .arg(Theme::kControlBg, Theme::kBorder);
    });
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(3);
    auto *name = settingsSectionHeading(title, card);
    layout->addWidget(name);
    *status = makeLabel(QStringLiteral("等待检测"));
    (*status)->setParent(card);
    (*status)->setWordWrap(true);
    layout->addWidget(*status);
    *detail = settingsHint(QStringLiteral("等待启动前置检查"), card);
    (*detail)->setWordWrap(false);
    layout->addWidget(*detail);
    return card;
}

void setReadinessLabel(QLabel *label, bool ready, const QString &detail)
{
    if (!label) {
        return;
    }
    const int issueCount = detail.count(QStringLiteral("；")) + (detail.isEmpty() ? 0 : 1);
    label->setText(ready ? QStringLiteral("● 准备就绪")
                         : QStringLiteral("● 未就绪 · %1 项需处理").arg(issueCount));
    label->setToolTip(ready ? QStringLiteral("启动脚本中的前置检查均已通过。") : detail);
    label->setWordWrap(false);
    label->setStyleSheet(QStringLiteral(
                             "color:%1;font-size:12px;font-weight:600;border:none;background:transparent;")
                             .arg(ready ? Theme::kStateSuccess : Theme::kStateError));
}

QString readinessIssueSummary(const NodeReadiness &result)
{
    QStringList names;
    for (const QString &issue : result.missing) {
        QString name = issue;
        const QString pathPrefix = QStringLiteral("文件不存在：");
        if (name.startsWith(pathPrefix)) {
            name = QFileInfo(name.mid(pathPrefix.size())).fileName();
        } else {
            name.remove(QStringLiteral("脚本未校验 "));
            name.remove(QStringLiteral("脚本缺少变量 "));
            name.remove(QStringLiteral(" 检查路径未能展开"));
        }
        if (!name.isEmpty() && !names.contains(name)) {
            names.append(name);
        }
    }
    if (names.isEmpty()) {
        return QStringLiteral("等待检测结果");
    }
    QStringList visible = names.mid(0, 2);
    if (names.size() > visible.size()) {
        visible << QStringLiteral("另外 %1 项").arg(names.size() - visible.size());
    }
    return QStringLiteral("缺少：%1").arg(visible.join(QStringLiteral("、")));
}

void setReadinessDetail(QLabel *label,
                        bool ready,
                        const QString &summary,
                        const QString &fullDetail)
{
    if (!label) {
        return;
    }
    label->setText(ready ? QStringLiteral("启动条件已全部通过") : summary);
    label->setToolTip(ready ? QStringLiteral("启动脚本中的前置检查均已通过。") : fullDetail);
    label->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                             .arg(Theme::kTextMuted));
}

void styleReadinessCard(QFrame *card, bool ready)
{
    if (!card) {
        return;
    }
    card->setStyleSheet(QStringLiteral(
                            "QFrame#nodeReadinessCard { background:%1; border:1px solid %2;"
                            " border-top:3px solid %3; border-radius:6px; }")
                            .arg(Theme::kPanelBg,
                                 Theme::kBorder,
                                 ready ? Theme::kStateSuccess : Theme::kStateError));
}

} // namespace

NodeCommSettingsPage::NodeCommSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    auto *servicePanel = settingsGroupPanel(QStringLiteral("节点与服务"),
                                             QStringLiteral(":/img/nav_settings.svg"), this);
    root->addWidget(servicePanel, 1);

    // 节点启停：原先在顶栏，按设计方案移到设置页，顶栏只保留设备在线状态。
    m_nodeState = makeLabel(QString());
    m_nodeState->setStyleSheet(QStringLiteral("color:%1;font-size:13px;border:none;background:transparent;")
                                   .arg(Theme::kTextSecondary));
    auto *control = new QWidget(servicePanel);
    auto *controlLayout = new QVBoxLayout(control);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(8);
    auto *controlHeading = new QHBoxLayout();
    controlHeading->addWidget(settingsSectionHeading(QStringLiteral("服务运行状态"), control));
    controlHeading->addStretch(1);
    controlHeading->addWidget(m_nodeState);
    controlLayout->addLayout(controlHeading);
    auto *controlRow = new QHBoxLayout();
    controlRow->setContentsMargins(0, 0, 0, 0);
    controlRow->setSpacing(10);
    m_startButton = new ElaPushButton(QStringLiteral("启动节点"), control);
    m_startButton->setFixedSize(96, 30);
    applyPrimaryButtonStyle(m_startButton);
    m_stopButton = new ElaPushButton(QStringLiteral("暂停节点"), control);
    m_stopButton->setFixedSize(96, 30);
    applySecondaryButtonStyle(m_stopButton);
    controlRow->addWidget(m_startButton);
    controlRow->addWidget(m_stopButton);
    controlRow->addStretch(1);
    controlLayout->addLayout(controlRow);
    auto *readinessHeading = new QHBoxLayout();
    readinessHeading->setContentsMargins(0, 8, 0, 0);
    readinessHeading->addWidget(settingsSectionHeading(QStringLiteral("启动前置检查"), control));
    readinessHeading->addStretch(1);
    m_checkButton = new ElaPushButton(QStringLiteral("重新检测"), control);
    m_checkButton->setFixedSize(96, 28);
    applySecondaryButtonStyle(m_checkButton);
    readinessHeading->addWidget(m_checkButton);
    controlLayout->addLayout(readinessHeading);
    auto *readinessGrid = new QGridLayout();
    readinessGrid->setContentsMargins(0, 0, 0, 0);
    readinessGrid->setHorizontalSpacing(8);
    readinessGrid->setVerticalSpacing(8);
    m_cameraCard = makeReadinessCard(QStringLiteral("相机服务节点"),
                                     &m_cameraReadiness,
                                     &m_cameraDetail,
                                     control);
    m_backendCard = makeReadinessCard(QStringLiteral("后端服务节点"),
                                      &m_backendReadiness,
                                      &m_backendDetail,
                                      control);
    m_algorithmCard = makeReadinessCard(QStringLiteral("算法服务节点"),
                                         &m_algorithmReadiness,
                                         &m_algorithmDetail,
                                         control);
    readinessGrid->addWidget(m_cameraCard, 0, 0);
    readinessGrid->addWidget(m_backendCard, 0, 1);
    readinessGrid->addWidget(m_algorithmCard, 0, 2);
    readinessGrid->setColumnStretch(0, 1);
    readinessGrid->setColumnStretch(1, 1);
    readinessGrid->setColumnStretch(2, 1);
    controlLayout->addLayout(readinessGrid);
    m_readinessSummary = settingsHint(QString(), control);
    m_readinessSummary->setWordWrap(true);
    controlLayout->addWidget(m_readinessSummary);
    m_readinessDetails = settingsHint(QString(), control);
    m_readinessDetails->setWordWrap(true);
    controlLayout->addWidget(m_readinessDetails);
    qobject_cast<QVBoxLayout *>(servicePanel->layout())->addWidget(control);
    connect(m_startButton, &ElaPushButton::clicked, this, &NodeCommSettingsPage::startNodesRequested);
    connect(m_stopButton, &ElaPushButton::clicked, this, &NodeCommSettingsPage::stopNodesRequested);
    connect(m_checkButton, &ElaPushButton::clicked, this, &NodeCommSettingsPage::checkNodeReadiness);
    updateNodeControls();

    qobject_cast<QVBoxLayout *>(servicePanel->layout())->addStretch(1);

    auto *storagePanel = settingsGroupPanel(QStringLiteral("数据库与存储"),
                                             QStringLiteral(":/img/kpi_check.svg"), this);
    root->addWidget(storagePanel, 1);
    auto *storageLayout = qobject_cast<QVBoxLayout *>(storagePanel->layout());
    m_databaseSummary = settingsReadOnlyText(storagePanel);
    storageLayout->addWidget(m_databaseSummary, 1);
    storageLayout->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：程序目录下的 database.ini。密码类键以掩码显示。"),
        storagePanel));

    auto *commPanel = settingsGroupPanel(QStringLiteral("设备通信"),
                                          QStringLiteral(":/img/nav_detect.svg"), this);
    root->addWidget(commPanel, 1);
    auto *commLayout = qobject_cast<QVBoxLayout *>(commPanel->layout());
    commLayout->addWidget(settingsPendingBlock(
        QStringLiteral("PLC / UDP 地址与超时"),
        QStringLiteral("PLC IP 等仍写在 start_all.bat 内，node_launcher.ini 没有这些键。"),
        commPanel));
    commLayout->addWidget(settingsPendingBlock(
        QStringLiteral("单个节点启停"),
        QStringLiteral("node_launcher.ini 现只有 StartAll / StopAll。"),
        commPanel));
    commLayout->addWidget(settingsPendingBlock(
        QStringLiteral("协议与超时"),
        QStringLiteral("PLC / UDP 协议、心跳周期和重连策略待后端接口确认。"),
        commPanel));
    commLayout->addWidget(settingsHint(
        QStringLiteral("设备地址、协议和超时参数在接口定义前不提供可编辑输入，避免保存无效配置。"),
        commPanel));
    commLayout->addStretch(1);
    reloadFromSource();
    Theme::watch(this, [this]() {
        updateNodeControls();
        checkNodeReadiness();
    });
}

QString NodeCommSettingsPage::bindingSummary() const
{
    return QStringLiteral("节点启停、SQLite 存储摘要；PLC/UDP 地址与协议待后端确认。");
}

void NodeCommSettingsPage::setNodesRunning(bool running)
{
    m_running = running;
    updateNodeControls();
}

void NodeCommSettingsPage::setNodesBusy(bool busy)
{
    m_busy = busy;
    updateNodeControls();
}

void NodeCommSettingsPage::updateNodeControls()
{
    if (m_nodeState) {
        QString text;
        QString color;
        if (m_busy) {
            text = QStringLiteral("● 脚本执行中…");
            color = Theme::kStateWarning;
        } else if (m_running) {
            text = QStringLiteral("● 节点运行中");
            color = Theme::kStateSuccess;
        } else {
            text = QStringLiteral("● 节点未运行");
            color = Theme::kStateError;
        }
        m_nodeState->setText(text);
        m_nodeState->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:600;border:none;background:transparent;")
                                       .arg(color));
    }
    if (m_startButton) {
        m_startButton->setEnabled(!m_busy && !m_running && m_allReady);
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(!m_busy && m_running);
    }
}

void NodeCommSettingsPage::checkNodeReadiness()
{
    const QString scriptPath = resolveNodeLauncherScript(QStringLiteral("StartAll"));
    QString script;
    if (!scriptPath.isEmpty()) {
        QFile file(scriptPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            script = QString::fromLocal8Bit(file.readAll());
        }
    }

    QStringList variableWarnings;
    const QHash<QString, QString> variables = readBatchVariables(script, &variableWarnings);
    const NodeReadiness camera = checkBatchFiles(
        script, variables, {QStringLiteral("CAMERA_EXE"),
                            QStringLiteral("CAMERA_IFACES"),
                            QStringLiteral("CAMERA_SENSOR"),
                            QStringLiteral("CAMERA_CONFIG"),
                            QStringLiteral("CAMERA_SICK_CSV"),
                            QStringLiteral("ROS_LOCAL_SETUP")});
    const NodeReadiness backend = checkBatchFiles(
        script, variables, {QStringLiteral("BACKEND_EXE"),
                            QStringLiteral("BACKEND_LOCAL_SETUP"),
                            QStringLiteral("ROS_LOCAL_SETUP")});
    const NodeReadiness algorithm = checkBatchFiles(
        script, variables, {QStringLiteral("ALGO_EXE")});

    const auto joinMissing = [](const NodeReadiness &result) {
        return result.missing.join(QStringLiteral("；"));
    };
    setReadinessLabel(m_cameraReadiness, camera.ready, joinMissing(camera));
    setReadinessLabel(m_backendReadiness, backend.ready, joinMissing(backend));
    setReadinessLabel(m_algorithmReadiness, algorithm.ready, joinMissing(algorithm));
    setReadinessDetail(m_cameraDetail,
                       camera.ready,
                       readinessIssueSummary(camera),
                       joinMissing(camera));
    setReadinessDetail(m_backendDetail,
                       backend.ready,
                       readinessIssueSummary(backend),
                       joinMissing(backend));
    setReadinessDetail(m_algorithmDetail,
                       algorithm.ready,
                       readinessIssueSummary(algorithm),
                       joinMissing(algorithm));
    styleReadinessCard(m_cameraCard, camera.ready);
    styleReadinessCard(m_backendCard, backend.ready);
    styleReadinessCard(m_algorithmCard, algorithm.ready);

    const int readyCount = int(camera.ready) + int(backend.ready) + int(algorithm.ready);
    const bool allReady = readyCount == 3;
    m_allReady = allReady;
    if (m_readinessSummary) {
        m_readinessSummary->setText(allReady
                                        ? QStringLiteral("● 3 / 3 个服务已准备就绪，可以启动节点。")
                                        : QStringLiteral("● %1 / 3 个服务已准备就绪，请先处理未就绪项。")
                                              .arg(readyCount));
        m_readinessSummary->setStyleSheet(
            QStringLiteral("color:%1;font-size:12px;font-weight:600;border:none;background:transparent;")
                .arg(allReady ? Theme::kStateSuccess : Theme::kStateWarning));
    }
    if (m_readinessDetails) {
        QString detail = scriptPath.isEmpty()
                             ? QStringLiteral("启动脚本：未找到")
                             : QStringLiteral("启动脚本：%1").arg(QDir::toNativeSeparators(scriptPath));
        if (!variableWarnings.isEmpty()) {
            detail += QStringLiteral("；变量提示：%1").arg(variableWarnings.join(QStringLiteral("；")));
        }
        m_readinessDetails->setText(detail);
    }
    if (m_startButton) {
        m_startButton->setEnabled(!m_busy && !m_running && allReady);
    }
}

void NodeCommSettingsPage::reloadFromSource()
{
    checkNodeReadiness();

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
