#ifndef APPSTYLE_H
#define APPSTYLE_H

#include <QString>

QString mainWindowStyleSheet(int panelRadius = 8);
// 把当前主题铺到应用样式、Ela 控件和已有按钮上。
void applyRuntimeTheme();

#endif // APPSTYLE_H
