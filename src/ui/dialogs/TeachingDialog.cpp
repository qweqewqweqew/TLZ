#include "ui/dialogs/TeachingDialog.h"

#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "bridge/Ros2Bridge.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

TeachingDialog::TeachingDialog(Ros2Bridge *bridge, QWidget *parent)
    : QFrame(parent)
{
    auto *panelLayout = new QVBoxLayout(this);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(8);
    panelLayout->addWidget(settingsSectionHeading(QStringLiteral("示教取坐标"), this));
    auto *hint = makeLabel(QStringLiteral("点击下方按钮，通知后端采集当前机械臂坐标。"),
                           QStringLiteral("sectionHint"));
    hint->setWordWrap(true);
    Theme::bindStyle(hint, []() {
        return QStringLiteral("color:%1;font-size:12px;border:none;background:transparent;")
            .arg(Theme::kTextMuted);
    });
    panelLayout->addWidget(hint);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    auto *getCoordBtn = new ElaPushButton(QStringLiteral("取坐标"), this);
    getCoordBtn->setFixedSize(96, 30);
    applyPrimaryButtonStyle(getCoordBtn);
    buttonRow->addWidget(getCoordBtn);
    buttonRow->addStretch(1);
    panelLayout->addLayout(buttonRow);

    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    connect(getCoordBtn, &ElaPushButton::clicked, this, [bridge]() {
        if (bridge) {
            bridge->publishTeachRequest();
        }
    });
}
