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
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    root->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：%1 的 [BackendParams]（OffsetX / OffsetY / OffsetZ / "
                       "DispatchPathsToPlc）。保存时写入该文件，并经 Ros2Bridge 发布 "
                       "plc/coordinate_offset、plc/dispatch_paths_to_plc。"
                       "示教沿用现有 TeachingDialog，只调用 publishTeachRequest。")
            .arg(backendParamsIniPath()),
        this));

    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);
    m_params = new BackendParamsDialog(bridge, this);
    row->addWidget(m_params, 0, Qt::AlignTop);
    row->addWidget(new TeachingDialog(bridge, this), 0, Qt::AlignTop);
    row->addStretch(1);
    root->addLayout(row);

    root->addWidget(settingsHint(
        QStringLiteral("计划权限：技术员及以上可改偏移与路径下发。当前没有登录钩子，此项不锁定。"
                       "卡片内「应用」与页底「保存」相同。"),
        this));

    auto *pending = createPanel(QStringLiteral("未对接工艺项"), nullptr, this);
    auto *pendingLayout = qobject_cast<QVBoxLayout *>(pending->layout());
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("进给"),
        QStringLiteral("待对接 工艺点表 / PLC 可写字段（backend_params.ini 无进给）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("深度"),
        QStringLiteral("待对接 工艺点表 / PLC 可写字段（backend_params.ini 无深度）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("转速"),
        QStringLiteral("待对接 工艺点表 / PLC 可写字段（backend_params.ini 无转速）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("板级策略"),
        QStringLiteral("待对接 工艺点表（现网无板级策略配置键）。"),
        pending));
    root->addWidget(pending);
    root->addStretch(1);

    connect(m_params, &BackendParamsDialog::dirtyChanged, this, &ProcessSettingsPage::dirtyChanged);
    reloadFromSource();
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
