#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>

class QFrame;
class QLabel;
class QToolButton;
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
    bool eventFilter(QObject *watched, QEvent *event) override;

    void buildMainView();
    QFrame *createPanel(const QString &title);
    QWidget *createStatusPill(const QString &text, const QString &state);
    QWidget *createStepRow(const QString &name, const QString &stateText, const QString &state);
    QWidget *createMetricRow(const QString &name, const QString &value, const QString &unit = QString());
    void updateMaximizeButtonIcon();

    Ui::MainWindow *ui;
    QWidget *m_titleDragArea{nullptr};
    QToolButton *m_maximizeButton{nullptr};
    QPoint m_dragPosition;
    bool m_dragging{false};
};
#endif // MAINWINDOW_H
