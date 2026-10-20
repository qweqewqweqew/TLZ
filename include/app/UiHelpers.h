#ifndef UIHELPERS_H
#define UIHELPERS_H

#include <QColor>
#include <QFrame>
#include <QPixmap>
#include <QString>

class ElaPushButton;
class QLabel;
class QWidget;

QLabel *makeLabel(const QString &text, const QString &objectName = QString());
// 把单色 SVG 资源渲染成指定颜色的位图（图标只画形状，颜色随主题给）。
QPixmap tintedIcon(const QString &resource, const QColor &color, int size, qreal devicePixelRatio = 1.0);
QFrame *createPanel(const QString &title, QWidget *headerWidget = nullptr, QWidget *parent = nullptr);
QWidget *createStatusPill(const QString &text, const QString &state, QWidget *parent = nullptr);
void applyPrimaryButtonStyle(ElaPushButton *button);
void applySecondaryButtonStyle(ElaPushButton *button);

#endif // UIHELPERS_H
