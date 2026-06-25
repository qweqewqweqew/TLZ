#include "EventLogPanel.h"

#include "ElaPlainTextEdit.h"
#include "UiHelpers.h"

#include <QDateTime>
#include <QTextCursor>
#include <QVBoxLayout>

EventLogPanel::EventLogPanel(QWidget *parent)
    : QFrame(parent)
{
    auto *panel = createPanel("报警与事件日志", nullptr, this);
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->addWidget(panel);

    setMinimumHeight(270);
    setMaximumHeight(340);

    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());
    m_eventLogEdit = new ElaPlainTextEdit(panel);
    m_eventLogEdit->setReadOnly(true);
    m_eventLogEdit->setPlaceholderText("这里显示报警和事件日志...");
    m_eventLogEdit->setMaximumBlockCount(500);
    m_eventLogEdit->setStyleSheet(R"(
        ElaPlainTextEdit {
            background: #101923;
            color: #B9C7D4;
            border: 1px solid #2F3D4A;
            border-radius: 6px;
            padding: 8px;
            font-size: 16px;
        }
    )");
    panelLayout->addWidget(m_eventLogEdit, 1);
}

void EventLogPanel::appendEvent(const QString &level, const QString &message)
{
    if (!m_eventLogEdit) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    const QString line = QString("[%1] [%2] %3").arg(timestamp, level, message);
    m_eventLogEdit->appendPlainText(line);
    m_eventLogEdit->moveCursor(QTextCursor::End);
}
