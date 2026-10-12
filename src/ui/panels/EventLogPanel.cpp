#include "ui/panels/EventLogPanel.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QDateTime>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextEdit>
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
    m_eventLogEdit = new QTextEdit(panel);
    m_eventLogEdit->setReadOnly(true);
    m_eventLogEdit->setPlaceholderText("这里显示报警和事件日志...");
    m_eventLogEdit->document()->setMaximumBlockCount(500);
    m_eventLogEdit->setStyleSheet(QString(R"(
        QTextEdit {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 8px;
            font-size: 14px;
        }
    )").arg(Theme::kLogBg, Theme::kTextSecondary, Theme::kDivider));

    auto *verticalScrollBar = m_eventLogEdit->verticalScrollBar();
    verticalScrollBar->setObjectName(QStringLiteral("eventLogScrollBar"));
    verticalScrollBar->setStyleSheet(QString(R"(
        QScrollBar#eventLogScrollBar:vertical {
            background: %1;
            width: 12px;
            margin: 0;
            border: none;
        }
        QScrollBar#eventLogScrollBar::handle:vertical {
            background: %2;
            min-height: 28px;
            margin: 2px;
            border: none;
            border-radius: 4px;
        }
        QScrollBar#eventLogScrollBar::handle:vertical:hover {
            background: %3;
        }
        QScrollBar#eventLogScrollBar::handle:vertical:pressed {
            background: %4;
        }
        QScrollBar#eventLogScrollBar::add-line:vertical,
        QScrollBar#eventLogScrollBar::sub-line:vertical {
            height: 0;
            background: %1;
            border: none;
        }
        QScrollBar#eventLogScrollBar::up-arrow:vertical,
        QScrollBar#eventLogScrollBar::down-arrow:vertical {
            width: 0;
            height: 0;
            background: none;
            border: none;
        }
        QScrollBar#eventLogScrollBar::add-page:vertical,
        QScrollBar#eventLogScrollBar::sub-page:vertical {
            background: %1;
            border: none;
        }
    )").arg(Theme::kLogBg,
             Theme::kBorder,
             Theme::kTextMuted,
             Theme::kAccent));
    panelLayout->addWidget(m_eventLogEdit, 1);
}

void EventLogPanel::appendEvent(const QString &level, const QString &message)
{
    if (!m_eventLogEdit) {
        return;
    }

    // 仅日志级别使用语义色，正文保持中性，避免整行高饱和。
    QString levelColor = QString::fromLatin1(Theme::kAccent);
    if (level.compare(QStringLiteral("ERROR"), Qt::CaseInsensitive) == 0) {
        levelColor = QString::fromLatin1(Theme::kStateError);
    } else if (level.compare(QStringLiteral("WARN"), Qt::CaseInsensitive) == 0
               || level.compare(QStringLiteral("WARNING"), Qt::CaseInsensitive) == 0) {
        levelColor = QString::fromLatin1(Theme::kStateWarning);
    } else if (level.compare(QStringLiteral("SUCCESS"), Qt::CaseInsensitive) == 0) {
        levelColor = QString::fromLatin1(Theme::kStateSuccess);
    }

    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    const QString html = QStringLiteral(
                             "<span style=\"color:%1;\">[%2]</span> "
                             "<span style=\"color:%3;font-weight:600;\">[%4]</span> "
                             "<span style=\"color:%5;\">%6</span>")
                             .arg(QString::fromLatin1(Theme::kTextMuted), // 时间戳（弱化）
                                  timestamp.toHtmlEscaped(),
                                  levelColor,                         // 级别着色
                                  level.toHtmlEscaped(),
                                  QString::fromLatin1(Theme::kTextSecondary), // 正文
                                  message.toHtmlEscaped());
    m_eventLogEdit->append(html);
    m_eventLogEdit->moveCursor(QTextCursor::End);
}
