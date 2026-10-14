#ifndef SETTINGSSUBPAGE_H
#define SETTINGSSUBPAGE_H

#include "auth/AuthTypes.h"

#include <QString>
#include <QWidget>

#include <optional>

class QShowEvent;

enum class SettingsSaveStatus {
    Saved,
    SavedWithWarning,
    Rejected,
};

struct SettingsSaveOutcome
{
    SettingsSaveStatus status{SettingsSaveStatus::Rejected};
    QString message;
};

// 设置子页。可写页必须读写既有 ini / 后端接口；没有字段时保持只读占位。
class SettingsSubPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsSubPage(QWidget *parent = nullptr);

    virtual QString bindingSummary() const = 0;
    virtual bool isEditable() const { return false; }
    virtual bool isDirty() const { return false; }
    virtual SettingsSaveOutcome save();
    virtual void revert() {}
    virtual void restoreDefaults() {}

    // 返回空表示本页没有受权限控制的写操作。
    virtual std::optional<Permission> writePermission() const { return std::nullopt; }
    // SettingsPage 按当前身份调用；页内可写控件据此置灰。
    virtual void setWriteAllowed(bool allowed) { Q_UNUSED(allowed); }
    // 保存前取「改前 → 改后」摘要写入审计，不得包含密码原文。
    virtual QString pendingChangeSummary() const { return QString(); }

signals:
    void dirtyChanged(bool dirty);
    // isEditable() 随页内状态（如当前页签）变化时发出，SettingsPage 据此刷新页底按钮。
    void editableChanged();
    void historyRequested();
    // 页内自带的「应用」按钮走与页底「保存」相同的权限校验和审计。
    void saveRequested();

protected:
    void showEvent(QShowEvent *event) override;
    virtual void reloadFromSource() {}
};

#endif // SETTINGSSUBPAGE_H
