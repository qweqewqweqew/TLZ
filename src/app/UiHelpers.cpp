#include "app/UiHelpers.h"

#include "ElaProgressBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "app/Theme.h"

#include <QColor>
#include <QFont>
#include <QVariant>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include <cmath>

namespace {

// 主题色在 main() 里才确定，不能在静态初始化期取值。
#define kButtonDefault QColor(Theme::kAccentFill)
#define kButtonHover QColor(Theme::kAccentFill).lighter(112)
#define kButtonPress QColor(Theme::kAccentFill).darker(118)
#define kButtonText QColor(Theme::kOnAccentFill)

QString pillStyle(const QString &state)
{
    if (state == "ok") {
        return QString("background:%1;border:1px solid %2;")
            .arg(Theme::kSuccessBg, Theme::kStateSuccess);
    }
    if (state == "running") {
        return QString("background:%1;border:1px solid %2;")
            .arg(Theme::kRunningBg, Theme::kStateRunning);
    }
    if (state == "error") {
        return QString("background:%1;border:1px solid %2;")
            .arg(Theme::kErrorBg, Theme::kStateError);
    }
    return QString("background:%1;border:1px solid %2;")
        .arg(Theme::kWarningBg, Theme::kStateWarning);
}

// 状态符号（第二视觉线索）：色盲用户无法仅靠颜色区分状态，
// 因此为每种状态附加语义化符号（WCAG 1.4.1）。
QString statusDotGlyph(const QString &state)
{
    if (state == "ok") {
        return QStringLiteral("✓");
    }
    if (state == "running") {
        return QStringLiteral("⟳");
    }
    if (state == "error") {
        return QStringLiteral("✕");
    }
    return QStringLiteral("!");
}

QString statusDotColor(const QString &state)
{
    if (state == "ok") {
        return Theme::kStateSuccess;
    }
    if (state == "running") {
        return Theme::kStateRunning;
    }
    if (state == "error") {
        return Theme::kStateError;
    }
    return Theme::kStateWarning;
}

} // namespace

// 按 point size 设置字号，使文字随系统 DPI 缩放（Windows 125%/150% 下不溢出）。
// 换算：96 DPI 基准下 1px = 0.75pt。
void applyPointSize(QLabel *label, qreal pointSize)
{
    QFont font = label->font();
    font.setPointSizeF(pointSize);
    label->setFont(font);
}

QLabel *makeLabel(const QString &text, const QString &objectName)
{
    auto *label = new ElaText(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    if (objectName == "systemTitle") {
        applyPointSize(label, 18.0);
    } else if (objectName == "topTime") {
        applyPointSize(label, 13.5);
    } else if (objectName == "largeValue") {
        applyPointSize(label, 16.5);
    } else if (objectName == "imageMainText") {
        applyPointSize(label, 13.5);
    } else if (objectName == "panelTitle") {
        applyPointSize(label, 13.5);
    } else if (objectName == "sectionHint") {
        applyPointSize(label, 9.0);
    } else {
        applyPointSize(label, 10.5);
    }
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    return label;
}

QPixmap tintedIcon(const QString &resource, const QColor &color, int size, qreal devicePixelRatio)
{
    const int pixels = qMax(1, int(std::lround(size * devicePixelRatio)));
    QPixmap base = QIcon(resource).pixmap(QSize(pixels, pixels));
    if (base.isNull()) {
        return base;
    }
    QPixmap tinted(base.size());
    tinted.fill(Qt::transparent);
    QPainter painter(&tinted);
    painter.drawPixmap(0, 0, base);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    painter.end();
    tinted.setDevicePixelRatio(devicePixelRatio);
    return tinted;
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
    const auto paintDivider = [divider]() {
        divider->setStyleSheet(QString("background:%1;border:none;").arg(Theme::kDivider));
    };
    paintDivider();
    Theme::watch(divider, paintDivider);
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

    auto *dot = makeLabel(statusDotGlyph(state));
    dot->setFixedWidth(14);
    dot->setAlignment(Qt::AlignCenter);
    dot->setStyleSheet(QString("color:%1;font-size:12px;").arg(statusDotColor(state)));

    auto *label = makeLabel(text);
    label->setStyleSheet(QString("color:%1;").arg(Theme::kTextField));
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(dot);
    layout->addWidget(label);
    return status;
}

void applyPrimaryButtonStyle(ElaPushButton *button)
{
    button->setProperty("mztlzButtonRole", QStringLiteral("primary"));
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
    button->setProperty("mztlzButtonRole", QStringLiteral("secondary"));
    button->setBorderRadius(6);
    button->setLightDefaultColor(QColor(Theme::kControlBg));
    button->setDarkDefaultColor(QColor(Theme::kControlBg));
    button->setLightHoverColor(QColor(Theme::kHoverBg));
    button->setDarkHoverColor(QColor(Theme::kHoverBg));
    button->setLightPressColor(QColor(Theme::kPressedBg));
    button->setDarkPressColor(QColor(Theme::kPressedBg));
    button->setLightTextColor(QColor(Theme::kTextSecondary));
    button->setDarkTextColor(QColor(Theme::kTextSecondary));
}
