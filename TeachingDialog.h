#ifndef TEACHINGDIALOG_H
#define TEACHINGDIALOG_H

#include <QDialog>

class Ros2Bridge;

class TeachingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TeachingDialog(Ros2Bridge *bridge, QWidget *parent = nullptr);
};

#endif // TEACHINGDIALOG_H
