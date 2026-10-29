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

// 深色监控盘：页面、卡片、画面井三层分开。正文、实时数、状态色各用各的。
// 绿/红按钮底用能压住白字的深度；琥珀按钮由控件按色相改深色字。
const Palette kDark = {
    "#0F172A", "#1B2336", "#272F42", "#1B2336", "#020617", "#020617", "#1B2336",
    "#334155", "#2A3548", "#334155", "#1E293B", "#1E3A5F", "#25486F", "#1A334F", "#38BDF8",
    "#E2E8F0", "#CBD5E1", "#94A3B8", "#CBD5E1", "#0F172A",
    "#F8FAFC", "#F8FAFC",
    "#38BDF8", "#0369A1", "#FFFFFF", "#7DD3FC",
    "#15803D", "#0284C7", "#D97706", "#DC2626", "#94A3B8",
    "#052E16", "#082F49", "#3B2A0A", "#3F1212",
};

// 浅色同一套角色：白卡片，画面井用浅灰，实时数只用一处蓝。
const Palette kLight = {
    "#F1F5F9", "#FFFFFF", "#F8FAFC", "#FFFFFF", "#E8EEF4", "#F8FAFC", "#FFFFFF",
    "#CBD5E1", "#E2E8F0", "#E2E8F0", "#CBD5E1", "#E0F2FE", "#BAE6FD", "#7DD3FC", "#0369A1",
    "#0F172A", "#334155", "#64748B", "#334155", "#FFFFFF",
    "#0F172A", "#0F172A",
    "#0369A1", "#0369A1", "#FFFFFF", "#0369A1",
    "#15803D", "#0369A1", "#B45309", "#DC2626", "#64748B",
    "#DCFCE7", "#E0F2FE", "#FEF3C7", "#FEE2E2",
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
