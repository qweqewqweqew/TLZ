#include "app/AppStyle.h"

#include "ElaPushButton.h"
#include "ElaTheme.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QApplication>
#include <QColor>
#include <QStyle>
#include <QWidget>

QString mainWindowStyleSheet(int panelRadius)
{
    QString style = QString(R"(
        QMainWindow {
            background: @WINDOW_BG@;
        }
        QWidget {
            font-family: "Microsoft YaHei";
            font-size: 14px;
            color: @TEXT_PRIMARY@;
        }
        QFrame#titleBar {
            background: @STATUS_BG@;
            border: 1px solid @DIVIDER@;
            border-radius: 8px;
        }
        QFrame#controlBar {
            background: @PANEL_BG@;
            border: 1px solid @DIVIDER@;
            border-radius: 8px;
        }
        QFrame#panel {
            background: @PANEL_BG@;
            border: 1px solid @BORDER@;
            border-radius: %1px;
        }
        QLabel#panelTitle {
            color: @TITLE@;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#systemTitle {
            color: @TITLE@;
            font-size: 26px;
            font-weight: 700;
        }
        QLabel#topTime {
            color: @TEXT_PRIMARY@;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#sectionHint {
            color: @TEXT_MUTED@;
            font-size: 12px;
        }
        QLabel#largeValue {
            color: @ACCENT@;
            font-size: 22px;
            font-weight: 600;
        }
        QLabel#metricName {
            color: @TEXT_SECONDARY@;
        }
        QLabel#metricValue {
            color: @TEXT_PRIMARY@;
            font-weight: 600;
        }
        QLabel#imageMainText {
            color: @TEXT_MUTED@;
            font-size: 18px;
            font-weight: 600;
        }
        ElaComboBox {
            background: @CONTROL_BG@;
            color: @TEXT_PRIMARY@;
            border: 1px solid @BORDER@;
            border-radius: 5px;
            padding-left: 8px;
        }
        ElaPlainTextEdit {
            background: @LOG_BG@;
            color: @TEXT_SECONDARY@;
            border: 1px solid @DIVIDER@;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: @SELECTION_BG@;
        }
        QStatusBar {
            background: @STATUS_BG@;
            color: @TEXT_SECONDARY@;
            border-top: 1px solid @DIVIDER@;
        }
        QLabel { color: @TEXT_PRIMARY@; }
        QPushButton {
            color: @TEXT_PRIMARY@;
            background: @CONTROL_BG@;
            border: 1px solid @BORDER@;
            border-radius: 6px;
            padding: 4px 10px;
        }
        QPushButton:hover { background: @HOVER@; }
        QPushButton:pressed { background: @PRESSED@; }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QComboBox, QAbstractSpinBox {
            color: @TEXT_PRIMARY@;
            background: @PANEL_BG@;
            border: 1px solid @BORDER@;
            border-radius: 5px;
            selection-background-color: @SELECTION_BG@;
            selection-color: @SELECTION_TEXT@;
        }
        QCheckBox, QRadioButton { color: @TEXT_PRIMARY@; background: transparent; }
        QTableWidget, QTableView {
            color: @TEXT_PRIMARY@;
            background: @PANEL_BG@;
            alternate-background-color: @CONTROL_BG@;
            gridline-color: @BORDER@;
            selection-background-color: @SELECTION_BG@;
            selection-color: @SELECTION_TEXT@;
            border: 1px solid @BORDER@;
        }
        QHeaderView::section {
            color: @TEXT_SECONDARY@;
            background: @CONTROL_BG@;
            border: none;
            border-bottom: 1px solid @BORDER@;
            padding: 6px 8px;
            font-weight: 600;
        }
        QTableCornerButton::section { background: @CONTROL_BG@; border: none; }
        QListWidget, QListView, QTreeView {
            color: @TEXT_PRIMARY@;
            background: @PANEL_BG@;
            border: 1px solid @BORDER@;
            selection-background-color: @SELECTION_BG@;
            selection-color: @SELECTION_TEXT@;
        }
        QTabWidget::pane { border: 1px solid @BORDER@; background: @PANEL_BG@; }
        QTabBar::tab {
            color: @TEXT_SECONDARY@;
            background: @CONTROL_BG@;
            border: 1px solid @BORDER@;
            padding: 6px 14px;
        }
        QTabBar::tab:selected { color: @TITLE@; background: @PANEL_BG@; }
        QMenu {
            color: @TEXT_PRIMARY@;
            background: @PANEL_BG@;
            border: 1px solid @BORDER@;
            padding: 4px;
        }
        QMenu::item { padding: 6px 18px; background: transparent; }
        QMenu::item:selected { background: @SELECTION_BG@; color: @SELECTION_TEXT@; }
        QScrollBar:vertical, QScrollBar:horizontal { background: @CONTROL_BG@; }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: @BORDER@;
            border-radius: 3px;
            min-height: 24px;
            min-width: 24px;
        }
        QToolTip {
            color: @TEXT_PRIMARY@;
            background: @PANEL_BG@;
            border: 1px solid @BORDER@;
        }
        QGroupBox { color: @TITLE@; border: 1px solid @BORDER@; }
    )").arg(panelRadius);
    style.replace(QStringLiteral("@WINDOW_BG@"), Theme::kWindowBg);
    style.replace(QStringLiteral("@PANEL_BG@"), Theme::kPanelBg);
    style.replace(QStringLiteral("@CONTROL_BG@"), Theme::kControlBg);
    style.replace(QStringLiteral("@LOG_BG@"), Theme::kLogBg);
    style.replace(QStringLiteral("@STATUS_BG@"), Theme::kStatusBarBg);
    style.replace(QStringLiteral("@BORDER@"), Theme::kBorder);
    style.replace(QStringLiteral("@DIVIDER@"), Theme::kDivider);
    style.replace(QStringLiteral("@SELECTION_BG@"), Theme::kSelectionBg);
    style.replace(QStringLiteral("@TEXT_PRIMARY@"), Theme::kTextPrimary);
    style.replace(QStringLiteral("@TEXT_SECONDARY@"), Theme::kTextSecondary);
    style.replace(QStringLiteral("@TEXT_MUTED@"), Theme::kTextMuted);
    style.replace(QStringLiteral("@REALTIME@"), Theme::kRealtime);
    style.replace(QStringLiteral("@TITLE@"), Theme::kTitle);
    style.replace(QStringLiteral("@ACCENT@"), Theme::kAccent);
    style.replace(QStringLiteral("@HOVER@"), Theme::kHoverBg);
    style.replace(QStringLiteral("@PRESSED@"), Theme::kPressedBg);
    style.replace(QStringLiteral("@SELECTION_TEXT@"), Theme::kSelectionText);
    return style;
}

