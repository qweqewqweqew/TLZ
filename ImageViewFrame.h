#ifndef IMAGEVIEWFRAME_H
#define IMAGEVIEWFRAME_H

#include <QFrame>

class ImageViewFrame : public QFrame
{
    Q_OBJECT

public:
    explicit ImageViewFrame(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // IMAGEVIEWFRAME_H
