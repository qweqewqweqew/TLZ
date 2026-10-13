#ifndef SETTINGSFORMWIDGETS_H
#define SETTINGSFORMWIDGETS_H

#include "ElaCheckBox.h"

#include <QLabel>
#include <QString>

class QFormLayout;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QWidget;

QLabel *settingsHint(const QString &text, QWidget *parent);
QLabel *settingsFieldLabel(const QString &text, QWidget *parent);
QLabel *settingsSourceBanner(const QString &text, QWidget *parent);
QLineEdit *settingsLineEdit(QWidget *parent);
QSpinBox *settingsSpinBox(QWidget *parent);
QPlainTextEdit *settingsReadOnlyText(QWidget *parent);
ElaCheckBox *settingsCheckBox(const QString &text, QWidget *parent);
QWidget *settingsPendingBlock(const QString &title, const QString &note, QWidget *parent);
void addSettingsField(QFormLayout *form,
                      const QString &label,
                      QWidget *field,
                      const QString &note,
                      QWidget *parent);

#endif // SETTINGSFORMWIDGETS_H
