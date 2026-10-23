#include "ui/settings/VisionSettingsPage.h"

#include "ElaPushButton.h"
#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "camera/HikCameraConfig.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QSettings>
#include <QShowEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

enum class StatusTone {
    Success,
    Warning,
    Error,
    Neutral,
};

QString statusColor(StatusTone tone)
{
    switch (tone) {
    case StatusTone::Success: return QString::fromLatin1(Theme::kStateSuccess);
    case StatusTone::Warning: return QString::fromLatin1(Theme::kStateWarning);
    case StatusTone::Error: return QString::fromLatin1(Theme::kStateError);
    case StatusTone::Neutral: return QString::fromLatin1(Theme::kTextSecondary);
    }
    return QString::fromLatin1(Theme::kTextSecondary);
}

QString statusBackground(StatusTone tone)
{
    switch (tone) {
    case StatusTone::Success: return QString::fromLatin1(Theme::kSuccessBg);
    case StatusTone::Warning: return QString::fromLatin1(Theme::kWarningBg);
    case StatusTone::Error: return QString::fromLatin1(Theme::kErrorBg);
    case StatusTone::Neutral: return QString::fromLatin1(Theme::kControlBg);
    }
    return QString::fromLatin1(Theme::kControlBg);
}

QLabel *makeStatusBadge(QWidget *parent)
{
    auto *badge = makeLabel(QString(), QStringLiteral("settingsStatusBadge"));
    badge->setParent(parent);
    badge->setMinimumWidth(88);
    badge->setFixedHeight(22);
    badge->setAlignment(Qt::AlignCenter);
    return badge;
}

void setStatusBadge(QLabel *badge, const QString &text, StatusTone tone)
{
    if (!badge) {
        return;
    }
    badge->setText(text);
    badge->setStyleSheet(QStringLiteral(
        "color:%1;background:%2;border:none;border-radius:11px;padding:2px 10px;font-size:11px;")
        .arg(statusColor(tone), statusBackground(tone)));
}

void setStatusBanner(QLabel *banner, const QString &text, StatusTone tone)
{
    if (!banner) {
        return;
    }
    banner->setText(text);
    banner->setStyleSheet(QStringLiteral(
        "color:%1;background:%2;border:1px solid %3;border-radius:5px;padding:8px 10px;font-size:12px;")
        .arg(statusColor(tone), statusBackground(tone), Theme::kBorder));
}

QLabel *makeValue(const QString &text, QWidget *parent, bool prominent = false)
{
    auto *value = makeLabel(text);
    value->setParent(parent);
    value->setMinimumWidth(0);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    Theme::bindStyle(value, [prominent]() {
        return QStringLiteral(
            "color:%1;font-size:%2px;font-weight:%3;border:none;background:transparent;")
            .arg(Theme::kTextPrimary)
            .arg(prominent ? 13 : 12)
            .arg(prominent ? 600 : 400);
    });
    return value;
}

QLabel *makeKey(const QString &text, QWidget *parent)
{
    auto *key = makeLabel(text);
    key->setParent(parent);
    Theme::bindStyle(key, []() {
        return QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
            .arg(Theme::kTextSecondary);
    });
    return key;
}

QWidget *makeValueRow(const QString &name, QLabel **value, QWidget *parent)
{
    auto *row = new QWidget(parent);
    row->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *key = makeKey(name, row);
    key->setFixedWidth(104);
    *value = makeValue(QStringLiteral("—"), row);
    layout->addWidget(key);
    layout->addWidget(*value, 1);
    return row;
}

QFrame *makeInsetPanel(QWidget *parent)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("visionInsetPanel"));
    Theme::bindStyle(panel, []() {
        return QStringLiteral("QFrame#visionInsetPanel { background:%1; border:1px solid %2; border-radius:5px; }")
            .arg(Theme::kPanelBg, Theme::kBorder);
    });
    return panel;
}

void styleActionButton(ElaPushButton *button, bool primary = false)
{
    if (!button) {
        return;
    }
    button->setMinimumHeight(31);
    if (primary) {
        applyPrimaryButtonStyle(button);
    } else {
        applySecondaryButtonStyle(button);
    }
}

