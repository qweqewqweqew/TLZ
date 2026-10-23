#include "ui/settings/ProcessSettingsPage.h"

#include "ElaPushButton.h"
#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "bridge/Ros2Bridge.h"
#include "ui/dialogs/BackendParamsDialog.h"
#include "ui/dialogs/TeachingDialog.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

void addPendingInput(QVBoxLayout *layout,
                     const QString &name,
                     const QString &unit,
                     QWidget *parent)
{
    auto *row = new QWidget(parent);
    row->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(8);
    auto *label = settingsFieldLabel(name, row);
    label->setFixedWidth(112);
    rowLayout->addWidget(label);

    auto *field = settingsLineEdit(row);
    field->setEnabled(false);
    rowLayout->addWidget(field, 1);
    if (!unit.isEmpty()) {
        auto *unitLabel = makeLabel(unit);
        unitLabel->setFixedWidth(48);
        Theme::bindStyle(unitLabel, []() {
            return QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                .arg(Theme::kTextMuted);
        });
        rowLayout->addWidget(unitLabel);
    }
    layout->addWidget(row);
}

void addPendingChoice(QVBoxLayout *layout,
                      const QString &name,
                      const QStringList &choices,
                      QWidget *parent)
{
    auto *row = new QWidget(parent);
    row->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(8);
    auto *label = settingsFieldLabel(name, row);
    label->setFixedWidth(112);
    rowLayout->addWidget(label);
    auto *combo = new QComboBox(row);
    combo->addItems(choices);
    combo->setEnabled(false);
    combo->setMinimumHeight(32);
    Theme::bindStyle(combo, []() {
        return QStringLiteral("QComboBox { color:%1;background:%2;border:1px solid %3;"
                              "border-radius:5px;padding:4px 9px;font-size:12px; }"
                              "QComboBox::drop-down { border:none;width:22px; }")
            .arg(Theme::kTextMuted, Theme::kPanelBg, Theme::kBorder);
    });
    rowLayout->addWidget(combo, 1);
    layout->addWidget(row);
}

} // namespace

ProcessSettingsPage::ProcessSettingsPage(Ros2Bridge *bridge, QWidget *parent)
    : SettingsSubPage(parent)
    , m_bridge(bridge)
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    auto *coordinatePanel = settingsGroupPanel(QStringLiteral("坐标补偿"),
                                                QStringLiteral(":/img/kpi_target.svg"), this);
    grid->addWidget(coordinatePanel, 0, 0);
    auto *coordinateLayout = qobject_cast<QVBoxLayout *>(coordinatePanel->layout());
    m_params = new BackendParamsDialog(bridge, coordinatePanel);
    coordinateLayout->addWidget(m_params);
    auto *coordinateHint = settingsHint(QStringLiteral("保存到 backend_params.ini · 发布至 plc/coordinate_offset"),
                                        coordinatePanel);
    coordinateHint->setWordWrap(false);
    coordinateLayout->addWidget(coordinateHint);
    coordinateLayout->addStretch(1);

    auto *teachingPanel = settingsGroupPanel(QStringLiteral("现场示教"),
                                              QStringLiteral(":/img/nav_detect.svg"), this);
    grid->addWidget(teachingPanel, 0, 1);
    auto *teachingLayout = qobject_cast<QVBoxLayout *>(teachingPanel->layout());
    m_teaching = new TeachingDialog(bridge, teachingPanel);
    teachingLayout->addWidget(m_teaching);
    teachingLayout->addStretch(1);

    auto *processPanel = settingsGroupPanel(QStringLiteral("修磨参数"),
                                             QStringLiteral(":/img/nav_settings.svg"), this);
    grid->addWidget(processPanel, 1, 0);
    auto *processLayout = qobject_cast<QVBoxLayout *>(processPanel->layout());
    addPendingInput(processLayout, QStringLiteral("目标残余高度"), QStringLiteral("mm"), processPanel);
    addPendingInput(processLayout, QStringLiteral("单次切深"), QStringLiteral("mm"), processPanel);
    addPendingInput(processLayout, QStringLiteral("进给速度"), QStringLiteral("mm/min"), processPanel);
    addPendingInput(processLayout, QStringLiteral("主轴转速"), QStringLiteral("r/min"), processPanel);
    processLayout->addStretch(1);

    auto *strategyPanel = settingsGroupPanel(QStringLiteral("复检策略"),
                                              QStringLiteral(":/img/kpi_check.svg"), this);
    grid->addWidget(strategyPanel, 1, 1);
    auto *strategyLayout = qobject_cast<QVBoxLayout *>(strategyPanel->layout());
    addPendingChoice(strategyLayout, QStringLiteral("修磨等级"), QStringList(), strategyPanel);
    addPendingChoice(strategyLayout, QStringLiteral("复检条件"), QStringList(), strategyPanel);
    addPendingChoice(strategyLayout, QStringLiteral("未达标处理"), QStringList(), strategyPanel);
    strategyLayout->addStretch(1);

    connect(m_params, &BackendParamsDialog::dirtyChanged, this, &ProcessSettingsPage::dirtyChanged);
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
    return QStringLiteral("坐标补偿与现场示教已接入。");
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
                error.isEmpty() ? QStringLiteral("坐标补偿未写入。") : error};
    }

    const QString path = backendParamsIniPath();
    if (!m_bridge || !m_bridge->isRunning()) {
        return {SettingsSaveStatus::SavedWithWarning,
                QStringLiteral("已写入 %1。ROS 节点未运行，坐标补偿没有发到后端。")
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
