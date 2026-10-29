#include "ui/settings/CraftPlaceholder.h"

#include "app/Theme.h"

#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

QString craftPageStyle()
{
    return QStringLiteral(
               "QWidget#craftPage, QScrollArea#craftScroll,"
               "QWidget#craftViewport, QWidget#craftContent { background:%12; }"
               "QScrollArea#craftScroll { border:none; }"
               "QFrame#craftCard { background:%1; border:1px solid %2; border-radius:10px; }"
               "QLabel#craftTitle { color:%3; font-size:15px; font-weight:600; background:transparent; border:none; }"
               "QLabel#craftSub { color:%4; font-size:12px; background:transparent; border:none; }"
               "QLabel#craftLabel { color:%5; font-size:13px; background:transparent; border:none; }"
               "QLabel#craftUnit { color:%4; font-size:12px; background:transparent; border:none; }"
               "QLabel#craftNote { color:%4; font-size:12px; background:transparent; border:none; }"
               "QLabel#craftInfo { color:%11; background:%7; border:1px solid %8; border-radius:6px; padding:8px; }"
               "QLabel#craftWarning { color:%14; background:%13; border:1px solid %14; border-radius:6px; padding:9px; }"
               "QLabel#craftRapidLabel { color:%11; font-size:12px; font-weight:600; background:transparent; border:none; }"
               "QLabel#craftCuttingLabel { color:%16; font-size:12px; font-weight:600; background:transparent; border:none; }"
               "QSpinBox, QDoubleSpinBox {"
               "  background:%6; color:%3; border:1px solid %2; border-radius:6px;"
               "  padding:2px 28px 2px 10px; min-height:34px; font-size:13px;"
               "  selection-background-color:%7; selection-color:%3; }"
               "QSpinBox:hover, QDoubleSpinBox:hover { border-color:%4; }"
               "QSpinBox:focus, QDoubleSpinBox:focus { border:1px solid %8; }"
               "QSpinBox:disabled, QDoubleSpinBox:disabled { background:%9; color:%4; }"
               "QSpinBox#craftEmphasis, QDoubleSpinBox#craftEmphasis {"
               "  background:%7; color:%11; border:1px solid %8; font-weight:600; }"
               "QSpinBox#craftRapidValue, QDoubleSpinBox#craftRapidValue { color:%11; font-weight:600; }"
               "QSpinBox#craftCuttingValue, QDoubleSpinBox#craftCuttingValue { color:%16; font-weight:600; }"
               "QSpinBox::up-button, QSpinBox::down-button,"
               "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
               "  width:20px; background:transparent; border:none; border-left:1px solid %17; }"
               "QSpinBox::up-button:hover, QSpinBox::down-button:hover,"
               "QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover { background:%9; }"
               "QSpinBox::up-button:pressed, QSpinBox::down-button:pressed,"
               "QDoubleSpinBox::up-button:pressed, QDoubleSpinBox::down-button:pressed { background:%10; }"
               "QComboBox { background:%6; color:%3; border:1px solid %2; border-radius:6px;"
               "  padding:2px 10px; min-height:34px; font-size:13px; }"
               "QComboBox:hover, QComboBox:focus { border-color:%8; }"
               "QComboBox::drop-down { width:30px; border:none; border-left:1px solid %17; }"
               "QComboBox QAbstractItemView { background:%1; color:%3; border:1px solid %2;"
               "  selection-background-color:%7; selection-color:%3; outline:none; }"
               "QPushButton#craftSeg {"
               "  background:%6; color:%5; border:1px solid %2; border-radius:6px;"
               "  padding:6px 12px; min-height:36px; font-size:13px; }"
               "QPushButton#craftSeg:hover:!checked { background:%9; color:%3; border-color:%4; }"
               "QPushButton#craftSeg:checked { background:%7; color:%11; border-color:%8; font-weight:600; }"
               "QPushButton#craftSeg:pressed { background:%10; }"
               "QPushButton#craftWarningButton { background:%13; color:%14; border:1px solid %14;"
               "  border-radius:6px; padding:8px 12px; min-height:36px; font-size:13px; }"
               "QPushButton#craftWarningButton:hover:!disabled { background:%7; color:%11; border-color:%8; }"
               "QPushButton#craftWarningButton:disabled { background:%9; color:%4; border-color:%2; }"
               "QFrame#craftPreview { background:%12; border:1px solid %17; border-radius:7px; }"
               "QLabel#craftPreviewText { color:%4; background:transparent; border:none; font-size:13px; }"
               "QFrame#craftMatrix { background:%12; border:1px solid %17; border-radius:7px; }"
               "QLabel#craftMatrixCell { color:%3; background:%6; border:1px solid %17;"
               "  border-radius:4px; padding:8px; font-weight:600; }"
               "QLabel#craftTranslationCell { color:%16; background:%15; border:1px solid %16;"
               "  border-radius:4px; padding:8px; font-weight:600; }"
               "QTabWidget#craftTabs::pane { border:none; background:%12; }"
               "QTabWidget#craftTabs QTabBar::tab { background:%6; color:%5; border:1px solid %2;"
               "  border-radius:6px; padding:8px 22px; margin-right:6px; min-width:108px; font-size:13px; }"
               "QTabWidget#craftTabs QTabBar::tab:selected { background:%7; color:%11; border-color:%8; font-weight:600; }"
               "QTabWidget#craftTabs QTabBar::tab:hover:!selected { background:%9; color:%3; }"
               "QFrame#craftReadOnlyRow { background:%6; border:1px solid %2; border-radius:7px; }"
               "QLabel#craftReadOnlyValue { color:%4; background:%9; border:1px solid %2; border-radius:5px;"
               "  padding:4px 10px; font-size:12px; font-weight:600; }"
               "QLabel#craftAlgorithmState { color:%11; background:%7; border:1px solid %8; border-radius:6px;"
               "  padding:9px 12px; font-size:12px; }"
               "QTableWidget#craftTable { background:%1; alternate-background-color:%6; color:%3;"
               "  border:1px solid %2; border-radius:6px; gridline-color:%17; font-size:12px; outline:none;"
               "  selection-background-color:%7; selection-color:%3; }"
               "QTableWidget#craftTable::item { padding:4px; border:none; }"
               "QHeaderView::section { background:%6; color:%5; border:none; border-bottom:1px solid %2;"
               "  padding:7px 4px; font-weight:600; }")
        .arg(QLatin1String(Theme::kPanelBg))
        .arg(QLatin1String(Theme::kBorder))
        .arg(QLatin1String(Theme::kTextPrimary))
        .arg(QLatin1String(Theme::kTextMuted))
        .arg(QLatin1String(Theme::kTextSecondary))
        .arg(QLatin1String(Theme::kControlBg))
        .arg(QLatin1String(Theme::kSelectionBg))
        .arg(QLatin1String(Theme::kFocusRing))
        .arg(QLatin1String(Theme::kHoverBg))
        .arg(QLatin1String(Theme::kPressedBg))
        .arg(QLatin1String(Theme::kAccent))
        .arg(QLatin1String(Theme::kCanvasBg))
        .arg(QLatin1String(Theme::kWarningBg))
        .arg(QLatin1String(Theme::kStateWarning))
        .arg(QLatin1String(Theme::kSuccessBg))
        .arg(QLatin1String(Theme::kStateSuccess))
        .arg(QLatin1String(Theme::kDivider));
}

