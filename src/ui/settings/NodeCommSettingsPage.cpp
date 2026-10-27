#include "ui/settings/NodeCommSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"
#include "data/Database.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QStyle>
#include <QStorageInfo>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

// ---- 通信/存储配置（device_comm.ini）----
QString deviceCommIniPath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/device_comm.ini");
}

struct DeviceCommConfig
{
    QString topic{QStringLiteral("/camera/image_raw")};
    QString triggerMode{QStringLiteral("soft")}; // soft | hard
    bool autoReconnect{true};
    QString imageRoot{QStringLiteral("D:/MZ-VISION/captured_images/")};
    QString saveMode{QStringLiteral("ng")}; // ng | all | none
    bool autoClean{true};
    int cleanMinGb{50};
    int cleanMaxDays{30};
};

DeviceCommConfig loadDeviceCommConfig()
{
    DeviceCommConfig config;
    QSettings settings(deviceCommIniPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    config.topic = settings.value(QStringLiteral("Camera/Topic"), config.topic).toString().trimmed();
    config.triggerMode = settings.value(QStringLiteral("Camera/TriggerMode"), config.triggerMode).toString();
    config.autoReconnect = settings.value(QStringLiteral("Camera/AutoReconnect"), config.autoReconnect).toBool();
    config.imageRoot = settings.value(QStringLiteral("Storage/ImageRoot"), config.imageRoot).toString();
    config.saveMode = settings.value(QStringLiteral("Storage/SaveMode"), config.saveMode).toString();
    config.autoClean = settings.value(QStringLiteral("Storage/AutoClean"), config.autoClean).toBool();
    config.cleanMinGb = settings.value(QStringLiteral("Storage/CleanMinGb"), config.cleanMinGb).toInt();
    config.cleanMaxDays = settings.value(QStringLiteral("Storage/CleanMaxDays"), config.cleanMaxDays).toInt();
    return config;
}

void saveDeviceCommConfig(const DeviceCommConfig &config, QString *error)
{
    QSettings settings(deviceCommIniPath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue(QStringLiteral("Camera/Topic"), config.topic);
    settings.setValue(QStringLiteral("Camera/TriggerMode"), config.triggerMode);
    settings.setValue(QStringLiteral("Camera/AutoReconnect"), config.autoReconnect);
    settings.setValue(QStringLiteral("Storage/ImageRoot"), config.imageRoot);
    settings.setValue(QStringLiteral("Storage/SaveMode"), config.saveMode);
    settings.setValue(QStringLiteral("Storage/AutoClean"), config.autoClean);
    settings.setValue(QStringLiteral("Storage/CleanMinGb"), config.cleanMinGb);
    settings.setValue(QStringLiteral("Storage/CleanMaxDays"), config.cleanMaxDays);
    settings.sync();
    if (settings.status() != QSettings::NoError && error) {
        *error = QStringLiteral("写入 device_comm.ini 失败：%1").arg(deviceCommIniPath());
    }
}

// ---- 节点就绪检查（保留原有脚本探测逻辑）----
struct NodeReadiness
{
    bool ready{false};
    QStringList missing;
};

QString expandBatchVariable(const QString &value, const QHash<QString, QString> &variables)
{
    QString expanded = value.trimmed();
    for (int pass = 0; pass < 6; ++pass) {
        QString next;
        int cursor = 0;
        bool changed = false;
        const QRegularExpression expression(QStringLiteral("%([A-Za-z0-9_]+)%"));
        QRegularExpressionMatchIterator iterator = expression.globalMatch(expanded);
        while (iterator.hasNext()) {
            const QRegularExpressionMatch match = iterator.next();
            next += expanded.mid(cursor, match.capturedStart() - cursor);
            const QString key = match.captured(1).toUpper();
            if (variables.contains(key)) {
                next += variables.value(key);
                changed = true;
            } else {
                next += match.captured(0);
            }
            cursor = match.capturedEnd();
        }
        next += expanded.mid(cursor);
        expanded = next;
        if (!changed) {
            break;
        }
    }
    return QDir::cleanPath(expanded.remove('"').trimmed());
}

QHash<QString, QString> readBatchVariables(const QString &script, QStringList *warnings)
{
    QHash<QString, QString> variables;
    const QRegularExpression setExpression(
        QStringLiteral("^\\s*set\\s+\\\"([A-Za-z0-9_]+)=(.*)\\\"\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    const QStringList lines = script.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = setExpression.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        variables.insert(match.captured(1).toUpper(), match.captured(2).trimmed());
    }
    for (auto iterator = variables.begin(); iterator != variables.end(); ++iterator) {
        const QString before = iterator.value();
        iterator.value() = expandBatchVariable(before, variables);
        if (iterator.value() == before && before.contains('%')) {
            warnings->append(QStringLiteral("变量 %1 未能完全展开：%2")
                                 .arg(iterator.key(), before));
        }
    }
    return variables;
}

NodeReadiness checkBatchFiles(const QString &script,
                              const QHash<QString, QString> &variables,
                              const QStringList &requiredVariables)
{
    NodeReadiness result;
    if (script.isEmpty()) {
        result.missing << QStringLiteral("未读取启动脚本");
        return result;
    }
    const QRegularExpression guardExpression(
        QStringLiteral("if\\s+not\\s+exist\\s+\\\"([^\\\"]+)\""),
        QRegularExpression::CaseInsensitiveOption);
    QStringList conditions;
    QRegularExpressionMatchIterator guardIterator = guardExpression.globalMatch(script);
    while (guardIterator.hasNext()) {
        conditions << guardIterator.next().captured(1);
    }
    for (const QString &variable : requiredVariables) {
        const QString key = variable.toUpper();
        if (!variables.contains(key)) {
            result.missing << QStringLiteral("脚本缺少变量 %1").arg(variable);
            continue;
        }
        QString condition;
        const QString reference = QStringLiteral("%%%1%%").arg(variable);
        for (const QString &candidate : conditions) {
            if (candidate.contains(reference, Qt::CaseInsensitive)) {
                condition = candidate;
                break;
            }
        }
        if (condition.isEmpty()) {
            result.missing << QStringLiteral("脚本未校验 %1").arg(variable);
            continue;
        }
        const QString path = expandBatchVariable(condition, variables);
        if (path.isEmpty() || path.contains('%')) {
            result.missing << QStringLiteral("%1 检查路径未能展开").arg(variable);
            continue;
        }
        if (!QFileInfo::exists(path)) {
            result.missing << QStringLiteral("文件不存在：%1").arg(QDir::toNativeSeparators(path));
        }
    }
    result.ready = result.missing.isEmpty();
    return result;
}

QString quoteSqliteIdentifier(const QString &identifier)
{
    QString escaped = identifier;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

// 颜色取自当前主题，切换时由 NodeCommSettingsPage::applyTheme() 重刷。
QString cardQss()
{
    return QStringLiteral(
               "QFrame#commCard { background:%1; border:1px solid %2;"
               " border-top:2px solid %3; border-radius:8px; }")
        .arg(Theme::kCardBg, Theme::kBorder, Theme::markColor());
}

QString primaryBtnQss()
{
    return QStringLiteral(
               "QPushButton { background:#1F6FEB; color:#FFFFFF; border:none; border-radius:5px;"
               " padding:6px 16px; font-size:13px; font-weight:bold; }"
               "QPushButton:hover { background:#388BFD; }"
               "QPushButton:pressed { background:#1850B8; }"
               "QPushButton:disabled { background:#174A9A; color:#FFFFFF; }");
}

QString secondaryBtnQss()
{
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:5px;"
               " padding:5px 12px; font-size:13px; }"
               "QPushButton:hover { background:%4; }"
               "QPushButton:pressed { background:%5; }"
               "QPushButton:disabled { color:%6; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder,
             Theme::kHoverBg, Theme::kPressedBg, Theme::kTextMuted);
}

QString segmentBtnQss()
{
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %3;"
               " border-radius:5px; padding:0px 12px; min-height:34px; max-height:34px; font-size:12px; }"
               "QPushButton:checked { background:%4; color:%5; border-color:%5; font-weight:600; }"
               "QPushButton:hover { background:%6; }")
        .arg(Theme::kControlBg, Theme::kTextSecondary, Theme::kBorder,
             Theme::kSelectionBg, Theme::markColor(), Theme::kHoverBg);
}

QString inputQss()
{
    return QStringLiteral(
               "QLineEdit { background:%1; color:%2; border:1px solid %3;"
               " border-radius:5px; padding:0px 8px; min-height:34px; font-size:13px;"
               " selection-background-color:%4; }"
               "QLineEdit:focus { border-color:%4; }"
               "QLineEdit:read-only { color:%5; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder, Theme::kFocusRing, Theme::kTextSecondary);
}

QString spinBoxQss()
{
    return QStringLiteral(
               "QSpinBox { background:%1; color:%2; border:1px solid %3;"
               " border-radius:5px; padding:0px 8px; margin:0;"
               " min-height:34px; max-height:34px; font-size:13px; }"
               "QSpinBox:focus { border-color:%4; }"
               "QSpinBox::up-button, QSpinBox::down-button { width:0; height:0; border:none; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder, Theme::kFocusRing);
}

QString checkQss()
{
    return QStringLiteral(
               "QCheckBox { color:%1; font-size:12px; spacing:6px; }"
               "QCheckBox::indicator { width:15px; height:15px; border-radius:3px;"
               " border:1px solid %2; background:%3; }"
               "QCheckBox::indicator:checked { background:%4; border-color:%4; }")
        .arg(Theme::kTextPrimary, Theme::kBorder, Theme::kControlBg, Theme::kAccent);
}

QString listQss()
{
    return QStringLiteral(
               "QListWidget#nodeList { background:transparent; border:none; outline:none; padding:0; }"
               "QListWidget#nodeList::item { background:transparent; border:none; margin:0; }"
               "QWidget#nodeItem { background:%1; border:1px solid %2; border-radius:6px; }"
               "QWidget#nodeItem:hover { background:%3; border:1px solid %4; }"
               "QWidget#nodeItem[selected=\"true\"] {"
               " background:%5; border:1px solid %6; border-left:3px solid %6; }")
        .arg(Theme::kControlBg, Theme::kBorder, Theme::kHoverBg, Theme::kFocusRing,
             Theme::kSelectionBg, Theme::markColor());
}

QString progressQss(bool warning)
{
    const QString chunk = warning ? QLatin1String(Theme::kStateWarning) : QLatin1String(Theme::kAccent);
    return QStringLiteral(
               "QProgressBar { background:%1; border:1px solid %2; border-radius:5px;"
               " text-align:center; color:%3; font-size:12px; font-weight:bold; min-height:24px; max-height:24px; }"
               "QProgressBar::chunk { background:%4; border-radius:4px; }")
        .arg(Theme::kControlBg, Theme::kBorder, Theme::kTextPrimary, chunk);
}

QString labelQss(const char *color, int size, int weight)
{
    return QStringLiteral("color:%1;font-size:%2px;font-weight:%3;border:none;background:transparent;")
        .arg(QLatin1String(color))
        .arg(size)
        .arg(weight);
}

QLabel *titleLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("commTitle"));
    label->setStyleSheet(labelQss(Theme::kTextPrimary, 14, 600));
    return label;
}

QLabel *fieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("commField"));
    label->setStyleSheet(labelQss(Theme::kTextSecondary, 12, 400));
    return label;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("commHint"));
    label->setStyleSheet(labelQss(Theme::kTextMuted, 11, 400));
    label->setWordWrap(true);
    return label;
}

