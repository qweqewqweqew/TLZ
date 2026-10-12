#ifndef TEACHINGDIALOG_H
#define TEACHINGDIALOG_H

#include <QFrame>

class Ros2Bridge;

class TeachingDialog : public QFrame
{
    Q_OBJECT

public:
    explicit TeachingDialog(Ros2Bridge *bridge, QWidget *parent = nullptr);
};

#endif // TEACHINGDIALOG_H
