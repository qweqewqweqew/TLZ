#ifndef TASKFLOWPANEL_H
#define TASKFLOWPANEL_H

#include <QFrame>

class QLabel;
class QScrollArea;
class QVBoxLayout;
struct PlcPathCommandParamsVM;

class TaskFlowPanel : public QFrame
{
    Q_OBJECT

public:
    explicit TaskFlowPanel(QWidget *parent = nullptr);

    void addPlcCommand(const PlcPathCommandParamsVM &command);
    void clearCommands();

private:
    QWidget *createCommandCard(const PlcPathCommandParamsVM &command,
                               int sequence,
                               QWidget *parent);

    QScrollArea *m_scrollArea{nullptr};
    QVBoxLayout *m_flowLayout{nullptr};
    int m_nextSequence{1};
};

#endif // TASKFLOWPANEL_H
