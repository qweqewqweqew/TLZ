#ifndef BACKENDPARAMSDIALOG_H
#define BACKENDPARAMSDIALOG_H

#include <QFrame>
#include <QString>

class ElaCheckBox;
class ElaPushButton;
class QLineEdit;
class Ros2Bridge;

// 设置页：坐标补偿 + 是否下发路径到 PLC。
// 「应用」只发出 applyRequested，由设置页统一做权限校验、写文件、下发和审计。
class BackendParamsDialog : public QFrame
{
    Q_OBJECT

public:
    explicit BackendParamsDialog(Ros2Bridge *bridge, QWidget *parent = nullptr);

    static QString filePath();

    double offsetX() const;
    double offsetY() const;
    double offsetZ() const;
    bool dispatchPathsToPlc() const;
    bool isDirty() const;
    QString pendingChangeSummary() const;
    bool commitToBackend(QString *error);
    void reloadFromDisk();
    void applyFactoryDefaults();

signals:
    void dirtyChanged(bool dirty);
    void applyRequested();

private:
    QLineEdit *makeOffsetEdit(QWidget *parent);
    void loadSettings();
    bool saveSettings() const;
    void rememberDisk();

    Ros2Bridge *m_bridge{nullptr};
    double m_diskX{0.0};
    double m_diskY{0.0};
    double m_diskZ{0.0};
    bool m_diskDispatch{true};
    QLineEdit *m_offsetXEdit{nullptr};
    QLineEdit *m_offsetYEdit{nullptr};
    QLineEdit *m_offsetZEdit{nullptr};
    ElaCheckBox *m_dispatchPathsCheck{nullptr};
};

#endif // BACKENDPARAMSDIALOG_H