QLabel *makeStatusDot(const QString &text, const char *color, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("statusDot"));
    label->setStyleSheet(labelQss(color, 11, 600));
    return label;
}

QPushButton *makeSegmentButton(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("segmentBtn"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(34);
    button->setStyleSheet(segmentBtnQss());
    return button;
}

QString stepperBtnQss()
{
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %3;"
               " border-radius:5px; font-size:16px; font-weight:700; padding:0; min-width:34px; max-width:34px;"
               " min-height:34px; max-height:34px; }"
               "QPushButton:hover { background:%4; border-color:%5; }"
               "QPushButton:pressed { background:%6; }"
               "QPushButton:disabled { color:%7; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder,
             Theme::kHoverBg, Theme::markColor(), Theme::kPressedBg, Theme::kTextMuted);
}

// 方案 A：隐藏 QSpinBox 原生步进按钮，改在输入框右侧组合清晰的 − / + 按钮。
QWidget *makeSpinStepper(QSpinBox *spin, QWidget *parent, const QString &suffix, int minWidth)
{
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setFixedHeight(34);
    spin->setMinimumWidth(minWidth);
    spin->setStyleSheet(spinBoxQss());
    spin->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    if (!suffix.isEmpty()) {
        spin->setSuffix(suffix);
    }

    auto *wrapper = new QWidget(parent);
    auto *layout = new QHBoxLayout(wrapper);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *minus = new QPushButton(QStringLiteral("−"), wrapper);
    auto *plus = new QPushButton(QStringLiteral("+"), wrapper);
    for (QPushButton *button : {minus, plus}) {
        button->setFixedSize(34, 34);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setObjectName(QStringLiteral("stepperBtn"));
        button->setStyleSheet(stepperBtnQss());
    }
    layout->addWidget(spin, 1);
    layout->addWidget(minus);
    layout->addWidget(plus);

    QObject::connect(minus, &QPushButton::clicked, spin, [spin]() {
        spin->setValue(spin->value() - spin->singleStep());
    });
    QObject::connect(plus, &QPushButton::clicked, spin, [spin]() {
        spin->setValue(spin->value() + spin->singleStep());
    });
    return wrapper;
}

} // namespace

NodeCommSettingsPage::NodeCommSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    m_config = HikCameraConfig::load(HikCameraConfig::defaultFilePath());
    rebuildUi();
}

QString NodeCommSettingsPage::bindingSummary() const
{
    return QStringLiteral("核心服务启停、海康相机配置与数据存储清理策略。");
}

bool NodeCommSettingsPage::isEditable() const
{
    return true;
}

bool NodeCommSettingsPage::isDirty() const
{
    return m_dirty;
}

