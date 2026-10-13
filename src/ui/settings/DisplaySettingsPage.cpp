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
#include <QVBoxLayout>

DisplaySettingsPage::DisplaySettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);
    root->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：display.ini 的 [HeightMap3D] FlipX、CropLeftColumns、"
                       "CropRightColumns。查找路径与三维视图相同。三维视图只在创建时读取，"
                       "保存后要重新启动程序才作用到点云画面。"),
        this));
    root->addWidget(settingsHint(
        QStringLiteral("计划权限：技术员及以上。当前没有登录钩子，此项不锁定。"),
        this));

    auto *panel = createPanel(QStringLiteral("三维高度图"), nullptr, this);
    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    auto *formHost = new QWidget(panel);
    auto *form = new QFormLayout(formHost);
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    m_flipX = settingsCheckBox(QStringLiteral("左右翻转（FlipX=1）"), formHost);
    m_cropLeft = settingsSpinBox(formHost);
    m_cropLeft->setRange(0, 4096);
    m_cropRight = settingsSpinBox(formHost);
    m_cropRight->setRange(0, 4096);
    addSettingsField(form,
                     QStringLiteral("左右翻转"),
                     m_flipX,
                     QStringLiteral("写入整数 0 或 1。三维视图用 toInt()==1 判断，不能写成 true/false。"),
                     formHost);
    addSettingsField(form, QStringLiteral("左侧裁列"), m_cropLeft, QString(), formHost);
    addSettingsField(form,
                     QStringLiteral("右侧裁列"),
                     m_cropRight,
                     QStringLiteral("只影响三维显示，不改变检测数据。"),
                     formHost);
    panelLayout->addWidget(formHost);
    root->addWidget(panel);

    auto *pending = createPanel(QStringLiteral("未对接显示项"), nullptr, this);
    auto *pendingLayout = qobject_cast<QVBoxLayout *>(pending->layout());
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("图像叠加默认开关"),
        QStringLiteral("待对接 display.ini（无叠加开关键）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("曲线窗口"),
        QStringLiteral("待对接 display.ini（无曲线窗口键）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("主题 / 字号"),
        QStringLiteral("待对接 display.ini（主题现由 main.cpp 里 ElaTheme Dark 写死）。"),
        pending));
    pendingLayout->addWidget(settingsPendingBlock(
        QStringLiteral("主页卡片显隐"),
        QStringLiteral("待对接 display.ini（无卡片显隐键）。"),
        pending));
    root->addWidget(pending);
    root->addStretch(1);

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

void DisplaySettingsPage::notifyDirty()
{
    emit dirtyChanged(isDirty());
}
