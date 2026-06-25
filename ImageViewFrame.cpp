#include "ImageViewFrame.h"

#include <QPainter>

ImageViewFrame::ImageViewFrame(QWidget *parent)
    : QFrame(parent)
{
}

void ImageViewFrame::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect area = rect().adjusted(1, 1, -2, -2);
    painter.fillRect(area, QColor("#0B1117"));

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
}
