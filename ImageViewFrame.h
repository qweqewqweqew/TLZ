#ifndef IMAGEVIEWFRAME_H
#define IMAGEVIEWFRAME_H

#include <QFrame>
#include <QImage>

class ImageViewFrame : public QFrame
{
    Q_OBJECT

public:
    explicit ImageViewFrame(QWidget *parent = nullptr);

    // 设置要显示的图像。空 QImage 会回到"等待图像"占位状态。
    void setImage(const QImage &image);
    void clearImage();
    bool hasImage() const { return !m_image.isNull(); }

    // 顶部覆盖层文本，比如 "frame=123 3200x3000"，为空则不绘制。
    void setOverlayText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;
    QString m_overlayText;
};

#endif // IMAGEVIEWFRAME_H
