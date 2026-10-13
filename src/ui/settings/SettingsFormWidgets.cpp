#include "ui/settings/SettingsFormWidgets.h"

#include "ElaCheckBox.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QSpinBox>
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
                             "color:%1;font-size:12px;border:none;background:transparent;")
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
                             "color:%1;font-size:12px;border:none;background:%2;"
                             "border-radius:6px;padding:8px;")
                             .arg(Theme::kTextSecondary, Theme::kPanelBg));
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
    auto *wrap = new QWidget(parent);
    auto *layout = new QVBoxLayout(wrap);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(settingsFieldLabel(title, wrap));

    auto *edit = settingsLineEdit(wrap);
    edit->setReadOnly(true);
    edit->setEnabled(false);
    edit->setText(QStringLiteral("待对接"));
    edit->setToolTip(note);
    layout->addWidget(edit);
    layout->addWidget(settingsHint(note, wrap));
    return wrap;
}

void addSettingsField(QFormLayout *form,
                      const QString &label,
                      QWidget *field,
                      const QString &note,
                      QWidget *parent)
{
    form->addRow(settingsFieldLabel(label, parent), field);
    if (!note.isEmpty()) {
        form->addRow(settingsHint(note, parent));
    }
}