void NodeCommSettingsPage::rebuildUi()
{
    setObjectName(QStringLiteral("nodeCommPage"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet(QStringLiteral("QWidget#nodeCommPage { background:%1; }")
                      .arg(Theme::kWindowBg));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 12);
    root->setSpacing(12);

    // ===== 1. 后台核心服务状态条 =====
    auto *serviceBar = new QFrame(this);
    serviceBar->setObjectName(QStringLiteral("serviceBar"));
    serviceBar->setStyleSheet(QStringLiteral(
        "QFrame#serviceBar { background:%1; border:1px solid %2; border-radius:6px; }")
        .arg(Theme::kCardBg, Theme::kBorder));
    serviceBar->setFixedHeight(52);
    auto *serviceLayout = new QHBoxLayout(serviceBar);
    serviceLayout->setContentsMargins(12, 0, 12, 0);
    serviceLayout->setSpacing(10);
    m_serviceSummary = new QLabel(serviceBar);
    m_serviceSummary->setObjectName(QStringLiteral("serviceSummary"));
    m_serviceSummary->setStyleSheet(labelQss(Theme::kTextPrimary, 13, 400));
    m_serviceSummary->setText(QStringLiteral("后台核心服务: 0/3 就绪"));
    serviceLayout->addWidget(m_serviceSummary, 1);
    m_startAllButton = new QPushButton(QStringLiteral("▶ 一键启动服务"), serviceBar);
    m_startAllButton->setObjectName(QStringLiteral("primaryBtn"));
    m_startAllButton->setCursor(Qt::PointingHandCursor);
    m_startAllButton->setStyleSheet(primaryBtnQss());
    serviceLayout->addWidget(m_startAllButton);
    m_redetectButton = new QPushButton(QStringLiteral("↻ 重新检测"), serviceBar);
    m_redetectButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_redetectButton->setCursor(Qt::PointingHandCursor);
    m_redetectButton->setStyleSheet(secondaryBtnQss());
    serviceLayout->addWidget(m_redetectButton);
    root->addWidget(serviceBar);

    connect(m_startAllButton, &QPushButton::clicked, this, [this]() {
        if (m_running) {
            emit stopNodesRequested();
        } else {
            emit startNodesRequested();
        }
    });
    connect(m_redetectButton, &QPushButton::clicked, this, [this]() { refreshNodeReadiness(); });

    // ===== 2. 主工作区：左 32 / 右 68 =====
    auto *workspace = new QHBoxLayout();
    workspace->setContentsMargins(0, 0, 0, 0);
    workspace->setSpacing(12);
    root->addLayout(workspace, 1);

    auto *leftCard = new QFrame(this);
    leftCard->setObjectName(QStringLiteral("commCard"));
    leftCard->setAttribute(Qt::WA_StyledBackground, true);
    leftCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    leftCard->setStyleSheet(cardQss());
    auto *leftPanel = new QVBoxLayout(leftCard);
    leftPanel->setContentsMargins(16, 16, 16, 16);
    leftPanel->setSpacing(12);
    auto *leftTitle = titleLabel(QStringLiteral("🖧 设备与通信节点列表"), leftCard);
    auto *leftSub = hintLabel(QStringLiteral("点击节点切换右侧配置面板"), leftCard);
    leftPanel->addWidget(leftTitle);
    leftPanel->addWidget(leftSub);
    m_nodeList = new QListWidget(leftCard);
    m_nodeList->setObjectName(QStringLiteral("nodeList"));
    m_nodeList->setStyleSheet(listQss());
    m_nodeList->setIconSize(QSize(26, 26));
    m_nodeList->setSpacing(12);
    m_nodeList->setFrameShape(QFrame::NoFrame);
    m_nodeList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_nodeList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    leftPanel->addWidget(m_nodeList, 1);
    workspace->addWidget(leftCard, 32);

    // 节点项：图标 + 名称 + 状态点 + 副标题，每个节点是一张独立圆角卡片。
    struct NodeDef {
        const char *icon;
        const char *name;
        const char *subtitle;
        QLabel **badge;
    };
    const NodeDef nodes[] = {
        {"📷", "3D相机 (海康)", "海康 3D 轮廓相机", &m_cameraNodeBadge},
        {"📡", "相机通信节点", "Topic: /camera/image_raw", &m_topicNodeBadge},
        {"🔌", "PLC 控制系统", "西门子 S7 / Modbus", &m_plcBadge},
        {"🤖", "修磨专机 (CNC/机械臂)", "后端下发工艺参数", &m_cncBadge},
        {"💾", "数据与图像存储", "SQLite + 本地图像", &m_storageBadge},
    };
    for (const NodeDef &node : nodes) {
        auto *itemWidget = new QWidget(m_nodeList);
        itemWidget->setObjectName(QStringLiteral("nodeItem"));
        itemWidget->setAttribute(Qt::WA_StyledBackground, true);
        itemWidget->setAttribute(Qt::WA_Hover, true);
        itemWidget->setProperty("selected", false);
        itemWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto *row = new QVBoxLayout(itemWidget);
        row->setContentsMargins(12, 8, 14, 8);
        row->setSpacing(2);
        auto *line1 = new QHBoxLayout();
        line1->setContentsMargins(0, 0, 0, 0);
        auto *icon = new QLabel(QString::fromUtf8(node.icon), itemWidget);
        icon->setFixedWidth(30);
        icon->setStyleSheet(QStringLiteral("font-size:20px;border:none;background:transparent;"));
        auto *name = new QLabel(QString::fromUtf8(node.name), itemWidget);
        name->setObjectName(QStringLiteral("nodeName"));
        name->setStyleSheet(labelQss(Theme::kTextPrimary, 13, 600));
        *node.badge = makeStatusDot(QStringLiteral("● 未连接"), Theme::kStateError, itemWidget);
        line1->addWidget(icon);
        line1->addWidget(name, 1);
        line1->addWidget(*node.badge);
        row->addLayout(line1);
        if (node.subtitle && *node.subtitle) {
            auto *sub = new QLabel(QString::fromUtf8(node.subtitle), itemWidget);
            sub->setObjectName(QStringLiteral("nodeSub"));
            sub->setStyleSheet(labelQss(Theme::kTextMuted, 11, 400));
            sub->setContentsMargins(30, 0, 0, 0);
            row->addWidget(sub);
        }
        auto *item = new QListWidgetItem(m_nodeList);
        item->setSizeHint(QSize(0, 68));
        m_nodeList->addItem(item);
        m_nodeList->setItemWidget(item, itemWidget);
    }

    // 右侧：相机配置卡 + 数据存储卡（上下堆叠）
    auto *rightPanel = new QVBoxLayout();
    rightPanel->setSpacing(12);
    workspace->addLayout(rightPanel, 68);

    // ---- 相机配置卡 ----
    m_cameraCard = new QFrame(this);
    m_cameraCard->setObjectName(QStringLiteral("commCard"));
    m_cameraCard->setStyleSheet(cardQss());
    auto *cameraLayout = new QVBoxLayout(m_cameraCard);
    cameraLayout->setContentsMargins(16, 16, 16, 16);
    cameraLayout->setSpacing(16);
    cameraLayout->addWidget(titleLabel(QStringLiteral("📷 海康3D相机 · 详细参数配置"), m_cameraCard));

    auto *cameraForm = new QGridLayout();
    cameraForm->setHorizontalSpacing(32);
    cameraForm->setVerticalSpacing(16);
    cameraForm->setColumnStretch(0, 1);
    cameraForm->setColumnStretch(1, 1);

    auto *ipRow = new QHBoxLayout();
    ipRow->setContentsMargins(0, 0, 0, 0);
    ipRow->setSpacing(8);
    ipRow->addWidget(fieldLabel(QStringLiteral("设备 IP 地址"), m_cameraCard));
    m_cameraIp = new QLineEdit(QStringLiteral("192.168.1.64"), m_cameraCard);
    m_cameraIp->setStyleSheet(inputQss());
    m_cameraIp->setMinimumHeight(34);
    m_cameraTestButton = new QPushButton(QStringLiteral("连接测试"), m_cameraCard);
    m_cameraTestButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_cameraTestButton->setFixedHeight(34);
    m_cameraTestButton->setStyleSheet(secondaryBtnQss());
    m_cameraPingButton = new QPushButton(QStringLiteral("Ping"), m_cameraCard);
    m_cameraPingButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_cameraPingButton->setFixedHeight(34);
    m_cameraPingButton->setStyleSheet(secondaryBtnQss());
    ipRow->addWidget(m_cameraIp, 1);
    ipRow->addWidget(m_cameraTestButton);
    ipRow->addWidget(m_cameraPingButton);
    cameraForm->addLayout(ipRow, 0, 0);

    auto *topicRow = new QHBoxLayout();
    topicRow->setContentsMargins(0, 0, 0, 0);
    topicRow->setSpacing(8);
    topicRow->addWidget(fieldLabel(QStringLiteral("绑定节点 Topic"), m_cameraCard));
    m_cameraTopic = new QLineEdit(m_cameraCard);
    m_cameraTopic->setStyleSheet(inputQss());
    m_cameraTopic->setMinimumHeight(34);
    topicRow->addWidget(m_cameraTopic, 1);
    cameraForm->addLayout(topicRow, 0, 1);

    auto *triggerRow = new QHBoxLayout();
    triggerRow->setContentsMargins(0, 0, 0, 0);
    triggerRow->setSpacing(8);
    triggerRow->addWidget(fieldLabel(QStringLiteral("触发模式"), m_cameraCard));
    m_softTriggerButton = makeSegmentButton(QStringLiteral("软触发"), m_cameraCard);
    m_hardTriggerButton = makeSegmentButton(QStringLiteral("硬件触发(PLC)"), m_cameraCard);
    m_triggerModeGroup = new QButtonGroup(m_cameraCard);
    m_triggerModeGroup->setExclusive(true);
    m_triggerModeGroup->addButton(m_softTriggerButton, 0);
    m_triggerModeGroup->addButton(m_hardTriggerButton, 1);
    triggerRow->addWidget(m_softTriggerButton);
    triggerRow->addWidget(m_hardTriggerButton);
    triggerRow->addStretch(1);
    cameraForm->addLayout(triggerRow, 1, 0);

    auto *timeoutRow = new QHBoxLayout();
    timeoutRow->setContentsMargins(0, 0, 0, 0);
    timeoutRow->setSpacing(8);
    timeoutRow->addWidget(fieldLabel(QStringLiteral("连接超时"), m_cameraCard));
    m_timeoutSpin = new QSpinBox(m_cameraCard);
    m_timeoutSpin->setRange(500, 10000);
    m_timeoutSpin->setSingleStep(100);
    m_timeoutSpin->setValue(3000);
    timeoutRow->addWidget(makeSpinStepper(m_timeoutSpin, m_cameraCard, QStringLiteral(" ms"), 120));
    timeoutRow->addStretch(1);
    cameraForm->addLayout(timeoutRow, 1, 1);

    m_autoReconnect = new QCheckBox(QStringLiteral("断线自动重连"), m_cameraCard);
    m_autoReconnect->setStyleSheet(checkQss());
    cameraForm->addWidget(m_autoReconnect, 2, 0, Qt::AlignVCenter);

    auto *formatRow = new QHBoxLayout();
    formatRow->setContentsMargins(0, 0, 0, 0);
    formatRow->setSpacing(8);
    formatRow->addWidget(fieldLabel(QStringLiteral("像素格式 / 采集帧率"), m_cameraCard));
    auto *formatValue = new QLineEdit(QStringLiteral("Mono8 / 3D点云 · 10 Hz"), m_cameraCard);
    formatValue->setReadOnly(true);
    formatValue->setStyleSheet(inputQss());
    formatValue->setMinimumHeight(34);
    formatRow->addWidget(formatValue, 1);
    cameraForm->addLayout(formatRow, 2, 1);
    cameraLayout->addLayout(cameraForm);

    auto *cameraActions = new QHBoxLayout();
    cameraActions->setContentsMargins(0, 0, 0, 0);
    cameraActions->setSpacing(12);
    auto *diagStrip = new QFrame(m_cameraCard);
    diagStrip->setObjectName(QStringLiteral("diagStrip"));
    auto *diagLayout = new QHBoxLayout(diagStrip);
    diagLayout->setContentsMargins(10, 6, 10, 6);
    m_cameraStateHint = hintLabel(QString(), diagStrip);
    m_cameraStateHint->setWordWrap(false);
    diagLayout->addWidget(m_cameraStateHint);
    cameraActions->addWidget(diagStrip, 1);
    m_cameraApplyButton = new QPushButton(QStringLiteral("保存并应用配置"), m_cameraCard);
    m_cameraApplyButton->setObjectName(QStringLiteral("primaryBtn"));
    m_cameraApplyButton->setStyleSheet(primaryBtnQss());
    cameraActions->addWidget(m_cameraApplyButton);
    cameraLayout->addLayout(cameraActions);
    m_cameraCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightPanel->addWidget(m_cameraCard, 0);

    // ---- 数据存储卡 ----
    m_storageCard = new QFrame(this);
    m_storageCard->setObjectName(QStringLiteral("commCard"));
    m_storageCard->setStyleSheet(cardQss());
    auto *storageLayout = new QVBoxLayout(m_storageCard);
    storageLayout->setContentsMargins(16, 16, 16, 16);
    storageLayout->setSpacing(16);
    storageLayout->addWidget(titleLabel(QStringLiteral("💾 数据存储与磁盘自动清理策略"), m_storageCard));

    // SQLite 状态
    auto *dbRow = new QHBoxLayout();
    dbRow->setContentsMargins(0, 0, 0, 0);
    dbRow->addWidget(fieldLabel(QStringLiteral("本地数据库 SQLite"), m_storageCard));
    m_dbStatusValue = makeStatusDot(QStringLiteral("● 已连接"), Theme::kStateSuccess, m_storageCard);
    dbRow->addWidget(m_dbStatusValue);
    dbRow->addStretch(1);
    m_dbViewButton = new QPushButton(QStringLiteral("查看数据表 (0 张)"), m_storageCard);
    m_dbViewButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_dbViewButton->setStyleSheet(secondaryBtnQss());
    dbRow->addWidget(m_dbViewButton);
    storageLayout->addLayout(dbRow);

    // 图像保存根目录
    auto *rootRow = new QHBoxLayout();
    rootRow->setContentsMargins(0, 0, 0, 0);
    rootRow->addWidget(fieldLabel(QStringLiteral("图像保存根目录"), m_storageCard));
    m_imageRoot = new QLineEdit(m_storageCard);
    m_imageRoot->setStyleSheet(inputQss());
    m_browseButton = new QPushButton(QStringLiteral("浏览..."), m_storageCard);
    m_browseButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_browseButton->setStyleSheet(secondaryBtnQss());
    rootRow->addWidget(m_imageRoot, 1);
    rootRow->addWidget(m_browseButton);
    storageLayout->addLayout(rootRow);

    // 存图模式
    auto *saveModeRow = new QHBoxLayout();
    saveModeRow->setContentsMargins(0, 0, 0, 0);
    saveModeRow->addWidget(fieldLabel(QStringLiteral("存图模式"), m_storageCard));
    m_saveNgOnlyButton = makeSegmentButton(QStringLiteral("仅保存NG/粒子图"), m_storageCard);
    m_saveAllButton = makeSegmentButton(QStringLiteral("全部保存"), m_storageCard);
    m_saveNoneButton = makeSegmentButton(QStringLiteral("不保存"), m_storageCard);
    m_saveModeGroup = new QButtonGroup(m_storageCard);
    m_saveModeGroup->setExclusive(true);
    m_saveModeGroup->addButton(m_saveNgOnlyButton, 0);
    m_saveModeGroup->addButton(m_saveAllButton, 1);
    m_saveModeGroup->addButton(m_saveNoneButton, 2);
    saveModeRow->addWidget(m_saveNgOnlyButton);
    saveModeRow->addWidget(m_saveAllButton);
    saveModeRow->addWidget(m_saveNoneButton);
    saveModeRow->addStretch(1);
    storageLayout->addLayout(saveModeRow);

    // D 盘剩余空间
    auto *diskLabel = fieldLabel(QStringLiteral("D 盘剩余空间"), m_storageCard);
    storageLayout->addWidget(diskLabel);
    m_diskBar = new QProgressBar(m_storageCard);
    m_diskBar->setRange(0, 100);
    m_diskBar->setValue(0);
    m_diskBar->setTextVisible(true);
    m_diskBar->setFixedHeight(24);
    m_diskBar->setStyleSheet(progressQss(false));
    storageLayout->addWidget(m_diskBar);

    // 自动清理策略
    m_autoClean = new QCheckBox(QStringLiteral("启用磁盘自动循环清理（防爆盘保护）"), m_storageCard);
    m_autoClean->setStyleSheet(checkQss());
    storageLayout->addWidget(m_autoClean);

    m_cleanMinGbSpin = new QSpinBox(m_storageCard);
    m_cleanMinGbSpin->setRange(1, 999);
    m_cleanMinGbSpin->setValue(50);
    m_cleanMaxDaysSpin = new QSpinBox(m_storageCard);
    m_cleanMaxDaysSpin->setRange(1, 365);
    m_cleanMaxDaysSpin->setValue(30);

    // 参数行保持单行水平对齐，避免末尾文案折行。
    auto *cleanRow = new QHBoxLayout();
    cleanRow->setContentsMargins(0, 0, 0, 0);
    cleanRow->setSpacing(6);
    auto *cleanPrefix = hintLabel(QStringLiteral("触发条件：剩余空间低于"), m_storageCard);
    cleanPrefix->setWordWrap(false);
    cleanRow->addWidget(cleanPrefix);
    cleanRow->addWidget(makeSpinStepper(m_cleanMinGbSpin, m_storageCard, QStringLiteral(" GB"), 95));
    auto *cleanOr = hintLabel(QStringLiteral("或  保存天数超过"), m_storageCard);
    cleanOr->setWordWrap(false);
    cleanRow->addWidget(cleanOr);
    cleanRow->addWidget(makeSpinStepper(m_cleanMaxDaysSpin, m_storageCard, QStringLiteral(" 天"), 95));
    auto *cleanSuffix = hintLabel(QStringLiteral("时，自动覆盖最早数据。"), m_storageCard);
    cleanSuffix->setWordWrap(false);
    cleanRow->addWidget(cleanSuffix);
    cleanRow->addStretch(1);
    storageLayout->addLayout(cleanRow);
    storageLayout->addStretch(1);
    m_storageCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    rightPanel->addWidget(m_storageCard, 1);

    // ---- 数据库表结构弹窗 ----
    m_databaseSchemaDialog = new QDialog(this);
    m_databaseSchemaDialog->setWindowTitle(QStringLiteral("数据表结构"));
    m_databaseSchemaDialog->resize(900, 560);
    auto *schemaDialogLayout = new QVBoxLayout(m_databaseSchemaDialog);
    m_databaseSchemaTable = new QTableWidget(m_databaseSchemaDialog);
    m_databaseSchemaTable->setColumnCount(3);
    m_databaseSchemaTable->setHorizontalHeaderLabels(
        {QStringLiteral("数据表"), QStringLiteral("用途"), QStringLiteral("字段")});
    m_databaseSchemaTable->setMinimumHeight(420);
    m_databaseSchemaTable->setColumnWidth(0, 96);
    m_databaseSchemaTable->setColumnWidth(1, 118);
    schemaDialogLayout->addWidget(m_databaseSchemaTable, 1);

    // ---- 交互连接 ----
    connect(m_cameraTestButton, &QPushButton::clicked, this, [this]() {
        m_cameraStateHint->setText(QStringLiteral("正在连接相机 %1 ...").arg(m_cameraIp->text().trimmed()));
        emit hikCameraConnectRequested();
    });
    connect(m_cameraPingButton, &QPushButton::clicked, this, [this]() {
        const QString host = m_cameraIp->text().trimmed();
        if (host.isEmpty()) {
            m_cameraStateHint->setText(QStringLiteral("IP 为空，无法 Ping。"));
            return;
        }
        m_cameraStateHint->setText(QStringLiteral("正在 Ping %1 ...").arg(host));
        auto *process = new QProcess(this);
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, host](int exitCode) {
            m_cameraStateHint->setText(exitCode == 0
                ? QStringLiteral("Ping %1 成功。").arg(host)
                : QStringLiteral("Ping %1 失败（设备不可达）。").arg(host));
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
            m_cameraStateHint->setText(QStringLiteral("无法执行 ping，请确认系统环境变量。"));
            process->deleteLater();
        });
        process->start(QStringLiteral("ping"), {QStringLiteral("-n"), QStringLiteral("2"), QStringLiteral("-w"), QStringLiteral("1000"), host});
    });
    connect(m_cameraApplyButton, &QPushButton::clicked, this, [this]() {
        emit saveRequested();
    });
    connect(m_browseButton, &QPushButton::clicked, this, [this]() {
        // 文件夹选择：这里保持轻量，直接打开当前目录，真正选择器待接入。
        const QString root = m_imageRoot->text().trimmed();
        if (!root.isEmpty() && QDir(root).exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(root));
        } else {
            m_cameraStateHint->setText(QStringLiteral("图像根目录不存在：%1").arg(root));
        }
    });
    connect(m_dbViewButton, &QPushButton::clicked, this, [this]() {
        reloadFromSource();
        m_databaseSchemaDialog->exec();
    });
    connect(m_nodeList, &QListWidget::currentRowChanged, this, [this](int row) {
        Q_UNUSED(row);
        // 右侧相机配置卡作为默认面板，始终展示；这里只同步节点卡片的选中态。
        for (int i = 0; i < m_nodeList->count(); ++i) {
            auto *item = m_nodeList->item(i);
            auto *card = item ? m_nodeList->itemWidget(item) : nullptr;
            if (!card) {
                continue;
            }
            const bool selected = (i == m_nodeList->currentRow());
            if (card->property("selected").toBool() == selected) {
                continue;
            }
            card->setProperty("selected", selected);
            card->style()->unpolish(card);
            card->style()->polish(card);
            card->update();
        }
    });

    // 配置控件变化 → 标脏
    const auto markDirty = [this]() {
        if (m_loading) {
            return;
        }
        if (!m_dirty) {
            m_dirty = true;
            emit dirtyChanged(true);
        }
    };
    connect(m_cameraIp, &QLineEdit::textChanged, this, markDirty);
    connect(m_cameraTopic, &QLineEdit::textChanged, this, markDirty);
    connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(m_autoReconnect, &QCheckBox::toggled, this, markDirty);
    connect(m_imageRoot, &QLineEdit::textChanged, this, markDirty);
    connect(m_cleanMinGbSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(m_cleanMaxDaysSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(m_autoClean, &QCheckBox::toggled, this, markDirty);
    connect(m_triggerModeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, markDirty);
    connect(m_saveModeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, markDirty);

    // 初始加载
    m_loading = true;
    const DeviceCommConfig comm = loadDeviceCommConfig();
    m_cameraIp->setText(m_config.address.isEmpty() ? QStringLiteral("192.168.1.64") : m_config.address);
    m_cameraTopic->setText(comm.topic);
    m_timeoutSpin->setValue(m_config.loginTimeoutMs > 0 ? m_config.loginTimeoutMs : 3000);
    m_autoReconnect->setChecked(comm.autoReconnect);
    m_imageRoot->setText(comm.imageRoot);
    m_cleanMinGbSpin->setValue(comm.cleanMinGb);
    m_cleanMaxDaysSpin->setValue(comm.cleanMaxDays);
    m_autoClean->setChecked(comm.autoClean);
    (comm.triggerMode == QStringLiteral("hard") ? m_hardTriggerButton : m_softTriggerButton)->setChecked(true);
    if (comm.saveMode == QStringLiteral("all")) {
        m_saveAllButton->setChecked(true);
    } else if (comm.saveMode == QStringLiteral("none")) {
        m_saveNoneButton->setChecked(true);
    } else {
        m_saveNgOnlyButton->setChecked(true);
    }
    m_loading = false;

    m_nodeList->setCurrentRow(0);
    updateServiceStatusBar();
    applyTheme();
    Theme::watch(this, [this]() { applyTheme(); });
}

void NodeCommSettingsPage::applyTheme()
{
    setStyleSheet(QStringLiteral("QWidget#nodeCommPage { background:%1; }").arg(Theme::kWindowBg));
    if (QFrame *bar = findChild<QFrame *>(QStringLiteral("serviceBar"))) {
        bar->setStyleSheet(QStringLiteral(
                               "QFrame#serviceBar { background:%1; border:1px solid %2; border-radius:6px; }")
                               .arg(Theme::kCardBg, Theme::kBorder));
    }
    const auto restyle = [](QWidget *widget, const QString &sheet) {
        if (widget) {
            widget->setStyleSheet(sheet);
        }
    };
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("commTitle"))) {
        restyle(label, labelQss(Theme::kTextPrimary, 14, 600));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("commField"))) {
        restyle(label, labelQss(Theme::kTextSecondary, 12, 400));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("commHint"))) {
        restyle(label, labelQss(Theme::kTextMuted, 11, 400));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("nodeName"))) {
        restyle(label, labelQss(Theme::kTextPrimary, 13, 600));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("nodeSub"))) {
        restyle(label, labelQss(Theme::kTextMuted, 11, 400));
    }
    restyle(m_serviceSummary, labelQss(Theme::kTextPrimary, 13, 400));
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("primaryBtn"))) {
        restyle(button, primaryBtnQss());
    }
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("secondaryBtn"))) {
        restyle(button, secondaryBtnQss());
    }
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("segmentBtn"))) {
        restyle(button, segmentBtnQss());
    }
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("stepperBtn"))) {
        restyle(button, stepperBtnQss());
    }
    for (QFrame *card : findChildren<QFrame *>(QStringLiteral("commCard"))) {
        restyle(card, cardQss());
    }
    for (QLineEdit *edit : findChildren<QLineEdit *>()) {
        restyle(edit, inputQss());
    }
    for (QSpinBox *spin : findChildren<QSpinBox *>()) {
        restyle(spin, spinBoxQss());
    }
    if (QFrame *strip = findChild<QFrame *>(QStringLiteral("diagStrip"))) {
        strip->setStyleSheet(QStringLiteral(
            "QFrame#diagStrip { background:%1; border:1px solid %2; border-radius:5px; }")
            .arg(Theme::kControlBg, Theme::kBorder));
    }
    for (QCheckBox *box : findChildren<QCheckBox *>()) {
        restyle(box, checkQss());
    }
    if (m_nodeList) {
        m_nodeList->setStyleSheet(listQss());
        for (int i = 0; i < m_nodeList->count(); ++i) {
            QWidget *card = m_nodeList->itemWidget(m_nodeList->item(i));
            if (!card) {
                continue;
            }
            card->style()->unpolish(card);
            card->style()->polish(card);
            card->update();
        }
    }
    if (m_diskBar) {
        m_diskBar->setStyleSheet(QString());
    }
    refreshNodeReadiness();
    reloadFromSource();
}