void applyRuntimeTheme()
{
    if (!qApp) {
        return;
    }

    const auto paintEla = [](ElaThemeType::ThemeMode mode, ElaThemeType::ThemeColor key, const char *hex) {
        eTheme->setThemeColor(mode, key, QColor(QLatin1String(hex)));
    };
    const ElaThemeType::ThemeMode modes[] = {ElaThemeType::Light, ElaThemeType::Dark};
    for (ElaThemeType::ThemeMode mode : modes) {
        paintEla(mode, ElaThemeType::WindowBase, Theme::kWindowBg);
        paintEla(mode, ElaThemeType::WindowCentralStackBase, Theme::kWindowBg);
        paintEla(mode, ElaThemeType::PrimaryNormal, Theme::kAccentFill);
        paintEla(mode, ElaThemeType::PrimaryHover, Theme::kAccentFill);
        paintEla(mode, ElaThemeType::PrimaryPress, Theme::kPressedBg);
        paintEla(mode, ElaThemeType::BasicText, Theme::kTextPrimary);
        paintEla(mode, ElaThemeType::BasicTextInvert, Theme::kOnAccentFill);
        paintEla(mode, ElaThemeType::BasicDetailsText, Theme::kTextSecondary);
        paintEla(mode, ElaThemeType::BasicTextNoFocus, Theme::kTextMuted);
        paintEla(mode, ElaThemeType::BasicTextDisable, Theme::kTextMuted);
        paintEla(mode, ElaThemeType::BasicBase, Theme::kPanelBg);
        paintEla(mode, ElaThemeType::BasicBaseDeep, Theme::kControlBg);
        paintEla(mode, ElaThemeType::BasicBorder, Theme::kBorder);
        paintEla(mode, ElaThemeType::BasicBorderDeep, Theme::kBorder);
        paintEla(mode, ElaThemeType::BasicHover, Theme::kHoverBg);
        paintEla(mode, ElaThemeType::BasicPress, Theme::kPressedBg);
        paintEla(mode, ElaThemeType::PopupBase, Theme::kPanelBg);
        paintEla(mode, ElaThemeType::PopupHover, Theme::kHoverBg);
        paintEla(mode, ElaThemeType::DialogBase, Theme::kPanelBg);
        paintEla(mode, ElaThemeType::DialogLayoutArea, Theme::kWindowBg);
    }

    const bool dark = Theme::current() == Theme::Id::Dark;
    eTheme->setThemeMode(dark ? ElaThemeType::Dark : ElaThemeType::Light);

    qApp->setStyleSheet(mainWindowStyleSheet());
    const auto widgets = qApp->allWidgets();
    for (QWidget *widget : widgets) {
        const QString role = widget->property("mztlzButtonRole").toString();
        if (role.isEmpty()) {
            continue;
        }
        if (auto *button = qobject_cast<ElaPushButton *>(widget)) {
            if (role == QLatin1String("primary")) {
                applyPrimaryButtonStyle(button);
            } else if (role == QLatin1String("secondary")) {
                applySecondaryButtonStyle(button);
            }
        }
    }
}
