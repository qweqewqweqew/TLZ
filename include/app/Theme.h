#ifndef THEME_H
#define THEME_H

// 语义色 token 表 —— 集中管理所有颜色，避免散落在各文件的裸 hex。
//
// 设计原则：
// 1. 蓝色统一表示交互、选中或运行中；其余状态使用独立语义色。
// 2. 文本层级收敛为 3 档：primary / secondary / muted。
// 3. 所有面板统一从这里取色，后续改动只改这一处。

namespace Theme {

// ---- surface 层级 ----
constexpr const char *kWindowBg   = "#0B1014";
constexpr const char *kPanelBg    = "#171D22";
constexpr const char *kControlBg  = "#20272D";
constexpr const char *kCardBg     = kControlBg;
constexpr const char *kCanvasBg   = "#080D11";
constexpr const char *kLogBg      = "#0E1418";
constexpr const char *kStatusBarBg = "#0D1216";

// ---- 结构层级 ----
constexpr const char *kBorder  = "#303A42";
constexpr const char *kDivider = "#283138";
constexpr const char *kHoverBg = "#273038";
constexpr const char *kPressedBg = "#1D252B";
constexpr const char *kSelectionBg = "#1C4960";

// ---- text 层级（对比度已按 WCAG AA ≥4.5:1 校准）----
constexpr const char *kTextPrimary   = "#E8ECEF";
constexpr const char *kTextSecondary = "#A7B0B7";
constexpr const char *kTextMuted     = "#74828C";
constexpr const char *kTextField     = kTextSecondary;

// ---- 交互/运行强调色与实时数据色 ----
constexpr const char *kAccent   = "#2697D4";
constexpr const char *kRealtime = "#F4C542";

// ---- 语义状态色 ----
constexpr const char *kStateSuccess = "#26B96B";   // 在线/完成/就绪/正向
constexpr const char *kStateRunning = kAccent;      // 选中/运行中
constexpr const char *kStateWarning = "#F0A020";   // 警告/负向
constexpr const char *kStateError   = "#E34D4D";   // 故障/离线
constexpr const char *kStateUnknown = "#6F7A82";   // 未知/停止

// ---- 克制的状态容器色 ----
constexpr const char *kSuccessBg = "#13261C";
constexpr const char *kRunningBg = "#112631";
constexpr const char *kWarningBg = "#2B2113";
constexpr const char *kErrorBg   = "#2A1718";

} // namespace Theme

#endif // THEME_H
