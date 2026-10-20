#ifndef SETTINGSFORMWIDGETS_H
#define SETTINGSFORMWIDGETS_H

#include "ElaCheckBox.h"

#include <QLabel>
#include <QString>

class QFormLayout;
class QFrame;
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
QFrame *settingsGroupPanel(const QString &title, const QString &iconResource, QWidget *parent);
QLabel *settingsSectionHeading(const QString &title, QWidget *parent);
QFormLayout *settingsFormLayout(QWidget *host);
void addSettingsField(QFormLayout *form,
                      const QString &label,
                      QWidget *field,
                      const QString &note,
                      QWidget *parent);

// Legacy helper for settings subpages that still use secondary tabs.
QTabWidget *settingsTabs(QWidget *parent);
QVBoxLayout *addSettingsTab(QTabWidget *tabs,
                            const QString &title,
                            bool scrollable = true,
                            const QString &iconPath = QString());

#endif // SETTINGSFORMWIDGETS_H
