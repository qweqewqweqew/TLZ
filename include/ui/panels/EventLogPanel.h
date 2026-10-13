#ifndef EVENTLOGPANEL_H
#define EVENTLOGPANEL_H

#include <QFrame>

class QTextEdit;

class EventLogPanel : public QFrame
{
    Q_OBJECT

public:
    explicit EventLogPanel(QWidget *parent = nullptr);

    void appendEvent(const QString &level, const QString &message);
    QString snapshotText() const;

private:
    QTextEdit *m_eventLogEdit{nullptr};
};

#endif // EVENTLOGPANEL_H