void NodeCommSettingsPage::updateServiceStatusBar()
{
    if (m_startAllButton) {
        m_startAllButton->setText(m_running ? QStringLiteral("⏹ 停止服务") : QStringLiteral("▶ 一键启动服务"));
        m_startAllButton->setEnabled(!m_busy);
    }
}

void NodeCommSettingsPage::updateNodeList()
{
    auto setBadge = [](QLabel *badge, const QString &text, const QString &color) {
        if (badge) {
            badge->setText(text);
            badge->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:600;border:none;background:transparent;")
                                     .arg(color));
        }
    };
    if (m_cameraNodeBadge) {
        setBadge(m_cameraNodeBadge,
                 m_hikConnected ? QStringLiteral("● 已连接") : QStringLiteral("● 未连接"),
                 m_hikConnected ? QLatin1String(Theme::kStateSuccess) : QLatin1String(Theme::kStateError));
    }
    if (m_topicNodeBadge) {
        const bool hasPublisher = m_rosBridgeRunning && m_cameraPublisherCount > 0;
        setBadge(m_topicNodeBadge,
                 hasPublisher ? QStringLiteral("● 已发布") : QStringLiteral("● 无发布者"),
                 hasPublisher ? QLatin1String(Theme::kStateSuccess) : QLatin1String(Theme::kStateError));
    }
    if (m_plcBadge) {
        setBadge(m_plcBadge,
                 QStringLiteral("● %1").arg(m_plcStatusText.isEmpty() ? QStringLiteral("待反馈") : m_plcStatusText),
                 QLatin1String(Theme::kTextMuted));
    }
    if (m_cncBadge) {
        setBadge(m_cncBadge, QStringLiteral("● 离线"), QLatin1String(Theme::kTextMuted));
    }
    if (m_storageBadge) {
        setBadge(m_storageBadge, QStringLiteral("● 正常"), QLatin1String(Theme::kStateSuccess));
    }
}

