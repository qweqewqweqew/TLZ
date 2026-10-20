#include "ui/panels/DeviceStatusPanel.h"

#include "bridge/PlcStatusPresentation.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QTimer>
#include <QVariant>

#ifndef MZTLZ_VERSION_STRING
#define MZTLZ_VERSION_STRING "dev"
#endif

DeviceStatusPanel::DeviceStatusPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("deviceStatusBar");
    setFixedHeight(36);
    setStyleSheet(QStringLiteral(R"(
        QFrame#deviceStatusBar {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
    )").arg(Theme::kStatusBarBg, Theme::kDivider));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 14, 0);
    layout->setSpacing(0);

    layout->addWidget(createStatusItem(QStringLiteral("3D相机"), m_camera, this));
    layout->addSpacing(28);
    layout->addWidget(createStatusItem(QStringLiteral("PLC"), m_plc, this));
    layout->addSpacing(28);
    layout->addWidget(createStatusItem(QStringLiteral("修磨机"), m_machine, this));
    layout->addSpacing(28);
    layout->addWidget(createStatusItem(QStringLiteral("坐标标定"), m_calibration, this));
    layout->addStretch(1);

    auto *rightLabel = makeLabel(
        QStringLiteral("长沙铭准信息科技有限公司 · v" MZTLZ_VERSION_STRING));
    rightLabel->setObjectName(QStringLiteral("statusCompany"));
    rightLabel->setWordWrap(false);
    rightLabel->setStyleSheet(QString(
        "color:%1;font-size:12px;border:none;background:transparent;")
                                  .arg(Theme::kTextMuted));
    layout->addWidget(rightLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    m_cameraStatusTimer = new QTimer(this);
    m_cameraStatusTimer->setSingleShot(true);
    m_cameraStatusTimer->setInterval(3000);
    connect(m_cameraStatusTimer, &QTimer::timeout, this, [this]() {
        applyCameraStatus(ConnectionState::Unknown, QStringLiteral("无状态"), QStringLiteral("状态消息超时"));
    });
    applyCameraStatus(ConnectionState::Unknown, QStringLiteral("等待"), QStringLiteral("等待相机状态"));
    clearBackendAndPlcStatus();
    setCalibrationStatus(false, QStringLiteral("尚未读取坐标补偿"));
    Theme::watch(this, [this]() {
        setStyleSheet(QStringLiteral(R"(
            QFrame#deviceStatusBar {
                background: %1;
                border: 1px solid %2;
                border-radius: 6px;
            }
        )").arg(Theme::kStatusBarBg, Theme::kDivider));
        if (auto *company = findChild<QLabel *>(QStringLiteral("statusCompany"))) {
            company->setStyleSheet(QStringLiteral("color:%1;font-size:12px;border:none;background:transparent;")
                                       .arg(Theme::kTextMuted));
        }
        for (QLabel *name : findChildren<QLabel *>(QStringLiteral("statusName"))) {
            name->setStyleSheet(QStringLiteral("color:%1;font-size:13px;border:none;background:transparent;")
                                    .arg(Theme::kTextSecondary));
        }
        const auto refreshItem = [this](StatusItem &item) {
            if (!item.value) {
                return;
            }
            const QString role = item.value->property("statusRole").toString();
            const char *color = Theme::kTextPrimary;
            if (role == QLatin1String("success")) {
                color = Theme::kStateSuccess;
            } else if (role == QLatin1String("error")) {
                color = Theme::kStateError;
            } else if (role == QLatin1String("warning")) {
                color = Theme::kStateWarning;
            } else if (role == QLatin1String("running")) {
                color = Theme::kStateRunning;
            } else if (role == QLatin1String("unknown")) {
                color = Theme::kStateUnknown;
            } else if (!role.isEmpty()) {
                return;
            }
            item.value->setProperty("statusColor", QString());
            applyStatusItem(item, QString::fromLatin1(color), item.value->text(), item.value->toolTip());
        };
        refreshItem(m_camera);
        refreshItem(m_plc);
        refreshItem(m_machine);
        refreshItem(m_calibration);
    });
}

void DeviceStatusPanel::clearBackendAndPlcStatus()
{
    m_backendOnline = false;
    applyStatusItem(m_plc,
                    QString::fromLatin1(Theme::kStateUnknown),
                    QStringLiteral("未连接"),
                    QStringLiteral("尚未收到 PLC 反馈"));
    applyStatusItem(m_machine,
                    QString::fromLatin1(Theme::kStateUnknown),
                    m_backendStateText.isEmpty() ? QStringLiteral("离线") : m_backendStateText,
                    QStringLiteral("尚未收到后端反馈"));
}

void DeviceStatusPanel::setCameraStatus(bool connected,
                                        const QString &cameraId,
                                        const QString &message)
{
    QString tooltip = cameraId;
    if (!message.isEmpty()) {
        if (!tooltip.isEmpty()) {
            tooltip += QLatin1Char('\n');
        }
        tooltip += message;
    }
    applyCameraStatus(connected ? ConnectionState::Online : ConnectionState::Offline,
                      connected ? QStringLiteral("在线") : QStringLiteral("离线"),
                      tooltip);
    if (m_cameraStatusTimer) {
        m_cameraStatusTimer->start();
    }
}

void DeviceStatusPanel::setBackendOnline(const QString &tooltip)
{
    m_backendOnline = true;
    applyStatusItem(m_machine,
                    QString::fromLatin1(Theme::kStateSuccess),
                    m_backendStateText.isEmpty() ? QStringLiteral("在线") : m_backendStateText,
                    tooltip);
}

void DeviceStatusPanel::setBackendState(const QString &state)
{
    m_backendStateText = state.trimmed();
    applyStatusItem(m_machine,
                    QString::fromLatin1(m_backendOnline ? Theme::kStateSuccess : Theme::kStateRunning),
                    m_backendStateText.isEmpty() ? QStringLiteral("在线") : m_backendStateText,
                    QStringLiteral("后端状态：%1").arg(state));
}

void DeviceStatusPanel::setPlcStatus(quint16 statusWord, quint16 faultCode)
{
    const PlcStatusPresentation plcStatus = plcStatusPresentation(statusWord, faultCode);
    const QString tooltip = QStringLiteral("状态: %1\n状态字: 0x%2\n故障码: %3")
                                .arg(plcStatus.text)
                                .arg(statusWord, 4, 16, QLatin1Char('0'))
                                .arg(faultCode)
                                .toUpper();
    QString displayColor = QString::fromLatin1(Theme::kStateSuccess);
    QString text = QStringLiteral("已连接");
    if (plcStatus.critical) {
        displayColor = QString::fromLatin1(Theme::kStateError);
        text = QStringLiteral("故障 %1").arg(faultCode);
    } else if (plcStatus.color == QString::fromLatin1(Theme::kStateUnknown)) {
        displayColor = QString::fromLatin1(Theme::kStateUnknown);
        text = QStringLiteral("未知");
    }
    applyStatusItem(m_plc, displayColor, text, tooltip);
}

void DeviceStatusPanel::setCalibrationStatus(bool calibrated, const QString &detail)
{
    applyStatusItem(m_calibration,
                    QString::fromLatin1(calibrated ? Theme::kStateSuccess : Theme::kStateWarning),
                    calibrated ? QStringLiteral("已设置") : QStringLiteral("未标定"),
                    detail);
}

QWidget *DeviceStatusPanel::createStatusItem(const QString &name, StatusItem &item, QWidget *parent)
{
    auto *host = new QWidget(parent);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    item.dot = makeLabel(QStringLiteral("●"));
    item.dot->setFixedWidth(14);
    layout->addWidget(item.dot);

    auto *nameLabel = makeLabel(name + QStringLiteral("："));
    nameLabel->setObjectName(QStringLiteral("statusName"));
    nameLabel->setStyleSheet(QString(
        "color:%1;font-size:13px;border:none;background:transparent;")
        .arg(Theme::kTextSecondary));
    layout->addWidget(nameLabel);

    item.value = makeLabel(QString());
    item.value->setStyleSheet(QString(
        "color:%1;font-size:13px;border:none;background:transparent;")
        .arg(Theme::kTextPrimary));
    layout->addWidget(item.value);
    return host;
}

void DeviceStatusPanel::applyCameraStatus(ConnectionState state, const QString &text, const QString &detail)
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
        color = QString::fromLatin1(Theme::kStateUnknown);
        break;
    }
    applyStatusItem(m_camera, color, text, detail);
}