void bindComboStyle(QComboBox *combo)
{
    Theme::bindStyle(combo, []() {
        return QStringLiteral(
            "QComboBox { color:%1;background:%2;border:1px solid %3;border-radius:5px;"
            "padding:5px 9px;min-height:23px;font-size:12px; }"
            "QComboBox:hover { border-color:%4; }"
            "QComboBox:focus { border-color:%5; }"
            "QComboBox::drop-down { border:none;width:24px; }"
            "QComboBox QAbstractItemView { color:%1;background:%6;selection-background-color:%7;"
            "border:1px solid %3;outline:none; }")
            .arg(Theme::kTextPrimary,
                 Theme::kControlBg,
                 Theme::kBorder,
                 Theme::kAccent,
                 Theme::kFocusRing,
                 Theme::kPanelBg,
                 Theme::kSelectionBg);
    });
}

void addHeaderBadge(QFrame *panel, QLabel *badge)
{
    if (!panel || !badge) {
        return;
    }
    if (QWidget *header = panel->findChild<QWidget *>(QStringLiteral("settingsPanelHeader"))) {
        if (auto *layout = qobject_cast<QHBoxLayout *>(header->layout())) {
            layout->addWidget(badge, 0, Qt::AlignVCenter);
        }
    }
}

} // namespace