void NodeCommSettingsPage::refreshNodeReadiness()
{
    const QString scriptPath = resolveNodeLauncherScript(QStringLiteral("StartAll"));
    QString script;
    if (!scriptPath.isEmpty()) {
        QFile file(scriptPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            script = QString::fromLocal8Bit(file.readAll());
        }
    }

    QStringList variableWarnings;
    const QHash<QString, QString> variables = readBatchVariables(script, &variableWarnings);
    const NodeReadiness camera = checkBatchFiles(
        script, variables, {QStringLiteral("CAMERA_EXE"),
                            QStringLiteral("CAMERA_IFACES"),
                            QStringLiteral("CAMERA_SENSOR"),
                            QStringLiteral("CAMERA_CONFIG"),
                            QStringLiteral("CAMERA_SICK_CSV"),
                            QStringLiteral("ROS_LOCAL_SETUP")});
    const NodeReadiness backend = checkBatchFiles(
        script, variables, {QStringLiteral("BACKEND_EXE"),
                            QStringLiteral("BACKEND_LOCAL_SETUP"),
                            QStringLiteral("ROS_LOCAL_SETUP")});
    const NodeReadiness algorithm = checkBatchFiles(script, variables, {QStringLiteral("ALGO_EXE")});

    const int readyCount = int(camera.ready) + int(backend.ready) + int(algorithm.ready);
    // 总状态灯：3/3 才绿色；0/3 红色；部分就绪橙色。
    const QString dotColor = readyCount == 3 ? QLatin1String(Theme::kStateSuccess)
                                             : (readyCount == 0 ? QLatin1String(Theme::kStateError) : QLatin1String(Theme::kStateWarning));
    const QString readyText =
        QStringLiteral("<span style='color:%1;font-size:13px;'>●</span> 后台核心服务: %2/3 就绪")
            .arg(dotColor)
            .arg(readyCount);
    const auto readyName = [](bool ready) { return ready ? QStringLiteral("🟢已就绪") : QStringLiteral("🔴未就绪"); };
    const QString detail = QStringLiteral(" (相机服务: %1 | 后端服务: %2 | 算法服务: %3)")
                               .arg(readyName(camera.ready), readyName(backend.ready), readyName(algorithm.ready));
    m_serviceSummary->setText(readyText + detail);
    m_serviceSummary->setToolTip(
        QStringList{
            QStringLiteral("相机：%1").arg(camera.missing.isEmpty() ? QStringLiteral("通过") : camera.missing.join(QStringLiteral("；"))),
            QStringLiteral("后端：%1").arg(backend.missing.isEmpty() ? QStringLiteral("通过") : backend.missing.join(QStringLiteral("；"))),
            QStringLiteral("算法：%1").arg(algorithm.missing.isEmpty() ? QStringLiteral("通过") : algorithm.missing.join(QStringLiteral("；"))),
        }.join(QStringLiteral("\n")));
    if (m_startAllButton) {
        m_startAllButton->setEnabled(!m_busy && readyCount == 3 && !m_running);
        m_startAllButton->setToolTip(readyCount == 3
            ? QStringLiteral("启动相机、后端和算法服务")
            : QStringLiteral("前置检查未通过，请先查看未就绪项"));
    }
}

