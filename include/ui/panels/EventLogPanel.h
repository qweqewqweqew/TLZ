#ifndef EVENTLOGPANEL_H
#define EVENTLOGPANEL_H

#include <QFrame>

class QTextEdit;

class EventLogPanel : public QFrame
{
    Q_OBJECT

public:
    // framed=false 时不带「报警与事件日志」卡片外框，供通知抽屉直接嵌入。
    explicit EventLogPanel(QWidget *parent = nullptr, bool framed = true);

    void appendEvent(const QString &level, const QString &message);
    QString snapshotText() const;
    void clear();

private:
    QTextEdit *m_eventLogEdit{nullptr};
};

#endif // EVENTLOGPANEL_H
