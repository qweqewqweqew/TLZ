#ifndef UIHELPERS_H
#define UIHELPERS_H

#include <QString>

class ElaPushButton;
class QFrame;
class QLabel;
class QWidget;

QLabel *makeLabel(const QString &text, const QString &objectName = QString());
QFrame *createPanel(const QString &title, QWidget *headerWidget = nullptr, QWidget *parent = nullptr);
QWidget *createStatusPill(const QString &text, const QString &state, QWidget *parent = nullptr);
void applyPrimaryButtonStyle(ElaPushButton *button);
void applySecondaryButtonStyle(ElaPushButton *button);

#endif // UIHELPERS_H
