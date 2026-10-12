#include "ui/dialogs/TeachingDialog.h"

#include "ElaPushButton.h"
#include "app/UiHelpers.h"
#include "bridge/Ros2Bridge.h"

#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

TeachingDialog::TeachingDialog(Ros2Bridge *bridge, QWidget *parent)
    : QFrame(parent)
{
    auto *panel = createPanel(QStringLiteral("示教模块"), nullptr, this);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    setMinimumWidth(360);
    setMaximumWidth(520);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    auto *hint = makeLabel(QStringLiteral("点击下方按钮，通知后端采集当前机械臂坐标。"),
                           QStringLiteral("sectionHint"));
    hint->setWordWrap(true);
    panelLayout->addWidget(hint);

    auto *getCoordBtn = new ElaPushButton(QStringLiteral("取坐标"), panel);
    getCoordBtn->setFixedSize(120, 36);
    applyPrimaryButtonStyle(getCoordBtn);
    panelLayout->addWidget(getCoordBtn, 0, Qt::AlignLeft);

    connect(getCoordBtn, &ElaPushButton::clicked, this, [bridge]() {
        if (bridge) {
            bridge->publishTeachRequest();
        }
    });
}
