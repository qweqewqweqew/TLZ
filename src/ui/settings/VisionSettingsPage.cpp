#include "ui/settings/VisionSettingsPage.h"

#include "app/Logger.h"
#include "app/UiHelpers.h"
#include "camera/HikCameraConfig.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

HikCameraConfig factoryConfig()
{
    return HikCameraConfig{};
}

bool sameConfig(const HikCameraConfig &a, const HikCameraConfig &b)
{
    return a.enabled == b.enabled
           && a.address == b.address
           && a.port == b.port
           && a.username == b.username
           && a.password == b.password
           && a.channel == b.channel
           && a.streamType == b.streamType
           && a.linkMode == b.linkMode
           && a.loginTimeoutMs == b.loginTimeoutMs
           && a.reconnectIntervalMs == b.reconnectIntervalMs;
}

} // namespace

VisionSettingsPage::VisionSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);
    root->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：%1 的 [HikCamera]，字段与 HikCameraConfig::load 一致。"
                       "保存写回该文件。HikCameraClient 在每次 startPreview 时重新读取，"
                       "已打开的预览不会热更新。曝光、增益、触发方式、标定不在该 ini 中。"
                       "独立 POE 视觉网段只作说明，本页不改路由。")
            .arg(HikCameraConfig::defaultFilePath()),
        this));
    root->addWidget(settingsHint(
        QStringLiteral("计划权限：管理员可写相机登录参数。当前没有登录钩子，此项不锁定。"),
        this));

    auto *panel = createPanel(QStringLiteral("海康连接"), nullptr, this);
    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    auto *formHost = new QWidget(panel);
    auto *form = new QFormLayout(formHost);
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    m_enabled = settingsCheckBox(QStringLiteral("启用海康预览（Enabled）"), formHost);
    m_address = settingsLineEdit(formHost);
    m_port = settingsSpinBox(formHost);
    m_port->setRange(1, 65535);
    m_username = settingsLineEdit(formHost);
    m_password = settingsLineEdit(formHost);
    m_password->setEchoMode(QLineEdit::Password);
    m_channel = settingsSpinBox(formHost);
    m_channel->setRange(1, 256);
    m_streamType = settingsSpinBox(formHost);
    m_streamType->setRange(0, 10);
    m_linkMode = settingsSpinBox(formHost);
    m_linkMode->setRange(0, 1);
    m_loginTimeout = settingsSpinBox(formHost);
    m_loginTimeout->setRange(100, 60000);
    m_loginTimeout->setSuffix(QStringLiteral(" ms"));
    m_reconnect = settingsSpinBox(formHost);
    m_reconnect->setRange(1000, 60000);
    m_reconnect->setSuffix(QStringLiteral(" ms"));

    form->addRow(m_enabled);
    addSettingsField(form, QStringLiteral("IP / Address"), m_address, QString(), formHost);
    addSettingsField(form, QStringLiteral("端口 / Port"), m_port, QString(), formHost);
    addSettingsField(form, QStringLiteral("用户名"), m_username, QString(), formHost);
    addSettingsField(form,
                     QStringLiteral("密码"),
                     m_password,
                     QStringLiteral("密码只写回 hik_camera.ini，界面以掩码显示，日志不记录密码。"),
                     formHost);
    addSettingsField(form, QStringLiteral("通道 / Channel"), m_channel, QString(), formHost);
    addSettingsField(form,
                     QStringLiteral("码流 / StreamType"),
                     m_streamType,
                     QStringLiteral("0=主码流，1=子码流。范围与 HikCameraConfig::validate 一致（0~10）。"),
                     formHost);
    addSettingsField(form,
                     QStringLiteral("传输 / LinkMode"),
                     m_linkMode,
                     QStringLiteral("0=TCP，1=UDP。这是传输方式，不是触发方式。"),
                     formHost);
    addSettingsField(form, QStringLiteral("登录超时"), m_loginTimeout, QString(), formHost);
    addSettingsField(form, QStringLiteral("重连间隔"), m_reconnect, QString(), formHost);
    panelLayout->addWidget(formHost);
    root->addWidget(panel);

    auto *pending = createPanel(QStringLiteral("未对接采集 / 标定"), nullptr, this);
    auto *pendingLayout = qobject_cast<QVBoxLayout *>(pending->layout());
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("触发方式"),
        QStringLiteral("待对接 hik_camera.ini（无 Trigger 键，HikCameraConfig 未读取）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("曝光"),
        QStringLiteral("待对接 hik_camera.ini（无 Exposure 键，HikCameraConfig 未读取）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("增益"),
        QStringLiteral("待对接 hik_camera.ini（无 Gain 键，HikCameraConfig 未读取）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("标定 / 坐标系"),
        QStringLiteral("待对接 相机标定文件。XYZ 补偿在「加工与工艺」的 backend_params.ini，不是相机标定。"),
        pending));
    pendingLayout->addWidget(settingsHint(
        QStringLiteral("POE 视觉网段：现场相机走独立网段。本页只维护登录参数，不配置交换机或路由。"),
        pending));
    root->addWidget(pending);
    root->addStretch(1);

    const auto mark = [this]() { notifyDirty(); };
    connect(m_enabled, &QCheckBox::toggled, this, mark);
    connect(m_address, &QLineEdit::textChanged, this, mark);
    connect(m_username, &QLineEdit::textChanged, this, mark);
    connect(m_password, &QLineEdit::textChanged, this, mark);
    connect(m_port, qOverload<int>(&QSpinBox::valueChanged), this, mark);
    connect(m_channel, qOverload<int>(&QSpinBox::valueChanged), this, mark);
    connect(m_streamType, qOverload<int>(&QSpinBox::valueChanged), this, mark);
    connect(m_linkMode, qOverload<int>(&QSpinBox::valueChanged), this, mark);
    connect(m_loginTimeout, qOverload<int>(&QSpinBox::valueChanged), this, mark);
    connect(m_reconnect, qOverload<int>(&QSpinBox::valueChanged), this, mark);
    reloadFromSource();
}

