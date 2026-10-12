#include "app/AppStyle.h"

#include "app/Theme.h"

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
            color: @TEXT_PRIMARY@;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#systemTitle {
            color: @TEXT_PRIMARY@;
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
            color: @REALTIME@;
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
    return style;
}