VisionSettingsPage::VisionSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    auto *nodePanel = settingsGroupPanel(QStringLiteral("相机节点"),
                                         QStringLiteral(":/img/nav_settings.svg"), this);
    grid->addWidget(nodePanel, 0, 0);
    auto *nodeLayout = qobject_cast<QVBoxLayout *>(nodePanel->layout());
    m_nodeBadge = makeStatusBadge(nodePanel);
    addHeaderBadge(nodePanel, m_nodeBadge);

    auto *nodeFacts = makeInsetPanel(nodePanel);
    auto *factsLayout = new QVBoxLayout(nodeFacts);
    factsLayout->setContentsMargins(10, 8, 10, 8);
    factsLayout->setSpacing(7);
    auto *rosRow = new QHBoxLayout();
    rosRow->setContentsMargins(0, 0, 0, 0);
    auto *rosName = makeLabel(QStringLiteral("ROS 2 节点"));
    rosName->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                               .arg(Theme::kTextSecondary));
    auto *rosValue = makeValue(QStringLiteral("/mz_tlz"), nodeFacts, true);
    rosRow->addWidget(rosName);
    rosRow->addWidget(rosValue);
    rosRow->addStretch(1);
    auto *domainName = makeLabel(QStringLiteral("Domain ID"));
    domainName->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                                  .arg(Theme::kTextSecondary));
    const QString domain = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("ROS_DOMAIN_ID"), QStringLiteral("0"));
    auto *domainValue = makeValue(domain, nodeFacts, true);
    rosRow->addWidget(domainName);
    rosRow->addWidget(domainValue);
    factsLayout->addLayout(rosRow);

    auto *topicRow = new QHBoxLayout();
    topicRow->setContentsMargins(0, 0, 0, 0);
    auto *topicName = makeLabel(QStringLiteral("订阅 Topic"));
    topicName->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                                 .arg(Theme::kTextSecondary));
    auto *topicValue = makeValue(QStringLiteral("/camera/status"), nodeFacts, true);
    topicValue->setObjectName(QStringLiteral("visionCameraTopic"));
    topicRow->addWidget(topicName);
    topicRow->addWidget(topicValue, 1);
    auto *publisherName = makeLabel(QStringLiteral("发布者"));
    publisherName->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                                     .arg(Theme::kTextSecondary));
    m_publisherCount = makeValue(QStringLiteral("查询中"), nodeFacts, true);
    topicRow->addWidget(publisherName);
    topicRow->addWidget(m_publisherCount);
    factsLayout->addLayout(topicRow);
    auto *typeRow = new QHBoxLayout();
    typeRow->setContentsMargins(0, 0, 0, 0);
    typeRow->addWidget(makeLabel(QStringLiteral("消息类型")));
    auto *typeValue = makeValue(QStringLiteral("mz_interfaces/msg/CameraStatus"), nodeFacts);
    typeValue->setStyleSheet(QStringLiteral(
        "color:%1;font-family:Consolas,monospace;font-size:11px;border:none;background:transparent;")
        .arg(Theme::kTextPrimary));
    typeRow->addWidget(typeValue, 1);
    factsLayout->addLayout(typeRow);
    auto *qosRow = new QHBoxLayout();
    qosRow->setContentsMargins(0, 0, 0, 0);
    qosRow->addWidget(makeLabel(QStringLiteral("QoS")));
    auto *qosValue = makeValue(QStringLiteral("Reliable · Transient Local · Depth 1"), nodeFacts);
    qosRow->addWidget(qosValue, 1);
    factsLayout->addLayout(qosRow);
    nodeLayout->addWidget(nodeFacts);

    m_diagnosticBanner = makeLabel(QString());
    m_diagnosticBanner->setWordWrap(true);
    m_diagnosticBanner->setMinimumHeight(38);
    nodeLayout->addWidget(m_diagnosticBanner);
    auto *nodeActions = new QHBoxLayout();
    nodeActions->setContentsMargins(0, 2, 0, 0);
    nodeActions->addStretch(1);
    m_refreshNodeButton = new ElaPushButton(QStringLiteral("重新检测"), nodePanel);
    styleActionButton(m_refreshNodeButton);
    m_diagnosticsButton = new ElaPushButton(QStringLiteral("查看诊断"), nodePanel);
    styleActionButton(m_diagnosticsButton);
    nodeActions->addWidget(m_refreshNodeButton);
    nodeActions->addWidget(m_diagnosticsButton);
    nodeLayout->addLayout(nodeActions);

    connect(m_refreshNodeButton, &ElaPushButton::clicked,
            this, &VisionSettingsPage::refreshCameraDiagnosticsRequested);
    connect(m_diagnosticsButton, &ElaPushButton::clicked, this, [this]() {
        QString body = QStringLiteral("节点：/mz_tlz\nDomain ID：%1\nTopic：/camera/status\n"
                                      "发布者：%2\n消息类型：mz_interfaces/msg/CameraStatus\n"
                                      "QoS：Reliable · Transient Local · Depth 1")
                           .arg(QProcessEnvironment::systemEnvironment().value(
                                    QStringLiteral("ROS_DOMAIN_ID"), QStringLiteral("0")),
                                m_publisherCount ? m_publisherCount->text() : QStringLiteral("—"));
        if (!m_cameraDetail.isEmpty()) {
            body += QStringLiteral("\n最近消息：%1").arg(m_cameraDetail.simplified());
        }
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("相机节点诊断"));
        box.setText(body);
        box.setIcon(QMessageBox::Information);
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
    });

    auto *previewPanel = settingsGroupPanel(QStringLiteral("实时预览"),
                                             QStringLiteral(":/img/nav_detect.svg"), this);
    grid->addWidget(previewPanel, 0, 1);
    auto *previewLayout = qobject_cast<QVBoxLayout *>(previewPanel->layout());
    m_previewBadge = makeStatusBadge(previewPanel);
    addHeaderBadge(previewPanel, m_previewBadge);

    m_previewSurface = new QFrame(previewPanel);
    m_previewSurface->setObjectName(QStringLiteral("visionPreviewSurface"));
    m_previewSurface->setMinimumHeight(145);
    m_previewSurface->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewSurface->setAttribute(Qt::WA_NativeWindow);
    m_previewSurface->setAttribute(Qt::WA_OpaquePaintEvent);
    m_previewSurface->setStyleSheet(QStringLiteral(
        "QFrame#visionPreviewSurface { background:#203548;border:1px solid %1;border-radius:5px; }")
        .arg(Theme::kBorder));
    auto *surfaceLayout = new QVBoxLayout(m_previewSurface);
    surfaceLayout->setContentsMargins(8, 8, 8, 8);
    m_previewPlaceholder = makeLabel(QStringLiteral("实时画面未启动"));
    m_previewPlaceholder->setAlignment(Qt::AlignCenter);
    m_previewPlaceholder->setStyleSheet(QStringLiteral(
        "color:#CBD7E2;font-size:13px;border:none;background:transparent;"));
    surfaceLayout->addWidget(m_previewPlaceholder);
    previewLayout->addWidget(m_previewSurface, 1);

    auto *cameraActions = new QHBoxLayout();
    cameraActions->setContentsMargins(0, 2, 0, 0);
    auto *deviceLabel = makeLabel(QStringLiteral("设备"));
    deviceLabel->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                                   .arg(Theme::kTextSecondary));
    cameraActions->addWidget(deviceLabel);
    m_cameraSelector = new QComboBox(previewPanel);
    QString cameraError;
    const HikCameraConfig hikConfig = HikCameraConfig::load(HikCameraConfig::defaultFilePath(), &cameraError);
    QString validationError;
    const bool cameraReady = cameraError.isEmpty() && hikConfig.validate(&validationError);
    if (cameraReady) {
        m_cameraSelector->addItem(QStringLiteral("海康相机 · %1").arg(hikConfig.address));
    } else {
        m_cameraSelector->addItem(QStringLiteral("未配置海康相机"));
        m_cameraSelector->setEnabled(false);
    }
    m_cameraSelector->setMinimumWidth(170);
    m_cameraSelector->setMaximumWidth(280);
    bindComboStyle(m_cameraSelector);
    cameraActions->addWidget(m_cameraSelector, 1);
    cameraActions->addStretch(1);
    m_stopPreviewButton = new ElaPushButton(QStringLiteral("停止预览"), previewPanel);
    styleActionButton(m_stopPreviewButton);
    m_startPreviewButton = new ElaPushButton(QStringLiteral("连接并预览"), previewPanel);
    styleActionButton(m_startPreviewButton, true);
    m_startPreviewButton->setEnabled(cameraReady);
    if (!cameraReady) {
        m_startPreviewButton->setToolTip(cameraError.isEmpty() ? validationError : cameraError);
    }
    cameraActions->addWidget(m_stopPreviewButton);
    cameraActions->addWidget(m_startPreviewButton);
    previewLayout->addLayout(cameraActions);
    connect(m_startPreviewButton, &ElaPushButton::clicked, this, [this]() {
        if (!m_previewSurface) {
            return;
        }
        m_previewRequested = true;
        m_previewPlaceholder->setText(QStringLiteral("正在连接海康相机…"));
        updatePreviewStatus();
        emit previewRequested(m_previewSurface->winId());
    });
    connect(m_stopPreviewButton, &ElaPushButton::clicked, this, [this]() {
        m_previewRequested = false;
        emit stopPreviewRequested();
        updatePreviewStatus();
    });

    auto *rulesPanel = settingsGroupPanel(QStringLiteral("检测判定"),
                                           QStringLiteral(":/img/kpi_target.svg"), this);
    grid->addWidget(rulesPanel, 1, 0);
    auto *rulesLayout = qobject_cast<QVBoxLayout *>(rulesPanel->layout());
    const auto addPendingRule = [rulesLayout, rulesPanel](const QString &name,
                                                          const QString &unit) {
        auto *row = new QWidget(rulesPanel);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        rowLayout->setSpacing(8);
        auto *label = makeLabel(name);
        label->setMinimumWidth(132);
        label->setStyleSheet(QStringLiteral("color:%1;font-size:12px;font-weight:600;border:none;background:transparent;")
                                 .arg(Theme::kTextPrimary));
        auto *value = makeLabel(QStringLiteral("未配置"));
        value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        value->setMinimumWidth(120);
        Theme::bindStyle(value, []() {
            return QStringLiteral("color:%1;background:%2;border:1px solid %3;border-radius:5px;"
                                  "padding:5px 10px;font-size:12px;")
                .arg(Theme::kTextMuted, Theme::kControlBg, Theme::kBorder);
        });
        rowLayout->addWidget(label);
        rowLayout->addWidget(value, 1);
        if (!unit.isEmpty()) {
            auto *unitLabel = makeLabel(unit);
            unitLabel->setFixedWidth(36);
            unitLabel->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                                         .arg(Theme::kTextMuted));
            rowLayout->addWidget(unitLabel);
        }
        rulesLayout->addWidget(row);
    };
    addPendingRule(QStringLiteral("粒子高度阈值"), QStringLiteral("mm"));
    addPendingRule(QStringLiteral("最小识别面积"), QStringLiteral("mm²"));
    addPendingRule(QStringLiteral("默认判定等级"), QString());
    addPendingRule(QStringLiteral("自动复核"), QString());
    rulesLayout->addStretch(1);

    auto *calibrationPanel = settingsGroupPanel(QStringLiteral("坐标标定"),
                                                 QStringLiteral(":/img/kpi_check.svg"), this);
    grid->addWidget(calibrationPanel, 1, 1);
    auto *calibrationLayout = qobject_cast<QVBoxLayout *>(calibrationPanel->layout());
    m_calibrationBadge = makeStatusBadge(calibrationPanel);
    addHeaderBadge(calibrationPanel, m_calibrationBadge);
    auto *relationRow = new QHBoxLayout();
    relationRow->setContentsMargins(0, 0, 0, 0);
    relationRow->addWidget(makeLabel(QStringLiteral("坐标关系")));
    auto *relationValue = makeValue(QStringLiteral("相机 → 铜板 → 机床"), calibrationPanel, true);
    relationRow->addWidget(relationValue, 1);
    calibrationLayout->addLayout(relationRow);

    auto *compensationTitle = settingsSectionHeading(QStringLiteral("当前坐标补偿 · mm"), calibrationPanel);
    calibrationLayout->addWidget(compensationTitle);
    auto *axes = new QHBoxLayout();
    axes->setContentsMargins(0, 0, 0, 0);
    axes->setSpacing(10);
    const auto addAxis = [axes, calibrationPanel](const QString &axis, QLabel **value) {
        auto *column = new QVBoxLayout();
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(4);
        auto *label = makeLabel(axis);
        label->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                                 .arg(Theme::kTextSecondary));
        *value = makeLabel(QStringLiteral("—"));
        (*value)->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        Theme::bindStyle(*value, []() {
            return QStringLiteral("color:%1;background:%2;border:1px solid %3;border-radius:5px;"
                                  "padding:5px 9px;font-size:12px;")
                .arg(Theme::kTextPrimary, Theme::kControlBg, Theme::kBorder);
        });
        column->addWidget(label);
        column->addWidget(*value);
        axes->addLayout(column, 1);
    };
    addAxis(QStringLiteral("X"), &m_compensationX);
    addAxis(QStringLiteral("Y"), &m_compensationY);
    addAxis(QStringLiteral("Z"), &m_compensationZ);
    calibrationLayout->addLayout(axes);
    auto *calibrationHelp = settingsHint(QStringLiteral("显示已保存的 XYZ 补偿。"), calibrationPanel);
    calibrationLayout->addWidget(calibrationHelp);
    calibrationLayout->addStretch(1);
    auto *calibrationActions = new QHBoxLayout();
    calibrationActions->addStretch(1);
    auto *editCompensation = new ElaPushButton(QStringLiteral("设置坐标"), calibrationPanel);
    styleActionButton(editCompensation);
    calibrationActions->addWidget(editCompensation);
    calibrationLayout->addLayout(calibrationActions);
    connect(editCompensation, &ElaPushButton::clicked,
            this, &VisionSettingsPage::processSettingsRequested);

    Theme::watch(this, [this]() {
        updateNodeStatus();
        updatePreviewStatus();
        refreshCalibrationValues();
        styleActionButton(m_refreshNodeButton);
        styleActionButton(m_diagnosticsButton, true);
        styleActionButton(m_stopPreviewButton);
        styleActionButton(m_startPreviewButton, true);
    });
    updateNodeStatus();
    updatePreviewStatus();
    refreshCalibrationValues();
}

