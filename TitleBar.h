#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QFrame>
#include <QPoint>

class ElaIconButton;
class QAction;

class TitleBar : public QFrame
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setMaximizedState(bool maximized);
    void setSimulationRunning(bool running);

signals:
    void settingsRequested();
    void simulationToggled(bool running);
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();
    void dragRequested(const QPoint &globalTopLeft);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateMaximizeButtonIcon();
    void updateSimulationActionText();

private:
    QAction *m_simulationAction{nullptr};
    ElaIconButton *m_maximizeButton{nullptr};
    QPoint m_dragPosition;
    bool m_dragging{false};
    bool m_maximized{false};
    bool m_simulationRunning{false};
};

#endif // TITLEBAR_H
