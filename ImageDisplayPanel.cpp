#include "ImageDisplayPanel.h"

#include "ElaPushButton.h"
#include "ImageViewFrame.h"
#include "UiHelpers.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

ImageDisplayPanel::ImageDisplayPanel(QWidget *parent)
    : QFrame(parent)
{
    auto *viewToolbar = new QWidget(this);
    auto *viewToolbarLayout = new QHBoxLayout(viewToolbar);
    viewToolbarLayout->setContentsMargins(0, 0, 0, 0);
    viewToolbarLayout->setSpacing(6);

    auto *view2dButton = new ElaPushButton("2D", viewToolbar);
    view2dButton->setFixedSize(66, 28);
    applyPrimaryButtonStyle(view2dButton);

    auto *view3dButton = new ElaPushButton("3D", viewToolbar);
    view3dButton->setFixedSize(66, 28);
    applySecondaryButtonStyle(view3dButton);

    viewToolbarLayout->addWidget(view2dButton);
    viewToolbarLayout->addWidget(view3dButton);

    auto *panel = createPanel("图像显示", viewToolbar, this);
    panel->setFixedWidth(780);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    auto *imageArea = new ImageViewFrame(panel);
    imageArea->setObjectName("imageArea");
    imageArea->setFixedSize(720, 600);
    imageArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    imageArea->setStyleSheet(R"(
        QFrame#imageArea {
            background: #0B1117;
            border: 1px solid #3A4755;
            border-radius: 6px;
        }
    )");

    auto *imageAreaLayout = new QVBoxLayout(imageArea);
    imageAreaLayout->setContentsMargins(18, 18, 18, 18);
    imageAreaLayout->addStretch();
    auto *imageText = makeLabel("等待图像", "imageMainText");
    imageText->setAlignment(Qt::AlignCenter);
    imageAreaLayout->addWidget(imageText);
    imageAreaLayout->addStretch();

    panelLayout->addWidget(imageArea, 0, Qt::AlignHCenter);
}
