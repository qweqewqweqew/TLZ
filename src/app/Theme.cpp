#include "app/Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <utility>

namespace Theme {
namespace {

struct Palette
{
    const char *windowBg;
    const char *panelBg;
    const char *controlBg;
    const char *cardBg;
    const char *canvasBg;
    const char *logBg;
    const char *statusBarBg;
    const char *border;
    const char *divider;
    const char *hoverBg;
    const char *pressedBg;
    const char *selectionBg;
    const char *selectedHoverBg;
    const char *selectedPressedBg;
    const char *focusRing;
    const char *textPrimary;
    const char *textSecondary;
    const char *textMuted;
    const char *textField;
    const char *onAccent;
    const char *title;
    const char *selectionText;
    const char *accent;
    const char *accentFill;
    const char *onAccentFill;
    const char *realtime;
    const char *stateSuccess;
    const char *stateRunning;
    const char *stateWarning;
    const char *stateError;
    const char *stateUnknown;
    const char *successBg;
    const char *runningBg;
    const char *warningBg;
    const char *errorBg;
};

// 深色：炭灰分层，钢蓝做交互，实时数用金色。
const Palette kDark = {
    "#0B1014", "#171D22", "#20272D", "#20272D", "#080D11", "#0E1418", "#0D1216",
    "#303A42", "#283138", "#1C252C", "#111920", "#244B62", "#1F4056", "#173449", "#55B8E6",
    "#E8ECEF", "#A7B0B7", "#74828C", "#A7B0B7", "#FFFFFF",
    "#E8ECEF", "#E8ECEF",
    "#2697D4", "#2697D4", "#FFFFFF", "#F4C542",
    "#26B96B", "#2697D4", "#F0A020", "#E34D4D", "#6F7A82",
    "#13261C", "#112631", "#2B2113", "#2A1718",
};

// 浅色沿用麦肯锡：白底，标题藏蓝，正文石板灰，亮蓝每页只留一处。
const Palette kLight = {
    "#FFFFFF", "#FFFFFF", "#F0F4F8", "#FFFFFF", "#F0F4F8", "#F0F4F8", "#FFFFFF",
    "#D6E0E9", "#E7EDF3", "#E5F0F7", "#C4DDED", "#D7ECFA", "#C0E2F7", "#9FCDE8", "#287DB2",
    "#4E5B66", "#4E5B66", "#4E5B66", "#4E5B66", "#FFFFFF",
    "#002D62", "#17466F",
    "#2251FF", "#002D62", "#FFFFFF", "#2251FF",
    "#03735C", "#002D62", "#B86E00", "#C73E3A", "#4E5B66",
    "#E5F4F0", "#F0F4F8", "#FBF3E6", "#FBECEA",
};

Id g_id = Id::Light;

const Palette &paletteFor(Id id)
{
    switch (id) {
    case Id::Dark:
        return kDark;
    case Id::Light:
    default:
        return kLight;
    }
}

void assign(const Palette &palette)
{
    kWindowBg = palette.windowBg;
    kPanelBg = palette.panelBg;
    kControlBg = palette.controlBg;
    kCardBg = palette.cardBg;
    kCanvasBg = palette.canvasBg;
    kLogBg = palette.logBg;
    kStatusBarBg = palette.statusBarBg;
    kBorder = palette.border;
    kDivider = palette.divider;
    kHoverBg = palette.hoverBg;
    kPressedBg = palette.pressedBg;
    kSelectionBg = palette.selectionBg;
    kSelectedHoverBg = palette.selectedHoverBg;
    kSelectedPressedBg = palette.selectedPressedBg;
    kFocusRing = palette.focusRing;
    kTextPrimary = palette.textPrimary;
    kTextSecondary = palette.textSecondary;
    kTextMuted = palette.textMuted;
    kTextField = palette.textField;
    kOnAccent = palette.onAccent;
    kTitle = palette.title;
    kSelectionText = palette.selectionText;
    kAccent = palette.accent;
    kAccentFill = palette.accentFill;
    kOnAccentFill = palette.onAccentFill;
    kRealtime = palette.realtime;
    kStateSuccess = palette.stateSuccess;
    kStateRunning = palette.stateRunning;
    kStateWarning = palette.stateWarning;
    kStateError = palette.stateError;
    kStateUnknown = palette.stateUnknown;
    kSuccessBg = palette.successBg;
    kRunningBg = palette.runningBg;
    kWarningBg = palette.warningBg;
    kErrorBg = palette.errorBg;
}

QString themeFilePath()
{
    if (!QCoreApplication::instance()) {
        return QString();
    }
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("ui_theme.ini"));
}

QString keyFor(Id id)
{
    return id == Id::Dark ? QStringLiteral("dark") : QStringLiteral("light");
}

Id idFromKey(const QString &key)
{
    if (key == QLatin1String("dark") || key == QLatin1String("tech")) {
        return Id::Dark;
    }
    return Id::Light;
}

void save()
{
    const QString path = themeFilePath();
    if (path.isEmpty()) {
        return;
    }
    QSettings settings(path, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QStringLiteral("Ui/Theme"), keyFor(g_id));
    settings.sync();
}

struct Binding
{
    QPointer<QObject> context;
    std::function<void()> apply;
};

QVector<Binding> g_bindings;

void notifyWatchers()
{
    QVector<Binding> live;
    live.reserve(g_bindings.size());
    for (const Binding &binding : g_bindings) {
        if (!binding.context) {
            continue;
        }
        live.append(binding);
    }
    g_bindings = live;
    const QVector<Binding> pending = g_bindings;
    for (const Binding &binding : pending) {
        if (binding.context && binding.apply) {
            binding.apply();
        }
    }
}

} // namespace

