#include "ui/settings/DisplaySettingsPage.h"

#include "app/Logger.h"
#include "app/RuntimeIniPaths.h"
#include "app/UiHelpers.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>

DisplaySettingsPage::DisplaySettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *tabs = settingsTabs(this);
    root->addWidget(tabs);

    QVBoxLayout *heightTab = addSettingsTab(tabs, QStringLiteral("三维高度图"));
    auto *panel = createPanel(QStringLiteral("显示裁切与翻转"), nullptr, this);
    auto *formHost = new QWidget(panel);
    m_form = formHost;
    QFormLayout *form = settingsFormLayout(formHost);

    m_flipX = settingsCheckBox(QStringLiteral("左右翻转"), formHost);
    m_cropLeft = settingsSpinBox(formHost);
    m_cropLeft->setRange(0, 4096);
    m_cropLeft->setSuffix(QStringLiteral(" 列"));
    m_cropRight = settingsSpinBox(formHost);
    m_cropRight->setRange(0, 4096);
    m_cropRight->setSuffix(QStringLiteral(" 列"));
    addSettingsField(form, QStringLiteral("翻转"), m_flipX, QString(), formHost);
    addSettingsField(form, QStringLiteral("左侧裁列"), m_cropLeft, QString(), formHost);
    addSettingsField(form,
                     QStringLiteral("右侧裁列"),
                     m_cropRight,
                     QStringLiteral("只影响三维显示，不改变检测数据。"),
                     formHost);
    qobject_cast<QVBoxLayout *>(panel->layout())->addWidget(formHost);
    panel->setMaximumWidth(640);
    heightTab->addWidget(panel);
    heightTab->addWidget(settingsHint(
        QStringLiteral("技术员及以上可修改。三维视图只在创建时读取，保存后重启程序才作用到点云画面。"),
        this));
    heightTab->addStretch(1);
    heightTab->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：display.ini 的 [HeightMap3D]（FlipX 写 0/1，CropLeftColumns，CropRightColumns）。"),
        this));

    QVBoxLayout *pendingTab = addSettingsTab(tabs, QStringLiteral("界面与主题"));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("图像叠加默认开关"),
        QStringLiteral("display.ini 无叠加开关键。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("曲线窗口"),
        QStringLiteral("display.ini 无曲线窗口键。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("主题 / 字号"),
        QStringLiteral("主题现由 main.cpp 写死为深色。"),
        this));
    pendingTab->addWidget(settingsPendingBlock(
        QStringLiteral("主页卡片显隐"),
        QStringLiteral("display.ini 无卡片显隐键。"),
        this));
    pendingTab->addStretch(1);

    connect(m_flipX, &QCheckBox::toggled, this, [this]() { notifyDirty(); });
    connect(m_cropLeft, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { notifyDirty(); });
    connect(m_cropRight, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { notifyDirty(); });
    reloadFromSource();
}

QString DisplaySettingsPage::bindingSummary() const
{
    return QStringLiteral("绑定 display.ini [HeightMap3D]。叠加、曲线、主题、卡片显隐待对接。目标文件：%1")
        .arg(targetPath());
}

bool DisplaySettingsPage::isDirty() const
{
    const Values now = currentValues();
    return now.flipX != m_loaded.flipX
           || now.cropLeft != m_loaded.cropLeft
           || now.cropRight != m_loaded.cropRight;
}

SettingsSaveOutcome DisplaySettingsPage::save()
{
    const QString path = targetPath();
    const Values values = currentValues();
    QSettings settings(path, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("HeightMap3D"));
    settings.setValue(QStringLiteral("FlipX"), values.flipX ? 1 : 0);
    settings.setValue(QStringLiteral("CropLeftColumns"), values.cropLeft);
    settings.setValue(QStringLiteral("CropRightColumns"), values.cropRight);
    settings.endGroup();

    QString error;
    if (!syncSettingsFile(settings, path, &error)) {
        return {SettingsSaveStatus::Rejected, error};
    }
    m_loaded = values;
    emit dirtyChanged(false);
    LOG("[显示配置] 已保存 display.ini flipX=%d crop=%d/%d file=%s",
        values.flipX ? 1 : 0,
        values.cropLeft,
        values.cropRight,
        path.toUtf8().constData());
    return {SettingsSaveStatus::Saved,
            QStringLiteral("已写入 %1。三维视图下次启动才重新读取，当前点云画面不变。").arg(path)};
}

void DisplaySettingsPage::revert()
{
    reloadFromSource();
}

void DisplaySettingsPage::restoreDefaults()
{
    Values values;
    values.flipX = false;
    values.cropLeft = 120;
    values.cropRight = 120;
    setValues(values);
    notifyDirty();
}

void DisplaySettingsPage::reloadFromSource()
{
    Values values;
    values.flipX = false;
    values.cropLeft = 120;
    values.cropRight = 120;
    const QString path = displayIniPath();
    if (!path.isEmpty()) {
        QSettings settings(path, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        values.flipX = settings.value(QStringLiteral("HeightMap3D/FlipX"), 0).toInt() == 1;
        values.cropLeft = qMax(0, settings.value(QStringLiteral("HeightMap3D/CropLeftColumns"), 120).toInt());
        values.cropRight = qMax(0, settings.value(QStringLiteral("HeightMap3D/CropRightColumns"), 120).toInt());
    }
    setValues(values);
    m_loaded = currentValues();
    emit dirtyChanged(false);
}

void DisplaySettingsPage::setValues(const Values &values)
{
    const QSignalBlocker blockFlip(m_flipX);
    const QSignalBlocker blockLeft(m_cropLeft);
    const QSignalBlocker blockRight(m_cropRight);
    m_flipX->setChecked(values.flipX);
    m_cropLeft->setValue(qBound(0, values.cropLeft, m_cropLeft->maximum()));
    m_cropRight->setValue(qBound(0, values.cropRight, m_cropRight->maximum()));
}

DisplaySettingsPage::Values DisplaySettingsPage::currentValues() const
{
    Values values;
    values.flipX = m_flipX && m_flipX->isChecked();
    values.cropLeft = m_cropLeft ? m_cropLeft->value() : 120;
    values.cropRight = m_cropRight ? m_cropRight->value() : 120;
    return values;
}

QString DisplaySettingsPage::targetPath() const
{
    const QString existing = displayIniPath();
    if (!existing.isEmpty()) {
        return existing;
    }
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("display.ini"));
}

void DisplaySettingsPage::setWriteAllowed(bool allowed)
{
    if (m_form) {
        m_form->setEnabled(allowed);
    }
}

QString DisplaySettingsPage::pendingChangeSummary() const
{
    const Values now = currentValues();
    QStringList changes;
    if (now.flipX != m_loaded.flipX) {
        changes << QStringLiteral("左右翻转 %1 → %2")
                       .arg(m_loaded.flipX ? 1 : 0)
                       .arg(now.flipX ? 1 : 0);
    }
    if (now.cropLeft != m_loaded.cropLeft) {
        changes << QStringLiteral("左侧裁列 %1 → %2").arg(m_loaded.cropLeft).arg(now.cropLeft);
    }
    if (now.cropRight != m_loaded.cropRight) {
        changes << QStringLiteral("右侧裁列 %1 → %2").arg(m_loaded.cropRight).arg(now.cropRight);
    }
    return changes.join(QStringLiteral("；"));
}

void DisplaySettingsPage::notifyDirty()
{
    emit dirtyChanged(isDirty());
}
