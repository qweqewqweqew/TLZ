#include "AppStyle.h"

QString mainWindowStyleSheet(int panelRadius)
{
    return QString(R"(
        QMainWindow {
            background: #16202B;
        }
        QWidget {
            font-family: "Microsoft YaHei";
            font-size: 14px;
            color: #E4EAF0;
        }
        QFrame#titleBar {
            background: #0E151D;
            border: 1px solid #283746;
            border-radius: 8px;
        }
        QFrame#controlBar {
            background: #182431;
            border: 1px solid #2E3E4E;
            border-radius: 8px;
        }
        QFrame#panel {
            background: #202B37;
            border: 1px solid #334353;
            border-radius: %1px;
        }
        QLabel#panelTitle {
            color: #F1F5F9;
            font-size: 15px;
            font-weight: 600;
        }
        QLabel#systemTitle {
            color: #F1F5F9;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#topTime {
            color: #DDE7F0;
            font-size: 16px;
            font-weight: 600;
        }
        QLabel#sectionHint {
            color: #8A9AA8;
            font-size: 12px;
        }
        QLabel#largeValue {
            color: #E6EEF5;
            font-size: 22px;
            font-weight: 600;
        }
        QLabel#metricName {
            color: #8A9AA8;
        }
        QLabel#metricValue {
            color: #E6EEF5;
            font-weight: 600;
        }
        QLabel#imageMainText {
            color: #5A6A78;
            font-size: 18px;
            font-weight: 600;
        }
        ElaComboBox {
            background: #101923;
            color: #E6EEF5;
            border: 1px solid #334353;
            border-radius: 5px;
            padding-left: 8px;
        }
        ElaPlainTextEdit {
            background: #17212C;
            color: #AABBCC;
            border: 1px solid #303C49;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #2D6F9F;
        }
        QStatusBar {
            background: #111922;
            color: #8A9AA8;
            border-top: 1px solid #2E3E4E;
        }
    )").arg(panelRadius);
}
