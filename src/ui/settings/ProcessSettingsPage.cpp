#include "ui/settings/ProcessSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/UiHelpers.h"
#include "bridge/Ros2Bridge.h"
#include "ui/dialogs/BackendParamsDialog.h"
#include "ui/dialogs/TeachingDialog.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QHBoxLayout>
#include <QTabWidget>
#include <QVBoxLayout>

ProcessSettingsPage::ProcessSettingsPage(Ros2Bridge *bridge, QWidget *parent)
    : SettingsSubPage(parent)
    , m_bridge(bridge)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *tabs = settingsTabs(this);
    root->addWidget(tabs);

    QVBoxLayout *offsetTab = addSettingsTab(tabs, QStringLiteral("坐标补偿与下发"));
    m_params = new BackendParamsDialog(bridge, this);
    auto *offsetRow = new QHBoxLayout();
    offsetRow->setContentsMargins(0, 0, 0, 0);
    offsetRow->addWidget(m_params, 0, Qt::AlignTop);
    offsetRow->addStretch(1);
    offsetTab->addLayout(offsetRow);
    offsetTab->addWidget(settingsHint(
        QStringLiteral("技术员及以上可修改。卡片内「应用」与页底「保存」相同，都会校验权限并写入操作审计。"),
        this));
    offsetTab->addStretch(1);
    offsetTab->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：%1 的 [BackendParams]。保存时写入该文件，并经 ROS 发布 "
                       "plc/coordinate_offset、plc/dispatch_paths_to_plc。")
            .arg(backendParamsIniPath()),
        this));

    QVBoxLayout *teachTab = addSettingsTab(tabs, QStringLiteral("示教取坐标"));
    m_teaching = new TeachingDialog(bridge, this);
    auto *teachRow = new QHBoxLayout();
    teachRow->setContentsMargins(0, 0, 0, 0);
    teachRow->addWidget(m_teaching, 0, Qt::AlignTop);
    teachRow->addStretch(1);
    teachTab->addLayout(teachRow);
    teachTab->addWidget(settingsHint(QStringLiteral("技术员及以上可执行示教。"), this));
    teachTab->addStretch(1);
    teachTab->addWidget(settingsSourceBanner(
        QStringLiteral("示教沿用现有示教模块，只发送 publishTeachRequest，不改任何配置文件。"),
        this));

    QVBoxLayout *pendingTab = addSettingsTab(tabs, QStringLiteral("工艺参数"));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("进给"),
        QStringLiteral("工艺点表 / PLC 可写字段（backend_params.ini 无进给）。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("深度"),
        QStringLiteral("工艺点表 / PLC 可写字段（backend_params.ini 无深度）。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("转速"),
        QStringLiteral("工艺点表 / PLC 可写字段（backend_params.ini 无转速）。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("板级策略"),
        QStringLiteral("工艺点表（现网无板级策略配置键）。"),
        this));
    pendingTab->addStretch(1);

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
    return QStringLiteral("绑定 backend_params.ini + ROS 坐标补偿 / 路径下发。进给、深度、转速、板级策略待对接。");
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
