#include "ImageDisplayPanel.h"

#include "ElaPushButton.h"
#include "ImageViewFrame.h"
#include "UiHelpers.h"

#include <QDateTime>
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

    // 2D = Range 深度图；3D 位置暂时复用为 Intensity 通道，待后续接入真正的 3D 视图。
    m_view2dButton = new ElaPushButton("2D", viewToolbar);
    m_view2dButton->setFixedSize(66, 28);
    applyPrimaryButtonStyle(m_view2dButton);

    m_view3dButton = new ElaPushButton("3D", viewToolbar);
    m_view3dButton->setFixedSize(66, 28);
    applySecondaryButtonStyle(m_view3dButton);

    viewToolbarLayout->addWidget(m_view2dButton);
    viewToolbarLayout->addWidget(m_view3dButton);

    auto *panel = createPanel("图像显示", viewToolbar, this);
    panel->setFixedWidth(780);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    m_imageArea = new ImageViewFrame(panel);
    m_imageArea->setObjectName("imageArea");
    m_imageArea->setFixedSize(720, 600);
    m_imageArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_imageArea->setStyleSheet(R"(
        QFrame#imageArea {
            background: #0B1117;
            border: 1px solid #3A4755;
            border-radius: 6px;
        }
    )");

    panelLayout->addWidget(m_imageArea, 0, Qt::AlignHCenter);

    connect(m_view2dButton, &ElaPushButton::clicked, this, [this]() {
        setViewMode(ViewMode::Range2D);
    });
    connect(m_view3dButton, &ElaPushButton::clicked, this, [this]() {
        setViewMode(ViewMode::Intensity);
    });
}

void ImageDisplayPanel::updateScanFrame(const QImage &range,
                                        const QImage &intensity,
                                        quint64 frameId,
                                        quint64 timestampNs)
{
    m_rangeImage = range;
    m_intensityImage = intensity;
    m_lastFrameId = frameId;
    m_lastTimestampNs = timestampNs;

    // 如果当前 mode 的那一路没有数据，就自动切到有数据的另一路，避免用户看到空。
    if (m_mode == ViewMode::Range2D && m_rangeImage.isNull() && !m_intensityImage.isNull()) {
        setViewMode(ViewMode::Intensity);
        return;
    }
    if (m_mode == ViewMode::Intensity && m_intensityImage.isNull() && !m_rangeImage.isNull()) {
        setViewMode(ViewMode::Range2D);
        return;
    }
    refreshView();
}

void ImageDisplayPanel::setViewMode(ViewMode mode)
{
    m_mode = mode;
    if (mode == ViewMode::Range2D) {
        applyPrimaryButtonStyle(m_view2dButton);
        applySecondaryButtonStyle(m_view3dButton);
    } else {
        applySecondaryButtonStyle(m_view2dButton);
        applyPrimaryButtonStyle(m_view3dButton);
    }
    refreshView();
}

void ImageDisplayPanel::refreshView()
{
    const QImage &shown = (m_mode == ViewMode::Range2D) ? m_rangeImage : m_intensityImage;
    m_imageArea->setImage(shown);

    if (shown.isNull()) {
        m_imageArea->setOverlayText(QString());
        return;
    }

    const QString kind = (m_mode == ViewMode::Range2D) ? QStringLiteral("Range")
                                                       : QStringLiteral("Intensity");
    QString overlay = QString("%1  frame=%2  %3x%4")
                          .arg(kind)
                          .arg(m_lastFrameId)
                          .arg(shown.width())
                          .arg(shown.height());
    if (m_lastTimestampNs != 0) {
        const qint64 ms = qint64(m_lastTimestampNs / 1000000ULL);
        const QString ts = QDateTime::fromMSecsSinceEpoch(ms).toString("HH:mm:ss.zzz");
        overlay += QString("  %1").arg(ts);
    }
    m_imageArea->setOverlayText(overlay);
}