const char *kWindowBg = kLight.windowBg;
const char *kPanelBg = kLight.panelBg;
const char *kControlBg = kLight.controlBg;
const char *kCardBg = kLight.cardBg;
const char *kCanvasBg = kLight.canvasBg;
const char *kLogBg = kLight.logBg;
const char *kStatusBarBg = kLight.statusBarBg;
const char *kBorder = kLight.border;
const char *kDivider = kLight.divider;
const char *kHoverBg = kLight.hoverBg;
const char *kPressedBg = kLight.pressedBg;
const char *kSelectionBg = kLight.selectionBg;
const char *kSelectedHoverBg = kLight.selectedHoverBg;
const char *kSelectedPressedBg = kLight.selectedPressedBg;
const char *kFocusRing = kLight.focusRing;
const char *kTextPrimary = kLight.textPrimary;
const char *kTextSecondary = kLight.textSecondary;
const char *kTextMuted = kLight.textMuted;
const char *kTextField = kLight.textField;
const char *kOnAccent = kLight.onAccent;
const char *kTitle = kLight.title;
const char *kSelectionText = kLight.selectionText;
const char *kAccent = kLight.accent;
const char *kAccentFill = kLight.accentFill;
const char *kOnAccentFill = kLight.onAccentFill;
const char *kRealtime = kLight.realtime;
const char *kStateSuccess = kLight.stateSuccess;
const char *kStateRunning = kLight.stateRunning;
const char *kStateWarning = kLight.stateWarning;
const char *kStateError = kLight.stateError;
const char *kStateUnknown = kLight.stateUnknown;
const char *kSuccessBg = kLight.successBg;
const char *kRunningBg = kLight.runningBg;
const char *kWarningBg = kLight.warningBg;
const char *kErrorBg = kLight.errorBg;

Id current()
{
    return g_id;
}

void setCurrent(Id id)
{
    if (g_id == id) {
        return;
    }
    g_id = id;
    assign(paletteFor(g_id));
    if (!QCoreApplication::instance() || !qApp) {
        save();
        return;
    }

    QVector<QPointer<QWidget>> frozen;
    const auto windows = qApp->topLevelWidgets();
    frozen.reserve(windows.size());
    for (QWidget *window : windows) {
        if (window && window->updatesEnabled()) {
            window->setUpdatesEnabled(false);
            frozen.append(window);
        }
    }
    notifyWatchers();
    for (const QPointer<QWidget> &window : frozen) {
        if (window) {
            window->setUpdatesEnabled(true);
        }
    }
    QTimer::singleShot(0, qApp, []() { save(); });
}

void load()
{
    const QString path = themeFilePath();
    Id id = Id::Light;
    if (!path.isEmpty()) {
        QSettings settings(path, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        id = idFromKey(settings.value(QStringLiteral("Ui/Theme"), QStringLiteral("light")).toString());
    }
    g_id = id;
    assign(paletteFor(g_id));
}

QString displayName(Id id)
{
    return id == Id::Dark ? QStringLiteral("深色模式") : QStringLiteral("浅色模式");
}

void watch(QObject *context, const std::function<void()> &apply)
{
    if (!context || !apply) {
        return;
    }
    g_bindings.append(Binding{context, apply});
}

void bindStyle(QWidget *widget, const std::function<QString()> &sheet)
{
    if (!widget || !sheet) {
        return;
    }
    widget->setStyleSheet(sheet());
    watch(widget, [widget, sheet]() {
        if (widget) {
            widget->setStyleSheet(sheet());
        }
    });
}

} // namespace Theme
