#include "TeachingDialog.h"

#include "AppStyle.h"
#include "ElaPushButton.h"
#include "Ros2Bridge.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

TeachingDialog::TeachingDialog(Ros2Bridge *bridge, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("示教模块");
    setMinimumWidth(360);
    setStyleSheet(mainWindowStyleSheet() + R"(
        QDialog {
            background: #16202B;
        }
    )");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto *hint = new QLabel("点击下方按钮，通知后端采集当前机械臂坐标。", this);
    hint->setObjectName("sectionHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("border: none; border-top: 1px solid #2E3E4E;");
    layout->addWidget(sep);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto *getCoordBtn = new ElaPushButton("取坐标", this);
    getCoordBtn->setFixedSize(120, 36);
    btnLayout->addWidget(getCoordBtn);

    layout->addLayout(btnLayout);

    connect(getCoordBtn, &ElaPushButton::clicked, this, [bridge]() {
        if (bridge) {
            bridge->publishTeachRequest();
        }
    });
}
