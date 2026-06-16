#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QFrame;
class QLabel;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void buildMainView();
    QFrame *createPanel(const QString &title);
    QLabel *createStatusPill(const QString &text, const QString &state);
    QWidget *createStepRow(const QString &name, const QString &stateText, const QString &state);
    QWidget *createMetricRow(const QString &name, const QString &value, const QString &unit = QString());

    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
