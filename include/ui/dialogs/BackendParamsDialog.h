#ifndef BACKENDPARAMSDIALOG_H
#define BACKENDPARAMSDIALOG_H

#include <QFrame>

class ElaCheckBox;
class ElaPushButton;
class QLineEdit;
class Ros2Bridge;

// 设置页：坐标补偿 + 是否下发路径到 PLC。
// 点「应用」后通过 ROS topic 写入后端，并持久化到 backend_params.ini。
class BackendParamsDialog : public QFrame
{
    Q_OBJECT

public:
    explicit BackendParamsDialog(Ros2Bridge *bridge, QWidget *parent = nullptr);

    double offsetX() const;
    double offsetY() const;
    double offsetZ() const;
    bool dispatchPathsToPlc() const;

private:
    QLineEdit *makeOffsetEdit(QWidget *parent);
    void applyParams();
    void loadSettings();
    void saveSettings() const;

    Ros2Bridge *m_bridge{nullptr};
    QLineEdit *m_offsetXEdit{nullptr};
    QLineEdit *m_offsetYEdit{nullptr};
    QLineEdit *m_offsetZEdit{nullptr};
    ElaCheckBox *m_dispatchPathsCheck{nullptr};
};

#endif // BACKENDPARAMSDIALOG_H
