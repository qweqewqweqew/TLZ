#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include "qcustomplot.h"

class QFrame;
class QLabel;
class QWidget;
class QTimer;
class ElaIconButton;
class ElaPlainTextEdit;

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
    QFrame *createPanel(const QString &title, QWidget *headerWidget = nullptr);
    QWidget *createStatusPill(const QString &text, const QString &state);
    QWidget *createStepRow(const QString &name, const QString &stateText, const QString &state);
    QWidget *createMetricRow(const QString &name, const QString &value, const QString &unit = QString());
    void appendEventLog(const QString &level, const QString &message);
    void updateMaximizeButtonIcon();

private slots:
    void onWaveformUpdate();
    void onSpeedPlotMouseMove(QMouseEvent *event);
    void onTorquePlotMouseMove(QMouseEvent *event);
    void onSpeedPlotMousePress(QMouseEvent *event);
    void onTorquePlotMousePress(QMouseEvent *event);

private:
    Ui::MainWindow *ui;
    QWidget *m_titleDragArea{nullptr};
    ElaIconButton *m_maximizeButton{nullptr};
    ElaPlainTextEdit *m_eventLogEdit{nullptr};
    QCustomPlot *m_speedPlot{nullptr};
    QCustomPlot *m_torquePlot{nullptr};
    QCPItemStraightLine *m_speedCursorLine{nullptr};
    QCPItemText *m_speedTimeLabel{nullptr};
    QCPItemStraightLine *m_torqueCursorLine{nullptr};
    QCPItemText *m_torqueTimeLabel{nullptr};
    QTimer *m_waveformTimer{nullptr};
    QPoint m_dragPosition;
    bool m_dragging{false};
    double m_currentT{0};
    bool m_speedCursorFrozen{false};
    bool m_torqueCursorFrozen{false};
    double m_speedFrozenX{0};
    double m_torqueFrozenX{0};
    QDateTime m_speedFrozenTime;
    QDateTime m_torqueFrozenTime;
};
#endif // MAINWINDOW_H
