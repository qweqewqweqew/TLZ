#ifndef EVENTLOGPANEL_H
#define EVENTLOGPANEL_H

#include <QFrame>

class ElaPlainTextEdit;

class EventLogPanel : public QFrame
{
    Q_OBJECT

public:
    explicit EventLogPanel(QWidget *parent = nullptr);

    void appendEvent(const QString &level, const QString &message);

private:
    ElaPlainTextEdit *m_eventLogEdit{nullptr};
};

#endif // EVENTLOGPANEL_H