QVBoxLayout *craftCard(QWidget *parent, QVBoxLayout *column, const QString &title, const QString &subtitle)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("craftCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);
    auto *heading = new QLabel(title, card);
    heading->setObjectName(QStringLiteral("craftTitle"));
    heading->setWordWrap(true);
    layout->addWidget(heading);
    if (!subtitle.isEmpty()) {
        auto *sub = new QLabel(subtitle, card);
        sub->setObjectName(QStringLiteral("craftSub"));
        sub->setWordWrap(true);
        layout->addWidget(sub);
    }
    column->addWidget(card);
    return layout;
}

QSpinBox *craftInt(QWidget *parent, int value, int minValue, int maxValue)
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(minValue, maxValue);
    spin->setValue(value);
    spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    return spin;
}

QDoubleSpinBox *craftDouble(QWidget *parent, double value, int decimals, double minValue, double maxValue, double step)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setDecimals(decimals);
    spin->setRange(minValue, maxValue);
    spin->setSingleStep(step);
    spin->setValue(value);
    spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    return spin;
}

QWidget *craftRow(QWidget *parent, const QString &label, QWidget *editor, const QString &unit)
{
    auto *row = new QWidget(parent);
    row->setMinimumHeight(38);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    auto *name = new QLabel(label, row);
    name->setObjectName(QStringLiteral("craftLabel"));
    name->setMinimumWidth(132);
    editor->setParent(row);
    layout->addWidget(name);
    layout->addWidget(editor, 1);
    if (!unit.isEmpty()) {
        auto *suffix = new QLabel(unit, row);
        suffix->setObjectName(QStringLiteral("craftUnit"));
        suffix->setMinimumWidth(42);
        layout->addWidget(suffix);
    }
    return row;
}