void NodeCommSettingsPage::refreshDiskUsage()
{
    if (!m_diskBar) {
        return;
    }
    QString root = m_imageRoot->text().trimmed();
    QStorageInfo storage(root.isEmpty() ? QDir::rootPath() : root);
    if (!storage.isValid() || storage.bytesTotal() <= 0) {
        storage.setPath(QDir::rootPath());
    }
    const qint64 total = storage.bytesTotal();
    const qint64 available = storage.bytesAvailable();
    const qint64 used = total - available;
    const double usedRatio = total > 0 ? double(used) / double(total) : 0.0;
    const int percent = int(std::lround(usedRatio * 100.0));

    const auto gb = [](qint64 bytes) {
        return QString::number(double(bytes) / (1024.0 * 1024.0 * 1024.0), 'f', 1);
    };
    const bool warning = percent >= 85;
    if (m_diskBar->property("warning").toBool() != warning || m_diskBar->styleSheet().isEmpty()) {
        m_diskBar->setProperty("warning", warning);
        m_diskBar->setStyleSheet(progressQss(warning));
    }
    m_diskBar->setValue(percent);
    m_diskBar->setFormat(QStringLiteral("%1 GB / %2 GB (已用 %3%)")
                             .arg(gb(available), gb(total))
                             .arg(percent));
}