void DeviceStatusPanel::applyStatusItem(StatusItem &item,
                                        const QString &color,
                                        const QString &text,
                                        const QString &tooltip)
{
    if (!item.dot || !item.value) {
        return;
    }
    if (item.value->text() == text && item.value->toolTip() == tooltip
        && item.value->property("statusColor").toString() == color) {
        return;
    }
    QString role = QStringLiteral("custom");
    if (color == QLatin1String(Theme::kStateSuccess)) {
        role = QStringLiteral("success");
    } else if (color == QLatin1String(Theme::kStateError)) {
        role = QStringLiteral("error");
    } else if (color == QLatin1String(Theme::kStateWarning)) {
        role = QStringLiteral("warning");
    } else if (color == QLatin1String(Theme::kStateRunning)) {
        role = QStringLiteral("running");
    } else if (color == QLatin1String(Theme::kStateUnknown)) {
        role = QStringLiteral("unknown");
    }
    item.value->setProperty("statusRole", role);
    item.value->setProperty("statusColor", color);
    item.dot->setStyleSheet(
        QString("color:%1;font-size:12px;border:none;background:transparent;").arg(color));
    item.dot->setToolTip(tooltip);
    item.value->setText(text);
    item.value->setStyleSheet(
        QString("color:%1;font-size:13px;border:none;background:transparent;").arg(color));
    item.value->setToolTip(tooltip);
}
