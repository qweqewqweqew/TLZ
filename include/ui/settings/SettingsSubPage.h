#ifndef SETTINGSSUBPAGE_H
#define SETTINGSSUBPAGE_H

#include <QString>
#include <QWidget>

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

signals:
    void dirtyChanged(bool dirty);
    void historyRequested();

protected:
    void showEvent(QShowEvent *event) override;
    virtual void reloadFromSource() {}
};

#endif // SETTINGSSUBPAGE_H
