#include "UiHelpers.h"

#include "ElaProgressBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

const QColor kButtonDefault("#2D6F9F");
const QColor kButtonHover("#377FAF");
const QColor kButtonPress("#245B83");
const QColor kButtonText("#F7FBFF");

QString pillStyle(const QString &state)
{
    if (state == "ok") {
        return "background:#142A24;border:1px solid #255A46;";
    }
    if (state == "running") {
        return "background:#122B3D;border:1px solid #275D81;";
    }
    if (state == "error") {
        return "background:#301D23;border:1px solid #70313B;";
    }
    return "background:#2C2919;border:1px solid #5E5024;";
}

QString statusDotColor(const QString &state)
{
    if (state == "ok") {
        return "#2ECC71";
    }
    if (state == "running") {
        return "#3498DB";
    }
    if (state == "error") {
        return "#E74C3C";
    }
    return "#F1C40F";
}

} // namespace

QLabel *makeLabel(const QString &text, const QString &objectName)
{
    auto *label = new ElaText(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    if (objectName == "systemTitle") {
        label->setTextPixelSize(22);
    } else if (objectName == "topTime") {
        label->setTextPixelSize(16);
    } else if (objectName == "largeValue") {
        label->setTextPixelSize(22);
    } else if (objectName == "imageMainText") {
        label->setTextPixelSize(18);
    } else if (objectName == "panelTitle") {
        label->setTextPixelSize(16);
    } else if (objectName == "sectionHint") {
        label->setTextPixelSize(12);
    } else {
        label->setTextPixelSize(14);
    }
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    return label;
}

QFrame *createPanel(const QString &title, QWidget *headerWidget, QWidget *parent)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName("panel");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    auto *titleRow = new QHBoxLayout();
    auto *titleLabel = makeLabel(title, "panelTitle");
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    if (headerWidget) {
        headerWidget->setParent(panel);
        titleRow->addWidget(headerWidget);
    }
    layout->addLayout(titleRow);

    auto *divider = new QFrame(panel);
    divider->setFixedHeight(1);
    divider->setStyleSheet("background:#303C49;border:none;");
    layout->addWidget(divider);

    return panel;
}

QWidget *createStatusPill(const QString &text, const QString &state, QWidget *parent)
{
    auto *status = new QWidget(parent);
    status->setMinimumHeight(28);
    status->setStyleSheet(QString("border-radius:4px;%1").arg(pillStyle(state)));

    auto *layout = new QHBoxLayout(status);
    layout->setContentsMargins(8, 3, 10, 3);
    layout->setSpacing(6);

    auto *dot = makeLabel(QString(QChar(0x25CF)));
    dot->setFixedWidth(10);
    dot->setAlignment(Qt::AlignCenter);
    dot->setStyleSheet(QString("color:%1;font-size:12px;").arg(statusDotColor(state)));

    auto *label = makeLabel(text);
    label->setStyleSheet("color:#AABBCC;");
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(dot);
    layout->addWidget(label);
    return status;
}

void applyPrimaryButtonStyle(ElaPushButton *button)
{
    button->setBorderRadius(6);
    button->setLightDefaultColor(kButtonDefault);
    button->setDarkDefaultColor(kButtonDefault);
    button->setLightHoverColor(kButtonHover);
    button->setDarkHoverColor(kButtonHover);
    button->setLightPressColor(kButtonPress);
    button->setDarkPressColor(kButtonPress);
    button->setLightTextColor(kButtonText);
    button->setDarkTextColor(kButtonText);
}

void applySecondaryButtonStyle(ElaPushButton *button)
{
    button->setBorderRadius(6);
    button->setLightDefaultColor(QColor("#15202B"));
    button->setDarkDefaultColor(QColor("#15202B"));
    button->setLightHoverColor(QColor("#203040"));
    button->setDarkHoverColor(QColor("#203040"));
    button->setLightPressColor(QColor("#253A4E"));
    button->setDarkPressColor(QColor("#253A4E"));
    button->setLightTextColor(QColor("#B8C8D8"));
    button->setDarkTextColor(QColor("#B8C8D8"));
}