void NodeCommSettingsPage::refreshHikCameraInfo()
{
    if (m_cameraStateHint) {
        QString validationError;
        const bool ok = m_config.enabled && m_config.validate(&validationError);
        m_cameraStateHint->setText(ok
            ? QStringLiteral("配置已就绪，可以发起连接测试。")
            : (m_config.enabled ? validationError
                                : QStringLiteral("海康相机未启用，请填写 hik_camera.ini 后连接测试。")));
    }
}

void NodeCommSettingsPage::updateCameraNodeDiagnostics()
{
    if (m_topicNodeBadge) {
        const bool hasPublisher = m_rosBridgeRunning && m_cameraPublisherCount > 0;
        m_topicNodeBadge->setText(hasPublisher ? QStringLiteral("● 已发布") : QStringLiteral("● 无发布者"));
        m_topicNodeBadge->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:600;border:none;background:transparent;")
                                            .arg(hasPublisher ? QLatin1String(Theme::kStateSuccess) : QLatin1String(Theme::kStateError)));
    }
}

void NodeCommSettingsPage::reloadFromSource()
{
    refreshDiskUsage();
    refreshHikCameraInfo();
    updateCameraNodeDiagnostics();
    updateNodeList();

    // 数据库健康与表数量
    QString health;
    const bool healthy = Database::instance().healthCheck(&health);
    const QSqlDatabase db = Database::instance().handle();
    int tableCount = 0;
    QStringList tableNames;
    if (db.isOpen()) {
        QSqlQuery tableQuery(db);
        if (tableQuery.exec(QStringLiteral(
                "SELECT name FROM sqlite_master WHERE type='table' "
                "AND name NOT LIKE 'sqlite_%' ORDER BY name"))) {
            while (tableQuery.next()) {
                tableNames.append(tableQuery.value(0).toString());
            }
        }
        tableCount = tableNames.size();
    }
    if (m_dbStatusValue) {
        m_dbStatusValue->setText(healthy ? QStringLiteral("● 已连接") : QStringLiteral("● 异常"));
        m_dbStatusValue->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:600;border:none;background:transparent;")
                                           .arg(healthy ? QLatin1String(Theme::kStateSuccess) : QLatin1String(Theme::kStateError)));
        m_dbStatusValue->setToolTip(healthy ? QStringLiteral("SQLite 连接正常") : health);
    }
    if (m_dbViewButton) {
        m_dbViewButton->setText(QStringLiteral("查看数据表 (%1 张)").arg(tableCount));
    }
    // 填充表结构弹窗
    if (m_databaseSchemaTable) {
        static const QHash<QString, QString> descriptions{
            {QStringLiteral("AppSetting"), QStringLiteral("应用设置项")},
            {QStringLiteral("AuditLog"), QStringLiteral("用户操作审计")},
            {QStringLiteral("InspectionFrame"), QStringLiteral("扫描帧元数据")},
            {QStringLiteral("InspectionRecord"), QStringLiteral("检测任务与汇总")},
            {QStringLiteral("ParticleDetection"), QStringLiteral("颗粒位置与状态")},
            {QStringLiteral("ProcessParameter"), QStringLiteral("当前工艺参数")},
            {QStringLiteral("ProcessParameterHistory"), QStringLiteral("任务工艺快照")},
            {QStringLiteral("UserAccount"), QStringLiteral("用户与角色")},
            {QStringLiteral("UserSession"), QStringLiteral("登录会话")},
        };
        m_databaseSchemaTable->setRowCount(tableNames.size());
        for (int row = 0; row < tableNames.size(); ++row) {
            const QString &tableName = tableNames.at(row);
            QStringList fields;
            QSqlQuery columns(db);
            if (db.isOpen()
                && columns.exec(QStringLiteral("PRAGMA table_info(%1)").arg(quoteSqliteIdentifier(tableName)))) {
                while (columns.next()) {
                    const QString name = columns.value(1).toString();
                    const QString type = columns.value(2).toString().trimmed();
                    fields.append(type.isEmpty() ? name : QStringLiteral("%1 %2").arg(name, type));
                }
            }
            auto *tableItem = new QTableWidgetItem(tableName);
            tableItem->setToolTip(tableName);
            m_databaseSchemaTable->setItem(row, 0, tableItem);
            auto *descriptionItem = new QTableWidgetItem(
                descriptions.value(tableName, QStringLiteral("业务数据表")));
            descriptionItem->setToolTip(descriptionItem->text());
            m_databaseSchemaTable->setItem(row, 1, descriptionItem);
            auto *fieldsItem = new QTableWidgetItem(fields.join(QStringLiteral("、")));
            fieldsItem->setToolTip(fields.join(QStringLiteral("、")));
            fieldsItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_databaseSchemaTable->setItem(row, 2, fieldsItem);
            m_databaseSchemaTable->setRowHeight(row, 46);
        }
    }
}

