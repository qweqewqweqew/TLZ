#ifndef THEME_H
#define THEME_H

#include <QString>

#include <functional>

// 语义色。深色 / 浅色两套配色共用这一套名字，切换时改指针指向的色值。
// 调用处每次读取都会拿到当前主题，已经写进样式表的颜色要在主题信号里重刷。

class QObject;
class QWidget;

namespace Theme {

enum class Id {
    Dark = 0,
    Light,
};

// 功能说明里的目标画布。屏幕放得下时按这个尺寸打开，放不下就铺满当前屏幕。
constexpr int kDesignWidth = 2556;
constexpr int kDesignHeight = 1440;

extern const char *kWindowBg;
extern const char *kPanelBg;
extern const char *kControlBg;
extern const char *kCardBg;
extern const char *kCanvasBg;
extern const char *kLogBg;
extern const char *kStatusBarBg;

extern const char *kBorder;
extern const char *kDivider;
extern const char *kHoverBg;
extern const char *kPressedBg;
extern const char *kSelectionBg;

extern const char *kTextPrimary;
extern const char *kTextSecondary;
extern const char *kTextMuted;
extern const char *kTextField;
extern const char *kOnAccent;
extern const char *kTitle;
extern const char *kSelectionText;

extern const char *kAccent;
extern const char *kAccentFill;
extern const char *kOnAccentFill;
extern const char *kRealtime;

extern const char *kStateSuccess;
extern const char *kStateRunning;
extern const char *kStateWarning;
extern const char *kStateError;
extern const char *kStateUnknown;

extern const char *kSuccessBg;
extern const char *kRunningBg;
extern const char *kWarningBg;
extern const char *kErrorBg;

Id current();
void setCurrent(Id id);

// 重复出现的标注。麦肯锡页上的亮蓝只留给一处重点数字，标注改用藏蓝。
inline const char *markColor()
{
    return current() == Id::Light ? kTitle : kAccent;
}

// 实时读数。科技蓝用金色，麦肯锡不用亮蓝铺满每个数字。
inline const char *liveColor()
{
    return current() == Id::Light ? kTitle : kRealtime;
}
void load();

QString displayName(Id id);

// 主题切换后调用 apply。context 销毁时自动断开。
void watch(QObject *context, const std::function<void()> &apply);
// 立刻套上样式，并在之后每次切换主题时重套。
void bindStyle(QWidget *widget, const std::function<QString()> &sheet);

} // namespace Theme

#endif // THEME_H
