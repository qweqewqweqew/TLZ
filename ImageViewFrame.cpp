#include "ImageViewFrame.h"

#include <QFont>
#include <QPainter>
#include <QRect>

ImageViewFrame::ImageViewFrame(QWidget *parent)
    : QFrame(parent)
{
}

void ImageViewFrame::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void ImageViewFrame::clearImage()
{
    m_image = QImage();
    update();
}

void ImageViewFrame::setOverlayText(const QString &text)
{
    if (m_overlayText == text) {
        return;
    }
    m_overlayText = text;
    update();
}

void ImageViewFrame::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect area = rect().adjusted(1, 1, -2, -2);
    painter.fillRect(area, QColor("#0B1117"));

    if (m_image.isNull()) {
        // 占位：网格 + 中心十字 + 文本
        QPen gridPen(QColor(60, 80, 96, 70));
        gridPen.setWidth(1);
        painter.setPen(gridPen);

        constexpr int gridSize = 48;
        for (int x = area.left() + gridSize; x < area.right(); x += gridSize) {
            painter.drawLine(x, area.top(), x, area.bottom());
        }
        for (int y = area.top() + gridSize; y < area.bottom(); y += gridSize) {
            painter.drawLine(area.left(), y, area.right(), y);
        }

        QPen centerPen(QColor(0, 229, 255, 55));
        centerPen.setWidth(1);
        painter.setPen(centerPen);
        painter.drawLine(area.center().x(), area.top(), area.center().x(), area.bottom());
        painter.drawLine(area.left(), area.center().y(), area.right(), area.center().y());
        return;
    }

    // 保持纵横比缩放到可用区域中心。
    const QSize imgSize = m_image.size();
    const QSize target = imgSize.scaled(area.size(), Qt::KeepAspectRatio);
    const QRect dstRect(area.x() + (area.width() - target.width()) / 2,
                        area.y() + (area.height() - target.height()) / 2,
                        target.width(),
                        target.height());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(dstRect, m_image);

    if (!m_overlayText.isEmpty()) {
        QFont f = painter.font();
        f.setPixelSize(12);
        painter.setFont(f);
        const QRect textArea = area.adjusted(8, 6, -8, -6);
        painter.setPen(QColor(0, 0, 0, 160));
        painter.drawText(textArea.adjusted(1, 1, 1, 1), Qt::AlignLeft | Qt::AlignTop, m_overlayText);
        painter.setPen(QColor("#B8C8D8"));
        painter.drawText(textArea, Qt::AlignLeft | Qt::AlignTop, m_overlayText);
    }
}
