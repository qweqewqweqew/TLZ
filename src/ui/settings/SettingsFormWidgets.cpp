#include "ui/settings/SettingsFormWidgets.h"

#include "ElaCheckBox.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QString fieldStyle()
{
    return QStringLiteral(
               "QLineEdit, QSpinBox {"
               "  background: %1;"
               "  color: %2;"
               "  border: 1px solid %3;"
               "  border-radius: 6px;"
               "  padding: 0 10px;"
               "  min-height: 32px;"
               "  font-size: 14px;"
               "  selection-background-color: %4;"
               "}"
               "QLineEdit:focus, QSpinBox:focus { border: 1px solid %5; }"
               "QLineEdit:read-only, QLineEdit:disabled, QSpinBox:disabled {"
               "  color: %6;"
               "  background: %7;"
               "}"
               "QSpinBox::up-button, QSpinBox::down-button {"
               "  width: 16px;"
               "  background: %8;"
               "  border: none;"
               "}")
        .arg(Theme::kControlBg,
             Theme::kTextPrimary,
             Theme::kBorder,
             Theme::kSelectionBg,
             Theme::kAccent,
             Theme::kTextMuted,
             Theme::kPanelBg,
             Theme::kHoverBg);
}

} // namespace

QLabel *settingsHint(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, QStringLiteral("sectionHint"));
    label->setParent(parent);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    label->setStyleSheet(QStringLiteral(
                             "color:%1;font-size:13px;border:none;background:transparent;")
                             .arg(Theme::kTextMuted));
    return label;
}

QLabel *settingsFieldLabel(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, QStringLiteral("metricName"));
    label->setParent(parent);
    label->setStyleSheet(QStringLiteral(
                             "color:%1;font-size:14px;border:none;background:transparent;")
                             .arg(Theme::kTextSecondary));
    return label;
}

QLabel *settingsSourceBanner(const QString &text, QWidget *parent)
{
    auto *label = settingsHint(text, parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setStyleSheet(QStringLiteral(
                             "color:%1;font-size:12px;background:transparent;"
                             "border:none;border-left:3px solid %2;padding:4px 10px;")
                             .arg(Theme::kTextMuted, Theme::kBorder));
    return label;
}

QLineEdit *settingsLineEdit(QWidget *parent)
{
    auto *edit = new QLineEdit(parent);
    edit->setMaxLength(256);
    edit->setMaximumWidth(460);
    edit->setStyleSheet(fieldStyle());
    return edit;
}

QSpinBox *settingsSpinBox(QWidget *parent)
{
    auto *spin = new QSpinBox(parent);
    spin->setMaximumWidth(180);
    spin->setStyleSheet(fieldStyle());
    return spin;
}

QPlainTextEdit *settingsReadOnlyText(QWidget *parent)
{
    auto *edit = new QPlainTextEdit(parent);
    edit->setReadOnly(true);
    edit->setMinimumHeight(140);
    edit->setStyleSheet(QStringLiteral(
                            "QPlainTextEdit {"
                            "  background: %1;"
                            "  color: %2;"
                            "  border: 1px solid %3;"
                            "  border-radius: 6px;"
                            "  padding: 8px;"
                            "  font-size: 13px;"
                            "}")
                            .arg(Theme::kLogBg, Theme::kTextSecondary, Theme::kDivider));
    return edit;
}

ElaCheckBox *settingsCheckBox(const QString &text, QWidget *parent)
{
    auto *box = new ElaCheckBox(text, parent);
    box->setStyleSheet(QStringLiteral(
                           "color:%1;font-size:14px;border:none;background:transparent;")
                           .arg(Theme::kTextPrimary));
    return box;
}

QWidget *settingsPendingBlock(const QString &title, const QString &note, QWidget *parent)
{
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("settingsPendingRow"));
    row->setStyleSheet(QStringLiteral(
                           "QWidget#settingsPendingRow { background:%1; border:1px solid %2;"
                           " border-radius:6px; }")
                           .arg(Theme::kControlBg, Theme::kDivider));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

    auto *name = settingsFieldLabel(title, row);
    name->setFixedWidth(150);
    layout->addWidget(name, 0, Qt::AlignVCenter);

    auto *badge = makeLabel(QStringLiteral("待对接"));
    badge->setParent(row);
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedWidth(64);
    badge->setStyleSheet(QStringLiteral(
                             "color:%1;background:%2;border:none;border-radius:4px;"
                             "font-size:12px;padding:2px 0;")
                             .arg(Theme::kStateWarning, Theme::kWarningBg));
    layout->addWidget(badge, 0, Qt::AlignVCenter);

    layout->addWidget(settingsHint(note, row), 1);
    return row;
}

QFormLayout *settingsFormLayout(QWidget *host)
{
    auto *form = new QFormLayout(host);
    form->setContentsMargins(0, 4, 0, 4);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

void addSettingsField(QFormLayout *form,
                      const QString &label,
                      QWidget *field,
                      const QString &note,
                      QWidget *parent)
{
    auto *name = settingsFieldLabel(label, parent);
    name->setMinimumWidth(120);
    name->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addRow(name, field);
    if (!note.isEmpty()) {
        auto *spacer = new QLabel(parent);
        spacer->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
        form->addRow(spacer, settingsHint(note, parent));
    }
}

QTabWidget *settingsTabs(QWidget *parent)
{
    auto *tabs = new QTabWidget(parent);
    tabs->setObjectName(QStringLiteral("settingsTabs"));
    tabs->setDocumentMode(true);
    tabs->tabBar()->setDrawBase(false);
    tabs->tabBar()->setExpanding(false);
    tabs->setStyleSheet(QStringLiteral(
                            "QTabWidget#settingsTabs::pane { border:none; top:0px; }"
                            "QTabWidget#settingsTabs > QStackedWidget { background:transparent; }"
                            "QTabWidget#settingsTabs QTabBar::tab {"
                            "  background:%1; color:%2; border:1px solid %3; border-radius:6px;"
                            "  padding:7px 20px; margin:0 8px 10px 0; min-width:84px; font-size:14px;"
                            "}"
                            "QTabWidget#settingsTabs QTabBar::tab:selected {"
                            "  background:%4; color:%5; border-color:%5;"
                            "}"
                            "QTabWidget#settingsTabs QTabBar::tab:hover:!selected { background:%6; }")
                            .arg(Theme::kPanelBg,
                                 Theme::kTextSecondary,
                                 Theme::kBorder,
                                 Theme::kRunningBg,
                                 Theme::kAccent,
                                 Theme::kHoverBg));
    return tabs;
}

QVBoxLayout *addSettingsTab(QTabWidget *tabs, const QString &title, bool scrollable)
{
    auto *content = new QWidget();
    content->setObjectName(QStringLiteral("settingsTabContent"));
    content->setStyleSheet(QStringLiteral("QWidget#settingsTabContent { background:transparent; }"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, scrollable ? 8 : 0, 0);
    layout->setSpacing(12);
    if (!scrollable) {
        tabs->addTab(content, title);
        return layout;
    }

    auto *scroll = new QScrollArea();
    scroll->setObjectName(QStringLiteral("settingsTabScroll"));
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea#settingsTabScroll { background:transparent; border:none; }"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(content);
    tabs->addTab(scroll, title);
    return layout;
}