QString VisionSettingsPage::bindingSummary() const
{
    return QStringLiteral("相机节点与实时预览已接入。坐标卡显示 backend_params.ini 中已保存的补偿值。");
}

void VisionSettingsPage::setCameraNodeStatus(bool connected, const QString &detail)
{
    m_cameraNodeReceived = true;
    m_cameraNodeConnected = connected;
    m_cameraDetail = detail.trimmed();
    m_cameraLastMessageAt = QDateTime::currentDateTime();
    updateNodeStatus();
}

void VisionSettingsPage::setCameraTopicDiagnostics(bool bridgeRunning, int publisherCount)
{
    m_bridgeRunning = bridgeRunning;
    m_publisherCountValue = publisherCount;
    updateNodeStatus();
}

void VisionSettingsPage::setHikCameraStatus(bool connected, const QString &detail)
{
    m_hikConnected = connected;
    m_hikConnecting = !connected && detail.contains(QStringLiteral("正在连接"));
    m_hikDetail = detail.trimmed();
    if (connected) {
        m_previewRequested = true;
    } else if (!m_hikConnecting) {
        m_previewRequested = false;
    }
    if (m_previewPlaceholder) {
        m_previewPlaceholder->setVisible(!connected);
        if (!connected) {
            m_previewPlaceholder->setText(m_hikConnecting ? QStringLiteral("正在连接海康相机…")
                                                          : QStringLiteral("实时画面未启动"));
        }
    }
    updatePreviewStatus();
}

