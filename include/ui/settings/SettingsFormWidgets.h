#ifndef SETTINGSFORMWIDGETS_H
#define SETTINGSFORMWIDGETS_H

#include "ElaCheckBox.h"

#include <QLabel>
#include <QString>

class QFormLayout;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;
class QVBoxLayout;
class QWidget;

QLabel *settingsHint(const QString &text, QWidget *parent);
QLabel *settingsFieldLabel(const QString &text, QWidget *parent);
QLabel *settingsSourceBanner(const QString &text, QWidget *parent);
QLineEdit *settingsLineEdit(QWidget *parent);
QSpinBox *settingsSpinBox(QWidget *parent);
QPlainTextEdit *settingsReadOnlyText(QWidget *parent);
ElaCheckBox *settingsCheckBox(const QString &text, QWidget *parent);
QWidget *settingsPendingBlock(const QString &title, const QString &note, QWidget *parent);
QFormLayout *settingsFormLayout(QWidget *host);
void addSettingsField(QFormLayout *form,
                      const QString &label,
                      QWidget *field,
                      const QString &note,
                      QWidget *parent);

// 设置子页内的二级页签。每个页签自带滚动，外层设置页不再整体滚动。
QTabWidget *settingsTabs(QWidget *parent);
// scrollable=false 时内容占满页签高度，适合表格自己滚动的页签。
QVBoxLayout *addSettingsTab(QTabWidget *tabs, const QString &title, bool scrollable = true);

#endif // SETTINGSFORMWIDGETS_H