QString VisionSettingsPage::bindingSummary() const
{
    return QStringLiteral("绑定 hik_camera.ini [HikCamera]。曝光、增益、触发、标定待对接。");
}

bool VisionSettingsPage::isDirty() const
{
    return !sameConfig(currentConfig(), m_loaded);
}

SettingsSaveOutcome VisionSettingsPage::save()
{
    const QString path = HikCameraConfig::defaultFilePath();
    const HikCameraConfig config = currentConfig();
    QString error;
    if (!config.save(path, &error)) {
        return {SettingsSaveStatus::Rejected,
                error.isEmpty() ? QStringLiteral("海康配置未写入。") : error};
    }
    m_loaded = config;
    emit dirtyChanged(false);
    LOG("[相机配置] 已保存 hik_camera.ini enabled=%s address=%s port=%d file=%s",
        config.enabled ? "true" : "false",
        config.address.toUtf8().constData(),
        config.port,
        path.toUtf8().constData());
    return {SettingsSaveStatus::Saved,
            QStringLiteral("已写入 %1。下次开始海康预览时生效，当前画面不会热更新。密码未写入日志。")
                .arg(path)};
}

void VisionSettingsPage::revert()
{
    reloadFromSource();
}

void VisionSettingsPage::restoreDefaults()
{
    setConfig(factoryConfig());
    notifyDirty();
}

void VisionSettingsPage::reloadFromSource()
{
    const QString path = HikCameraConfig::defaultFilePath();
    QString error;
    HikCameraConfig config = HikCameraConfig::load(path, &error);
    if (!error.isEmpty() && !QFileInfo::exists(path)) {
        config = factoryConfig();
    }
    setConfig(config);
    m_loaded = currentConfig();
    emit dirtyChanged(false);
}

void VisionSettingsPage::setConfig(const HikCameraConfig &config)
{
    const QSignalBlocker blockEnabled(m_enabled);
    const QSignalBlocker blockAddress(m_address);
    const QSignalBlocker blockPort(m_port);
    const QSignalBlocker blockUser(m_username);
    const QSignalBlocker blockPassword(m_password);
    const QSignalBlocker blockChannel(m_channel);
    const QSignalBlocker blockStream(m_streamType);
    const QSignalBlocker blockLink(m_linkMode);
    const QSignalBlocker blockLogin(m_loginTimeout);
    const QSignalBlocker blockReconnect(m_reconnect);
    m_enabled->setChecked(config.enabled);
    m_address->setText(config.address);
    m_port->setValue(qBound(m_port->minimum(), config.port, m_port->maximum()));
    m_username->setText(config.username);
    m_password->setText(config.password);
    m_channel->setValue(qBound(m_channel->minimum(), config.channel, m_channel->maximum()));
    m_streamType->setValue(qBound(m_streamType->minimum(), config.streamType, m_streamType->maximum()));
    m_linkMode->setValue(qBound(m_linkMode->minimum(), config.linkMode, m_linkMode->maximum()));
    m_loginTimeout->setValue(
        qBound(m_loginTimeout->minimum(), config.loginTimeoutMs, m_loginTimeout->maximum()));
    m_reconnect->setValue(
        qBound(m_reconnect->minimum(), config.reconnectIntervalMs, m_reconnect->maximum()));
}

HikCameraConfig VisionSettingsPage::currentConfig() const
{
    HikCameraConfig config;
    config.enabled = m_enabled && m_enabled->isChecked();
    config.address = m_address ? m_address->text().trimmed() : QString();
    config.port = m_port ? m_port->value() : 8000;
    config.username = m_username ? m_username->text().trimmed() : QString();
    config.password = m_password ? m_password->text() : QString();
    config.channel = m_channel ? m_channel->value() : 1;
    config.streamType = m_streamType ? m_streamType->value() : 1;
    config.linkMode = m_linkMode ? m_linkMode->value() : 0;
    config.loginTimeoutMs = m_loginTimeout ? m_loginTimeout->value() : 3000;
    config.reconnectIntervalMs = m_reconnect ? m_reconnect->value() : 10000;
    return config;
}

void VisionSettingsPage::notifyDirty()
{
    emit dirtyChanged(isDirty());
}