QButtonGroup *craftSegments(QWidget *parent, QVBoxLayout *body, const QStringList &labels, int selected)
{
    auto *host = new QWidget(parent);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto *group = new QButtonGroup(host);
    group->setExclusive(true);
    for (int i = 0; i < labels.size(); ++i) {
        auto *button = new QPushButton(labels.at(i), host);
        button->setObjectName(QStringLiteral("craftSeg"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setChecked(i == selected);
        group->addButton(button, i);
        layout->addWidget(button, 1);
    }
    body->addWidget(host);
    return group;
}

QLabel *craftNote(QWidget *parent, const QString &text)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("craftNote"));
    label->setWordWrap(true);
    return label;
}

QFrame *craftReadOnlyRow(QWidget *parent,
                         const QString &label,
                         const QString &description,
                         const QString &jsonKey,
                         const QString &unit)
{
    auto *row = new QFrame(parent);
    row->setObjectName(QStringLiteral("craftReadOnlyRow"));
    row->setMinimumHeight(58);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 8, 10, 8);
    layout->setSpacing(12);

    auto *textColumn = new QVBoxLayout();
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(3);
    auto *name = new QLabel(label, row);
    name->setObjectName(QStringLiteral("craftLabel"));
    textColumn->addWidget(name);
    auto *note = craftNote(row, description);
    textColumn->addWidget(note);
    layout->addLayout(textColumn, 1);

    auto *valueColumn = new QVBoxLayout();
    valueColumn->setContentsMargins(0, 0, 0, 0);
    valueColumn->setSpacing(2);
    auto *value = new QLabel(QStringLiteral("未获取"), row);
    value->setObjectName(QStringLiteral("craftReadOnlyValue"));
    value->setAlignment(Qt::AlignCenter);
    value->setMinimumWidth(78);
    value->setProperty("algorithmJsonKey", jsonKey);
    value->setToolTip(QStringLiteral("program_param.json → %1").arg(jsonKey));
    valueColumn->addWidget(value, 0, Qt::AlignRight);
    if (!unit.isEmpty()) {
        auto *unitLabel = new QLabel(unit, row);
        unitLabel->setObjectName(QStringLiteral("craftUnit"));
        unitLabel->setAlignment(Qt::AlignCenter);
        valueColumn->addWidget(unitLabel, 0, Qt::AlignRight);
    }
    layout->addLayout(valueColumn);
    return row;
}