void VisionSettingsPage::showEvent(QShowEvent *event)
{
    SettingsSubPage::showEvent(event);
    refreshCalibrationValues();
}

void VisionSettingsPage::hideEvent(QHideEvent *event)
{
    if (m_previewRequested || m_hikConnected || m_hikConnecting) {
        m_previewRequested = false;
        emit stopPreviewRequested();
    }
    SettingsSubPage::hideEvent(event);
}

void VisionSettingsPage::updateNodeStatus()
{
    if (!m_nodeBadge) {
        return;
    }

    QString status;
    QString detail;
    StatusTone tone = StatusTone::Warning;
    if (!m_bridgeRunning) {
        status = QStringLiteral("ROS 2 未启动");
        detail = QStringLiteral("等待 ROS 2 通信节点启动");
        tone = StatusTone::Error;
    } else if (m_publisherCountValue == 0) {
        status = QStringLiteral("无发布者");
        detail = QStringLiteral("未发现 /camera/status 发布者");
        tone = StatusTone::Error;
    } else if (m_cameraNodeReceived && m_cameraNodeConnected) {
        status = QStringLiteral("已连接");
        detail = m_cameraDetail.isEmpty() ? QStringLiteral("已收到相机状态") : m_cameraDetail.simplified();
        tone = StatusTone::Success;
    } else if (m_cameraNodeReceived) {
        status = QStringLiteral("相机离线");
        detail = m_cameraDetail.isEmpty() ? QStringLiteral("后端报告相机离线") : m_cameraDetail.simplified();
        tone = StatusTone::Error;
    } else if (m_publisherCountValue > 0) {
        status = QStringLiteral("等待状态");
        detail = QStringLiteral("Topic 已有发布者，尚未收到状态消息");
        tone = StatusTone::Warning;
    } else {
        status = QStringLiteral("等待 Topic");
        detail = QStringLiteral("状态诊断每秒自动更新");
        tone = StatusTone::Warning;
    }

    setStatusBadge(m_nodeBadge, status, tone);
    const QString receivedAt = m_cameraLastMessageAt.isValid()
        ? QStringLiteral(" · %1").arg(m_cameraLastMessageAt.toString(QStringLiteral("HH:mm:ss")))
        : QString();
    setStatusBanner(m_diagnosticBanner, detail + receivedAt, tone);
    if (m_publisherCount) {
        m_publisherCount->setText(m_publisherCountValue < 0
                                      ? QStringLiteral("查询中")
                                      : QStringLiteral("%1 个").arg(m_publisherCountValue));
    }
}

