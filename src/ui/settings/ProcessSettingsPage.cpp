#include "ui/settings/ProcessSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/UiHelpers.h"
#include "bridge/Ros2Bridge.h"
#include "ui/dialogs/BackendParamsDialog.h"
#include "ui/dialogs/TeachingDialog.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

ProcessSettingsPage::ProcessSettingsPage(Ros2Bridge *bridge, QWidget *parent)
    : SettingsSubPage(parent)
    , m_bridge(bridge)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    auto *coordinatePanel = settingsGroupPanel(QStringLiteral("坐标与示教"),
                                                QStringLiteral(":/img/kpi_target.svg"), this);
    root->addWidget(coordinatePanel, 1);
    auto *coordinateLayout = qobject_cast<QVBoxLayout *>(coordinatePanel->layout());
    m_params = new BackendParamsDialog(bridge, this);
    m_params->setParent(coordinatePanel);
    auto *offsetRow = new QHBoxLayout();
    offsetRow->setContentsMargins(0, 0, 0, 0);
    offsetRow->addWidget(m_params, 0, Qt::AlignTop);
    offsetRow->addStretch(1);
    coordinateLayout->addLayout(offsetRow);
    coordinateLayout->addWidget(settingsHint(
        QStringLiteral("技术员及以上可修改。卡片内「应用」与页底「保存」相同，都会校验权限并写入操作审计。"),
        coordinatePanel));
    auto *teachPanel = createPanel(QStringLiteral("示教取坐标"), nullptr, coordinatePanel);
    auto *teachPanelLayout = qobject_cast<QVBoxLayout *>(teachPanel->layout());
    m_teaching = new TeachingDialog(bridge, this);
    auto *teachRow = new QHBoxLayout();
    teachRow->setContentsMargins(0, 0, 0, 0);
    teachRow->addWidget(m_teaching, 0, Qt::AlignTop);
    teachRow->addStretch(1);
    teachPanelLayout->addLayout(teachRow);
    coordinateLayout->addWidget(teachPanel);
    coordinateLayout->addWidget(settingsHint(QStringLiteral("技术员及以上可修改补偿并执行示教。"), coordinatePanel));
    coordinateLayout->addStretch(1);
    coordinateLayout->addWidget(settingsSourceBanner(
        QStringLiteral("坐标参数来源：%1 的 [BackendParams]；下发 ROS 坐标补偿 / 路径请求。"
                       "示教只发送 publishTeachRequest，不改配置文件。")
            .arg(backendParamsIniPath()),
        coordinatePanel));

    auto *processPanel = settingsGroupPanel(QStringLiteral("修磨参数"),
                                             QStringLiteral(":/img/nav_settings.svg"), this);
    root->addWidget(processPanel, 1);
    auto *processLayout = qobject_cast<QVBoxLayout *>(processPanel->layout());
    processLayout->addWidget(settingsPendingBlock(
        QStringLiteral("目标残余高度"),
        QStringLiteral("修磨后允收高度及单位需由现场工艺确认。"),
        processPanel));
    processLayout->addWidget(settingsPendingBlock(
        QStringLiteral("单次切深"),
        QStringLiteral("最大切深、分层规则及安全余量待机床能力确认。"),
        processPanel));
    processLayout->addWidget(settingsPendingBlock(
        QStringLiteral("进给"),
        QStringLiteral("工艺点表 / PLC 可写字段（backend_params.ini 无进给）。"),
        processPanel));
    processLayout->addWidget(settingsPendingBlock(
        QStringLiteral("转速"),
        QStringLiteral("工艺点表 / PLC 可写字段（backend_params.ini 无转速）。"),
        processPanel));
    processLayout->addWidget(settingsHint(
        QStringLiteral("后端参数接口补齐并经现场确认后，再开放这些参数的编辑和保存。"), processPanel));
    processLayout->addStretch(1);

    auto *strategyPanel = settingsGroupPanel(QStringLiteral("策略与复检"),
                                              QStringLiteral(":/img/kpi_check.svg"), this);
    root->addWidget(strategyPanel, 1);
    auto *strategyLayout = qobject_cast<QVBoxLayout *>(strategyPanel->layout());
    strategyLayout->addWidget(settingsPendingBlock(
        QStringLiteral("分级修磨"),
        QStringLiteral("标准修磨、重点分层修磨及人工复核规则待工艺确认。"),
        strategyPanel));
    strategyLayout->addWidget(settingsPendingBlock(
        QStringLiteral("自动复检"),
        QStringLiteral("复检触发、合格判定和未达标返修流程待后端接入。"),
        strategyPanel));
    strategyLayout->addWidget(settingsPendingBlock(
        QStringLiteral("安全策略"),
        QStringLiteral("抬刀、避障、加工顺序与急停恢复由机床控制方案定义。"),
        strategyPanel));
    strategyLayout->addStretch(1);

    connect(m_params, &BackendParamsDialog::dirtyChanged, this, &ProcessSettingsPage::dirtyChanged);
    connect(m_params, &BackendParamsDialog::applyRequested, this, &ProcessSettingsPage::saveRequested);
    reloadFromSource();
}

void ProcessSettingsPage::setWriteAllowed(bool allowed)
{
    if (m_params) {
        m_params->setEnabled(allowed);
    }
    if (m_teaching) {
        m_teaching->setEnabled(allowed);
    }
}

QString ProcessSettingsPage::pendingChangeSummary() const
{
    if (!m_params) {
        return QString();
    }
    const QString changes = m_params->pendingChangeSummary();
    return changes.isEmpty() ? QStringLiteral("参数未变，重新下发到后端") : changes;
}

QString ProcessSettingsPage::bindingSummary() const
{
    return QStringLiteral("绑定 backend_params.ini 与 ROS 坐标补偿 / 路径下发；工艺参数和复检待确认。");
}

bool ProcessSettingsPage::isDirty() const
{
    return m_params && m_params->isDirty();
}

SettingsSaveOutcome ProcessSettingsPage::save()
{
    QString error;
    if (!m_params || !m_params->commitToBackend(&error)) {
        return {SettingsSaveStatus::Rejected,
                error.isEmpty() ? QStringLiteral("加工参数未写入。") : error};
    }

    const QString path = backendParamsIniPath();
    if (!m_bridge || !m_bridge->isRunning()) {
        return {SettingsSaveStatus::SavedWithWarning,
                QStringLiteral("已写入 %1。ROS 节点未运行，坐标补偿与「下发路径到 PLC」没有发到后端。")
                    .arg(path)};
    }
    return {SettingsSaveStatus::Saved,
            QStringLiteral("已写入 %1，并已请求下发 plc/coordinate_offset 与 plc/dispatch_paths_to_plc。")
                .arg(path)};
}

void ProcessSettingsPage::revert()
{
    reloadFromSource();
}

void ProcessSettingsPage::restoreDefaults()
{
    if (m_params) {
        m_params->applyFactoryDefaults();
    }
}

void ProcessSettingsPage::reloadFromSource()
{
    if (m_params) {
        m_params->reloadFromDisk();
    }
}