SettingsSaveOutcome NodeCommSettingsPage::save()
{
    const DeviceCommConfig comm = loadDeviceCommConfig();
    DeviceCommConfig next;
    next.topic = m_cameraTopic->text().trimmed();
    next.triggerMode = m_hardTriggerButton->isChecked() ? QStringLiteral("hard") : QStringLiteral("soft");
    next.autoReconnect = m_autoReconnect->isChecked();
    next.imageRoot = m_imageRoot->text().trimmed();
    next.saveMode = m_saveAllButton->isChecked() ? QStringLiteral("all")
                                                  : (m_saveNoneButton->isChecked() ? QStringLiteral("none")
                                                                                   : QStringLiteral("ng"));
    next.autoClean = m_autoClean->isChecked();
    next.cleanMinGb = m_cleanMinGbSpin->value();
    next.cleanMaxDays = m_cleanMaxDaysSpin->value();
    Q_UNUSED(comm);

    QString deviceError;
    saveDeviceCommConfig(next, &deviceError);

    // 相机 IP 与超时写回 hik_camera.ini（保留端口/用户名/密码等未暴露字段）。
    HikCameraConfig camera = m_config;
    camera.address = m_cameraIp->text().trimmed();
    camera.loginTimeoutMs = m_timeoutSpin->value();
    QString cameraError;
    const bool cameraOk = camera.save(HikCameraConfig::defaultFilePath(), &cameraError);

    m_dirty = false;
    emit dirtyChanged(false);

    SettingsSaveOutcome outcome;
    if (!cameraOk && !deviceError.isEmpty()) {
        outcome.status = SettingsSaveStatus::Rejected;
        outcome.message = QStringLiteral("相机配置：%1；通信配置：%2").arg(cameraError, deviceError);
    } else if (!cameraOk) {
        outcome.status = SettingsSaveStatus::SavedWithWarning;
        outcome.message = QStringLiteral("通信配置已保存，但相机配置写入失败：%1").arg(cameraError);
    } else if (!deviceError.isEmpty()) {
        outcome.status = SettingsSaveStatus::SavedWithWarning;
        outcome.message = QStringLiteral("相机配置已保存，但通信配置写入失败：%1").arg(deviceError);
    } else {
        outcome.status = SettingsSaveStatus::Saved;
        outcome.message = QStringLiteral("相机与通信配置已保存。");
    }
    m_config = camera;
    refreshHikCameraInfo();
    return outcome;
}

void NodeCommSettingsPage::revert()
{
    m_loading = true;
    m_config = HikCameraConfig::load(HikCameraConfig::defaultFilePath());
    const DeviceCommConfig comm = loadDeviceCommConfig();
    m_cameraIp->setText(m_config.address.isEmpty() ? QStringLiteral("192.168.1.64") : m_config.address);
    m_cameraTopic->setText(comm.topic);
    m_timeoutSpin->setValue(m_config.loginTimeoutMs > 0 ? m_config.loginTimeoutMs : 3000);
    m_autoReconnect->setChecked(comm.autoReconnect);
    m_imageRoot->setText(comm.imageRoot);
    m_cleanMinGbSpin->setValue(comm.cleanMinGb);
    m_cleanMaxDaysSpin->setValue(comm.cleanMaxDays);
    m_autoClean->setChecked(comm.autoClean);
    (comm.triggerMode == QStringLiteral("hard") ? m_hardTriggerButton : m_softTriggerButton)->setChecked(true);
    if (comm.saveMode == QStringLiteral("all")) {
        m_saveAllButton->setChecked(true);
    } else if (comm.saveMode == QStringLiteral("none")) {
        m_saveNoneButton->setChecked(true);
    } else {
        m_saveNgOnlyButton->setChecked(true);
    }
    m_loading = false;
    m_dirty = false;
    emit dirtyChanged(false);
    refreshHikCameraInfo();
}

void NodeCommSettingsPage::restoreDefaults()
{
    m_loading = true;
    m_cameraIp->setText(QStringLiteral("192.168.1.64"));
    m_cameraTopic->setText(QStringLiteral("/camera/image_raw"));
    m_timeoutSpin->setValue(3000);
    m_autoReconnect->setChecked(true);
    m_imageRoot->setText(QStringLiteral("D:/MZ-VISION/captured_images/"));
    m_cleanMinGbSpin->setValue(50);
    m_cleanMaxDaysSpin->setValue(30);
    m_autoClean->setChecked(true);
    m_softTriggerButton->setChecked(true);
    m_saveNgOnlyButton->setChecked(true);
    m_loading = false;
    m_dirty = true;
    emit dirtyChanged(true);
}

QString NodeCommSettingsPage::pendingChangeSummary() const
{
    return QStringLiteral("IP=%1，Topic=%2，超时=%3ms，存图=%4，清理阈值=%5GB/%6天")
        .arg(m_cameraIp->text().trimmed(),
             m_cameraTopic->text().trimmed())
        .arg(m_timeoutSpin->value())
        .arg(m_saveAllButton->isChecked() ? QStringLiteral("全部")
                                          : (m_saveNoneButton->isChecked() ? QStringLiteral("不保存") : QStringLiteral("仅NG")))
        .arg(m_cleanMinGbSpin->value())
        .arg(m_cleanMaxDaysSpin->value());
}

void NodeCommSettingsPage::setNodesRunning(bool running)
{
    m_running = running;
    updateServiceStatusBar();
}

void NodeCommSettingsPage::setNodesBusy(bool busy)
{
    m_busy = busy;
    updateServiceStatusBar();
}

void NodeCommSettingsPage::setCameraNodeStatus(bool connected, const QString &detail)
{
    m_cameraNodeConnected = connected;
    m_cameraStatusMessage = detail.trimmed();
    m_cameraLastMessageAt = QDateTime::currentDateTime();
    updateNodeList();
}

void NodeCommSettingsPage::setCameraTopicDiagnostics(bool bridgeRunning, int publisherCount)
{
    m_rosBridgeRunning = bridgeRunning;
    m_cameraPublisherCount = publisherCount;
    updateCameraNodeDiagnostics();
}

void NodeCommSettingsPage::setHikCameraStatus(bool connected, const QString &detail)
{
    m_hikConnected = connected;
    if (m_cameraStateHint) {
        m_cameraStateHint->setText(connected
            ? QStringLiteral("海康相机已连接：%1").arg(detail)
            : QStringLiteral("海康相机未连接：%1").arg(detail.isEmpty() ? QStringLiteral("等待连接") : detail));
    }
    updateNodeList();
}

void NodeCommSettingsPage::setPlcDeviceStatus(const QString &status, const QString &detail)
{
    m_plcStatusText = status.trimmed().isEmpty() ? QStringLiteral("待反馈") : status.trimmed();
    m_plcStatusDetail = detail;
    updateNodeList();
}