void VisionSettingsPage::updatePreviewStatus()
{
    if (!m_previewBadge) {
        return;
    }
    if (m_hikConnected) {
        setStatusBadge(m_previewBadge, QStringLiteral("已连接"), StatusTone::Success);
    } else if (m_hikConnecting || m_previewRequested) {
        setStatusBadge(m_previewBadge, QStringLiteral("连接中"), StatusTone::Warning);
    } else if (m_cameraSelector && !m_cameraSelector->isEnabled()) {
        setStatusBadge(m_previewBadge, QStringLiteral("未配置"), StatusTone::Warning);
    } else {
        setStatusBadge(m_previewBadge, QStringLiteral("未连接"), StatusTone::Warning);
    }
    if (m_startPreviewButton) {
        m_startPreviewButton->setEnabled(!m_hikConnected && !m_hikConnecting
                                         && !m_previewRequested
                                         && m_cameraSelector && m_cameraSelector->isEnabled());
        m_startPreviewButton->setText(m_hikConnecting ? QStringLiteral("正在连接…")
                                                       : QStringLiteral("连接并预览"));
    }
    if (m_stopPreviewButton) {
        m_stopPreviewButton->setEnabled(m_hikConnected || m_hikConnecting || m_previewRequested);
    }
    if (m_previewSurface) {
        m_previewSurface->setToolTip(m_hikDetail);
    }
}

void VisionSettingsPage::refreshCalibrationValues()
{
    const QString path = backendParamsIniPath();
    const bool exists = !path.isEmpty() && QFileInfo::exists(path);
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (exists) {
        QSettings settings(path, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.beginGroup(QStringLiteral("BackendParams"));
        x = settings.value(QStringLiteral("OffsetX"), 0.0).toDouble();
        y = settings.value(QStringLiteral("OffsetY"), 0.0).toDouble();
        z = settings.value(QStringLiteral("OffsetZ"), 0.0).toDouble();
        settings.endGroup();
    }

    if (m_compensationX) m_compensationX->setText(exists ? QString::number(x, 'f', 2) : QStringLiteral("—"));
    if (m_compensationY) m_compensationY->setText(exists ? QString::number(y, 'f', 2) : QStringLiteral("—"));
    if (m_compensationZ) m_compensationZ->setText(exists ? QString::number(z, 'f', 2) : QStringLiteral("—"));
    if (m_calibrationBadge) {
        setStatusBadge(m_calibrationBadge,
                       exists ? QStringLiteral("补偿已配置") : QStringLiteral("未配置"),
                       exists ? StatusTone::Success : StatusTone::Warning);
    }
}
