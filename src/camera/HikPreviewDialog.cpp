#include "camera/HikPreviewDialog.h"

#include "app/Theme.h"
#include "camera/HikLiveView.h"

#include <QCloseEvent>
#include <QVBoxLayout>

HikPreviewDialog::HikPreviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("海康相机实时预览"));
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                   Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setMinimumSize(640, 360);
    resize(960, 540);
    setModal(false);
    setStyleSheet(QStringLiteral("QDialog{background:%1;}").arg(Theme::kWindowBg));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);
    m_liveView = new HikLiveView(this);
    m_liveView->setExpandButtonVisible(false);
    layout->addWidget(m_liveView, 1);
}

WId HikPreviewDialog::videoWindowId()
{
    return m_liveView ? m_liveView->videoWindowId() : WId();
}

void HikPreviewDialog::setConnectionStatus(bool connected, const QString &message)
{
    if (m_liveView) {
        m_liveView->setConnectionStatus(connected, message);
    }
}

void HikPreviewDialog::closeEvent(QCloseEvent *event)
{
    emit closed();
    QDialog::closeEvent(event);
}
