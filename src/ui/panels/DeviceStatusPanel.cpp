#include "ui/panels/DeviceStatusPanel.h"

#include "bridge/PlcStatusPresentation.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>

namespace {

const QString kValueColorNoData = Theme::kStateUnknown;

} // namespace

DeviceStatusPanel::DeviceStatusPanel(QWidget *parent)
    : QFrame(parent)
{
    // 横跨主窗口底部：左侧时间、中间版权、右侧三个状态指示控件。
    setObjectName("deviceStatusBar");
    setMinimumHeight(42);
    setStyleSheet(QStringLiteral(R"(
        QFrame#deviceStatusBar {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(Theme::kStatusBarBg, Theme::kDivider));

    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setHorizontalSpacing(12);

    m_clockLabel = makeLabel(QString());
    m_clockLabel->setWordWrap(false);
    m_clockLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_clockLabel->setStyleSheet(QString(
        "color:%1;font-size:16px;border:none;background:transparent;")
                                    .arg(Theme::kTextSecondary));
    layout->addWidget(m_clockLabel, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

    auto *copyrightLabel = makeLabel(
        QStringLiteral("版权所有：长沙铭准信息科技有限公司"));
    copyrightLabel->setWordWrap(false);
    copyrightLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    copyrightLabel->setStyleSheet(QString(
        "color:%1;font-size:16px;border:none;background:transparent;")
                                      .arg(Theme::kTextMuted));
    layout->addWidget(copyrightLabel, 0, 1, Qt::AlignCenter);

    auto *statusHost = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusHost);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(8);
    statusLayout->addWidget(createStatusItem(QStringLiteral("相机"),
                                             m_cameraStatusDot,
                                             statusHost));
    statusLayout->addWidget(createStatusItem(QStringLiteral("后端"),
                                             m_backendStatusDot,
                                             statusHost));
    statusLayout->addWidget(createStatusItem(QStringLiteral("PLC"),
                                             m_plcStatusDot,
                                             statusHost));
    layout->addWidget(statusHost, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(2, 1);

    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(1000);
    connect(m_clockTimer, &QTimer::timeout, this, &DeviceStatusPanel::updateCurrentTime);
    updateCurrentTime();
    m_clockTimer->start();

    m_cameraStatusTimer = new QTimer(this);
    m_cameraStatusTimer->setSingleShot(true);
    m_cameraStatusTimer->setInterval(3000);
    connect(m_cameraStatusTimer, &QTimer::timeout, this, [this]() {
        applyCameraStatus(ConnectionState::Unknown, QStringLiteral("状态消息超时"));
    });
    applyCameraStatus(ConnectionState::Unknown, QStringLiteral("等待状态"));
    clearBackendAndPlcStatus();
}

void DeviceStatusPanel::updateCurrentTime()
{
    if (!m_clockLabel) {
        return;
    }

    static const QStringList weekdays = {
        QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("日")
    };
    const QDateTime now = QDateTime::currentDateTime();
    const int weekdayIndex = now.date().dayOfWeek() - 1;
    m_clockLabel->setText(
        QStringLiteral("%1 星期%2")
            .arg(now.toString(QStringLiteral("yyyy年MM月dd日 HH:mm:ss")),
                 weekdays.value(weekdayIndex)));
}

void DeviceStatusPanel::clearBackendAndPlcStatus()
{
    applyStatusItem(m_backendStatusDot,
                    kValueColorNoData,
                    QStringLiteral("尚未收到 PLC 反馈"));
    applyStatusItem(m_plcStatusDot,
                    kValueColorNoData,
                    QStringLiteral("尚未收到 PLC 反馈"));
}

void DeviceStatusPanel::setCameraStatus(bool connected,
                                        const QString &cameraId,
                                        const QString &message)
{
    QString detail = connected ? cameraId : message;
    if (detail.isEmpty()) {
        detail = connected ? QStringLiteral("已连接") : QStringLiteral("未连接");
    }

    QString tooltip = cameraId;
    if (!message.isEmpty()) {
        if (!tooltip.isEmpty()) {
            tooltip += QLatin1Char('\n');
        }
        tooltip += message;
    }
    applyCameraStatus(connected ? ConnectionState::Online : ConnectionState::Offline,
                      tooltip.isEmpty() ? detail : tooltip);
    if (m_cameraStatusTimer) {
        m_cameraStatusTimer->start();
    }
}

void DeviceStatusPanel::setBackendOnline(const QString &tooltip)
{
    applyStatusItem(m_backendStatusDot,
                    QString::fromLatin1(Theme::kStateSuccess),
                    tooltip);
}

void DeviceStatusPanel::setPlcStatus(quint16 statusWord, quint16 faultCode)
{
    const PlcStatusPresentation plcStatus = plcStatusPresentation(statusWord, faultCode);
    const QString tooltip = QStringLiteral("状态: %1\n状态字: 0x%2\n故障码: %3")
                                .arg(plcStatus.text)
                                .arg(statusWord, 4, 16, QLatin1Char('0'))
                                .arg(faultCode)
                                .toUpper();
    QString displayColor = QString::fromLatin1(Theme::kStateUnknown);
    if (plcStatus.critical) {
        displayColor = QString::fromLatin1(Theme::kStateError);
    } else if (plcStatus.color != QString::fromLatin1(Theme::kStateUnknown)) {
        displayColor = QString::fromLatin1(Theme::kStateSuccess);
    }
    applyStatusItem(m_plcStatusDot,
                    displayColor,
                    tooltip);
}

QWidget *DeviceStatusPanel::createStatusItem(const QString &name,
                                             QLabel *&dotLabel,
                                             QWidget *parent)
{
    auto *item = new QWidget(parent);
    auto *layout = new QHBoxLayout(item);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(5);

    dotLabel = makeLabel(QStringLiteral("●"));
    dotLabel->setFixedWidth(16);
    layout->addWidget(dotLabel);

    auto *nameLabel = makeLabel(name);
    nameLabel->setStyleSheet(QString(
        "color:%1;font-size:14px;border:none;background:transparent;")
        .arg(Theme::kTextSecondary));
    layout->addWidget(nameLabel);
    return item;
}

void DeviceStatusPanel::applyCameraStatus(ConnectionState state, const QString &detail)
{
    QString color;
    switch (state) {
    case ConnectionState::Online:
        color = QString::fromLatin1(Theme::kStateSuccess);
        break;
    case ConnectionState::Offline:
        color = QString::fromLatin1(Theme::kStateError);
        break;
    case ConnectionState::Unknown:
        color = kValueColorNoData;
        break;
    }

    applyStatusItem(m_cameraStatusDot, color, detail);
}

void DeviceStatusPanel::applyStatusItem(QLabel *dotLabel,
                                        const QString &color,
                                        const QString &tooltip)
{
    if (!dotLabel) {
        return;
    }

    dotLabel->setText(QStringLiteral("●"));
    dotLabel->setStyleSheet(
        QString("color:%1;font-size:14px;border:none;background:transparent;").arg(color));
    dotLabel->setToolTip(tooltip);
}
