#include "ui/settings/VisionSettingsPage.h"

#include "app/Logger.h"
#include "app/UiHelpers.h"
#include "camera/HikCameraConfig.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QCheckBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
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
    root->setSpacing(0);
    auto *tabs = settingsTabs(this);
    root->addWidget(tabs);

    QVBoxLayout *connectionTab = addSettingsTab(tabs, QStringLiteral("海康连接"));
    auto *formHost = new QWidget(this);
    m_form = formHost;
    auto *columns = new QHBoxLayout(formHost);
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(16);

    auto *loginPanel = createPanel(QStringLiteral("登录"), nullptr, formHost);
    auto *loginHost = new QWidget(loginPanel);
    QFormLayout *form = settingsFormLayout(loginHost);
    qobject_cast<QVBoxLayout *>(loginPanel->layout())->addWidget(loginHost);
    qobject_cast<QVBoxLayout *>(loginPanel->layout())->addStretch(1);

    auto *streamPanel = createPanel(QStringLiteral("码流与重连"), nullptr, formHost);
    auto *streamHost = new QWidget(streamPanel);
    QFormLayout *streamForm = settingsFormLayout(streamHost);
    qobject_cast<QVBoxLayout *>(streamPanel->layout())->addWidget(streamHost);
    qobject_cast<QVBoxLayout *>(streamPanel->layout())->addStretch(1);

    columns->addWidget(loginPanel, 1);
    columns->addWidget(streamPanel, 1);

    m_enabled = settingsCheckBox(QStringLiteral("启用海康预览"), formHost);
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

    form->addRow(new QLabel(loginHost), m_enabled);
    addSettingsField(form, QStringLiteral("IP 地址"), m_address, QString(), loginHost);
    addSettingsField(form, QStringLiteral("端口"), m_port, QString(), loginHost);
    addSettingsField(form, QStringLiteral("用户名"), m_username, QString(), loginHost);
    addSettingsField(form,
                     QStringLiteral("密码"),
                     m_password,
                     QStringLiteral("以掩码显示，日志不记录密码。"),
                     loginHost);
    addSettingsField(streamForm, QStringLiteral("通道"), m_channel, QString(), streamHost);
    addSettingsField(streamForm,
                     QStringLiteral("码流"),
                     m_streamType,
                     QStringLiteral("0 = 主码流，1 = 子码流。"),
                     streamHost);
    addSettingsField(streamForm,
                     QStringLiteral("传输方式"),
                     m_linkMode,
                     QStringLiteral("0 = TCP，1 = UDP（不是触发方式）。"),
                     streamHost);
    addSettingsField(streamForm, QStringLiteral("登录超时"), m_loginTimeout, QString(), streamHost);
    addSettingsField(streamForm, QStringLiteral("重连间隔"), m_reconnect, QString(), streamHost);

    connectionTab->addWidget(formHost);
    connectionTab->addWidget(settingsHint(
        QStringLiteral("只有管理员能修改相机登录参数。保存后在下次开始海康预览时生效，已打开的画面不会热更新。"),
        this));
    connectionTab->addStretch(1);
    connectionTab->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：%1 的 [HikCamera]。").arg(HikCameraConfig::defaultFilePath()),
        this));

    QVBoxLayout *pendingTab = addSettingsTab(tabs, QStringLiteral("采集与标定"));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("触发方式"),
        QStringLiteral("hik_camera.ini 无 Trigger 键，HikCameraConfig 未读取。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("曝光"),
        QStringLiteral("hik_camera.ini 无 Exposure 键，HikCameraConfig 未读取。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("增益"),
        QStringLiteral("hik_camera.ini 无 Gain 键，HikCameraConfig 未读取。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("标定 / 坐标系"),
        QStringLiteral("相机标定文件。XYZ 补偿在「加工与工艺」，不是相机标定。"),
        this));
    pendingTab->addWidget(settingsHint(
        QStringLiteral("现场相机走独立 POE 网段。本页只维护登录参数，不配置交换机或路由。"),
        this));
    pendingTab->addStretch(1);

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

void VisionSettingsPage::setWriteAllowed(bool allowed)
{
    if (m_form) {
        m_form->setEnabled(allowed);
    }
}

QString VisionSettingsPage::pendingChangeSummary() const
{
    const HikCameraConfig now = currentConfig();
    const HikCameraConfig &was = m_loaded;
    QStringList changes;
    const auto addText = [&changes](const QString &name, const QString &before, const QString &after) {
        if (before != after) {
            changes << QStringLiteral("%1 %2 → %3").arg(name, before, after);
        }
    };
    const auto addInt = [&addText](const QString &name, int before, int after) {
        addText(name, QString::number(before), QString::number(after));
    };
    addText(QStringLiteral("启用"),
            was.enabled ? QStringLiteral("是") : QStringLiteral("否"),
            now.enabled ? QStringLiteral("是") : QStringLiteral("否"));
    addText(QStringLiteral("地址"), was.address, now.address);
    addInt(QStringLiteral("端口"), was.port, now.port);
    addText(QStringLiteral("用户名"), was.username, now.username);
    if (was.password != now.password) {
        changes << QStringLiteral("密码已修改");
    }
    addInt(QStringLiteral("通道"), was.channel, now.channel);
    addInt(QStringLiteral("码流"), was.streamType, now.streamType);
    addInt(QStringLiteral("传输"), was.linkMode, now.linkMode);
    addInt(QStringLiteral("登录超时 ms"), was.loginTimeoutMs, now.loginTimeoutMs);
    addInt(QStringLiteral("重连间隔 ms"), was.reconnectIntervalMs, now.reconnectIntervalMs);
    return changes.join(QStringLiteral("；"));
}

void VisionSettingsPage::notifyDirty()
{
    emit dirtyChanged(isDirty());
}
