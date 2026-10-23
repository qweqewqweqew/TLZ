#include "ui/settings/NodeCommSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "camera/HikCameraConfig.h"
#include "data/Database.h"
#include "ui/settings/SettingsFormWidgets.h"

#include "ElaPushButton.h"

#include <QCoreApplication>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QToolButton>
#include <QProcessEnvironment>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace {

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
        QStringLiteral("if\\s+not\\s+exist\\s+\\\"([^\\\"]+)\\\""),
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

QFrame *makeReadinessCard(const QString &title,
                          QLabel **status,
                          QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("nodeReadinessCard"));
    Theme::bindStyle(card, []() {
        return QStringLiteral("QFrame#nodeReadinessCard { background:%1; border:1px solid %2;"
                              " border-radius:5px; }")
            .arg(Theme::kPanelBg, Theme::kBorder);
    });
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(5);
    auto *name = settingsSectionHeading(title, card);
    layout->addWidget(name);
    *status = makeLabel(QStringLiteral("等待检测"));
    (*status)->setParent(card);
    (*status)->setWordWrap(false);
    (*status)->setStyleSheet(QStringLiteral("font-size:19px;font-weight:700;color:%1;border:none;background:transparent;")
                                 .arg(Theme::kTextMuted));
    layout->addWidget(*status);
    card->setMinimumHeight(70);
    return card;
}

void setReadinessLabel(QLabel *label, bool ready, const QString &detail)
{
    if (!label) {
        return;
    }
    label->setText(ready ? QStringLiteral("● 准备就绪") : QStringLiteral("● 未就绪"));
    label->setToolTip(ready ? QStringLiteral("前置检查已通过") : detail);
    label->setStyleSheet(QStringLiteral(
                             "color:%1;font-size:19px;font-weight:700;border:none;background:transparent;")
                             .arg(ready ? Theme::kStateSuccess : Theme::kStateError));
}

void styleReadinessCard(QFrame *card, bool ready)
{
    if (!card) {
        return;
    }
    card->setStyleSheet(QStringLiteral(
                            "QFrame#nodeReadinessCard { background:%1; border:1px solid %2;"
                            " border-radius:5px; }")
                            .arg(Theme::kPanelBg, ready ? Theme::kStateSuccess : Theme::kBorder));
}

void applyOutlinedButtonStyle(ElaPushButton *button)
{
    if (!button) {
        return;
    }
    button->setBorderRadius(5);
    button->setLightDefaultColor(QColor(Theme::kPanelBg));
    button->setDarkDefaultColor(QColor(Theme::kControlBg));
    button->setLightHoverColor(QColor(Theme::kHoverBg));
    button->setDarkHoverColor(QColor(Theme::kHoverBg));
    button->setLightPressColor(QColor(Theme::kPressedBg));
    button->setDarkPressColor(QColor(Theme::kPressedBg));
    button->setLightTextColor(QColor(Theme::markColor()));
    button->setDarkTextColor(QColor(Theme::kTextPrimary));
    button->setStyleSheet(QStringLiteral("QPushButton { border:1px solid %1; border-radius:5px; }")
                              .arg(Theme::markColor()));
}

QString quoteSqliteIdentifier(const QString &identifier)
{
    QString escaped = identifier;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

void styleSchemaTable(QTableWidget *table)
{
    if (!table) {
        return;
    }
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setShowGrid(false);
    table->setWordWrap(true);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(72);
    table->setStyleSheet(QStringLiteral(
        "QTableWidget { background:%1; color:%2; font-size:11px; border:1px solid %3; border-radius:5px;"
        " alternate-background-color:%4; gridline-color:%3; }"
        "QTableWidget::item { padding:2px 5px; border-bottom:1px solid %3; }"
        "QHeaderView::section { background:%5; color:%2; border:none; border-bottom:1px solid %3;"
        " padding:5px 6px; font-weight:600; }")
                             .arg(Theme::kPanelBg,
                                  Theme::kTextPrimary,
                                  Theme::kBorder,
                                  Theme::kCanvasBg,
                                  Theme::kControlBg));
}

QFrame *makeDeviceStatusRow(const QString &title,
                            QLabel **status,
                            QToolButton **toggle,
                            QWidget **details,
                            QWidget *parent,
                            const QString &initialText)
{
    auto *row = new QFrame(parent);
    row->setObjectName(QStringLiteral("deviceStatusRow"));
    row->setMinimumHeight(62);
    Theme::bindStyle(row, []() {
        return QStringLiteral(
                   "QFrame#deviceStatusRow { background:%1; color:%2;"
                   " border:1px solid %3; border-left:3px solid %4; border-radius:5px; }")
            .arg(Theme::kPanelBg, Theme::kTextPrimary, Theme::kBorder, Theme::kStateUnknown);
    });
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(9, 6, 9, 7);
    layout->setSpacing(5);
    auto *header = new QWidget(row);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    *toggle = new QToolButton(header);
    (*toggle)->setText(title);
    (*toggle)->setArrowType(Qt::RightArrow);
    (*toggle)->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    (*toggle)->setCursor(Qt::PointingHandCursor);
    (*toggle)->setCheckable(true);
    Theme::bindStyle(*toggle, []() {
        return QStringLiteral(
                   "QToolButton { color:%1; font-size:13px; font-weight:600; border:none; background:transparent;"
                   " text-align:left; padding:3px 2px; }"
                   "QToolButton:checked { color:%3; background:%4; border-radius:4px; }"
                   "QToolButton:checked:hover { color:%3; background:%5; }"
                   "QToolButton:checked:pressed { color:%3; background:%6; }"
                   "QToolButton:hover { color:%2; }")
            .arg(Theme::kTextPrimary,
                 Theme::markColor(),
                 Theme::kSelectionText,
                 Theme::kSelectionBg,
                 Theme::kSelectedHoverBg,
                 Theme::kSelectedPressedBg);
    });
    headerLayout->addWidget(*toggle, 1);
    *status = makeLabel(initialText);
    (*status)->setParent(row);
    (*status)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    (*status)->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;color:%1;border:none;background:transparent;")
                                 .arg(Theme::kStateUnknown));
    headerLayout->addWidget(*status);
    layout->addWidget(header);
    *details = new QWidget(row);
    (*details)->setObjectName(QStringLiteral("deviceStatusDetails"));
    Theme::bindStyle(*details, []() {
        return QStringLiteral("QWidget#deviceStatusDetails { background:%1; border:none; border-top:1px solid %2; }")
            .arg(Theme::kPanelBg, Theme::kBorder);
    });
    (*details)->setVisible(false);
    layout->addWidget(*details);
    return row;
}

QLabel *addDeviceInfoLine(QVBoxLayout *layout, const QString &key, QWidget *parent)
{
    auto *line = new QWidget(parent);
    line->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    auto *row = new QHBoxLayout(line);
    row->setContentsMargins(1, 0, 1, 0);
    row->setSpacing(10);
    auto *name = makeLabel(key);
    name->setFixedWidth(82);
    name->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                            .arg(Theme::kTextMuted));
    auto *value = makeLabel(QStringLiteral("—"));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setWordWrap(true);
    value->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    value->setMinimumWidth(0);
    value->setStyleSheet(QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
                             .arg(Theme::kTextPrimary));
    row->addWidget(name);
    row->addWidget(value, 1);
    layout->addWidget(line);
    return value;
}

void setDeviceStatusLabel(QLabel *label, QWidget *card, const QString &text, const QString &color, const QString &detail)
{
    if (!label) {
        return;
    }
    label->setText(text);
    label->setToolTip(detail);
    label->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;color:%1;border:none;background:transparent;")
                             .arg(color));
    if (!card) {
        for (QWidget *candidate = label->parentWidget(); candidate; candidate = candidate->parentWidget()) {
            if (candidate->objectName() == QStringLiteral("deviceStatusRow")) {
                card = candidate;
                break;
            }
        }
    }
    if (card) {
        card->setStyleSheet(QStringLiteral(
                                "QFrame#deviceStatusRow { background:%1; color:%2;"
                                " border:1px solid %3; border-left:3px solid %4; border-radius:5px; }")
                                .arg(Theme::kPanelBg, Theme::kTextPrimary, Theme::kBorder, color));
    }
}

} // namespace

NodeCommSettingsPage::NodeCommSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    auto *servicePanel = settingsGroupPanel(QStringLiteral("节点服务"),
                                             QStringLiteral(":/img/nav_settings.svg"), this);
    root->addWidget(servicePanel, 1);
    servicePanel->setMinimumWidth(0);
    servicePanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    // 节点启停：原先在顶栏，按设计方案移到设置页，顶栏只保留设备在线状态。
    m_nodeState = makeLabel(QString());
    m_nodeState->setStyleSheet(QStringLiteral("color:%1;font-size:16px;border:none;background:transparent;")
                                   .arg(Theme::kTextSecondary));
    auto *control = new QWidget(servicePanel);
    auto *controlLayout = new QVBoxLayout(control);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(9);
    auto *controlHeading = new QHBoxLayout();
    controlHeading->addWidget(settingsSectionHeading(QStringLiteral("服务运行状态"), control));
    controlHeading->addStretch(1);
    controlHeading->addWidget(m_nodeState);
    controlLayout->addLayout(controlHeading);
    auto *controlRow = new QHBoxLayout();
    controlRow->setContentsMargins(0, 0, 0, 0);
    controlRow->setSpacing(8);
    m_startButton = new ElaPushButton(QStringLiteral("启动节点"), control);
    m_startButton->setMinimumHeight(36);
    applyPrimaryButtonStyle(m_startButton);
    m_stopButton = new ElaPushButton(QStringLiteral("暂停节点"), control);
    m_stopButton->setMinimumHeight(36);
    applyOutlinedButtonStyle(m_stopButton);
    auto *readinessHeading = new QHBoxLayout();
    readinessHeading->setContentsMargins(0, 4, 0, 0);
    readinessHeading->addWidget(settingsSectionHeading(QStringLiteral("节点状态"), control));
    readinessHeading->addStretch(1);
    m_checkButton = new ElaPushButton(QStringLiteral("重新检测"), control);
    m_checkButton->setMinimumHeight(32);
    applyOutlinedButtonStyle(m_checkButton);
    controlLayout->addLayout(readinessHeading);
    auto *readinessList = new QVBoxLayout();
    readinessList->setContentsMargins(0, 0, 0, 0);
    readinessList->setSpacing(8);
    m_cameraCard = makeReadinessCard(QStringLiteral("相机服务节点"),
                                     &m_cameraReadiness,
                                     control);
    m_backendCard = makeReadinessCard(QStringLiteral("后端服务节点"),
                                      &m_backendReadiness,
                                      control);
    m_algorithmCard = makeReadinessCard(QStringLiteral("算法服务节点"),
                                         &m_algorithmReadiness,
                                         control);
    readinessList->addWidget(m_cameraCard, 1);
    readinessList->addWidget(m_backendCard, 1);
    readinessList->addWidget(m_algorithmCard, 1);
    controlLayout->addLayout(readinessList, 1);
    m_readinessSummary = settingsHint(QString(), control);
    m_readinessSummary->setWordWrap(false);
    controlLayout->addWidget(m_readinessSummary);
    controlLayout->addStretch(1);
    controlRow->addWidget(m_startButton, 1);
    controlRow->addWidget(m_stopButton, 1);
    controlRow->addWidget(m_checkButton, 1);
    controlLayout->addLayout(controlRow);
    qobject_cast<QVBoxLayout *>(servicePanel->layout())->addWidget(control, 1);
    connect(m_startButton, &ElaPushButton::clicked, this, &NodeCommSettingsPage::startNodesRequested);
    connect(m_stopButton, &ElaPushButton::clicked, this, &NodeCommSettingsPage::stopNodesRequested);
    connect(m_checkButton, &ElaPushButton::clicked, this, &NodeCommSettingsPage::checkNodeReadiness);
    updateNodeControls();

    auto *storagePanel = settingsGroupPanel(QStringLiteral("数据存储"),
                                             QStringLiteral(":/img/kpi_check.svg"), this);
    root->addWidget(storagePanel, 1);
    storagePanel->setMinimumWidth(0);
    storagePanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *storageLayout = qobject_cast<QVBoxLayout *>(storagePanel->layout());
    m_databaseSummary = new QWidget(storagePanel);
    auto *databaseLayout = new QVBoxLayout(m_databaseSummary);
    databaseLayout->setContentsMargins(0, 0, 0, 0);
    databaseLayout->setSpacing(7);
    auto *tableHeading = new QHBoxLayout();
    tableHeading->setContentsMargins(0, 0, 0, 0);
    tableHeading->addWidget(settingsSectionHeading(QStringLiteral("数据表结构"), m_databaseSummary));
    tableHeading->addStretch(1);
    m_databaseStatusValue = settingsHint(QStringLiteral("读取中"), m_databaseSummary);
    m_databaseStatusValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tableHeading->addWidget(m_databaseStatusValue);
    databaseLayout->addLayout(tableHeading);
    m_databaseSchemaDialog = new QDialog(this);
    m_databaseSchemaDialog->setWindowTitle(QStringLiteral("数据表结构"));
    m_databaseSchemaDialog->setMinimumSize(720, 420);
    m_databaseSchemaDialog->resize(900, 560);
    auto *schemaDialogLayout = new QVBoxLayout(m_databaseSchemaDialog);
    m_databaseSchemaTable = new QTableWidget(m_databaseSchemaDialog);
    m_databaseSchemaTable->setColumnCount(3);
    m_databaseSchemaTable->setHorizontalHeaderLabels(
        {QStringLiteral("数据表"), QStringLiteral("用途"), QStringLiteral("字段")});
    m_databaseSchemaTable->setMinimumHeight(360);
    m_databaseSchemaTable->setColumnWidth(0, 96);
    m_databaseSchemaTable->setColumnWidth(1, 118);
    styleSchemaTable(m_databaseSchemaTable);
    schemaDialogLayout->addWidget(m_databaseSchemaTable, 1);
    m_databaseSchemaButton = new ElaPushButton(QStringLiteral("查看数据表"), m_databaseSummary);
    m_databaseSchemaButton->setMinimumHeight(32);
    applyOutlinedButtonStyle(m_databaseSchemaButton);
    databaseLayout->addWidget(m_databaseSchemaButton, 0, Qt::AlignLeft);
    connect(m_databaseSchemaButton, &ElaPushButton::clicked, this, [this]() {
        refreshDatabaseSchema();
        m_databaseSchemaDialog->exec();
    });
    storageLayout->addWidget(m_databaseSummary, 1);

    auto *imageStorage = new QFrame(storagePanel);
    imageStorage->setObjectName(QStringLiteral("imageStorageCard"));
    Theme::bindStyle(imageStorage, []() {
        return QStringLiteral("QFrame#imageStorageCard { background:%1; border:1px solid %2; border-radius:5px; }")
            .arg(Theme::kPanelBg, Theme::kBorder);
    });
    auto *imageLayout = new QVBoxLayout(imageStorage);
    imageLayout->setContentsMargins(10, 8, 10, 8);
    imageLayout->setSpacing(6);
    auto *imageHeader = new QHBoxLayout();
    imageHeader->setContentsMargins(0, 0, 0, 0);
    imageHeader->addWidget(settingsSectionHeading(QStringLiteral("图像存储"), imageStorage));
    imageHeader->addStretch(1);
    m_imageStorageValue = settingsHint(QStringLiteral("暂无记录"), imageStorage);
    m_imageStorageValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    imageHeader->addWidget(m_imageStorageValue);
    imageLayout->addLayout(imageHeader);
    auto *imageAction = new QHBoxLayout();
    imageAction->setContentsMargins(0, 0, 0, 0);
    auto *imagePathHint = settingsHint(QStringLiteral("路径来自检测记录中的图像字段"), imageStorage);
    imagePathHint->setWordWrap(false);
    imageAction->addWidget(imagePathHint, 1);
    m_openImageFolderButton = new ElaPushButton(QStringLiteral("查看图像目录"), imageStorage);
    m_openImageFolderButton->setMinimumHeight(30);
    applyOutlinedButtonStyle(m_openImageFolderButton);
    imageAction->addWidget(m_openImageFolderButton);
    imageLayout->addLayout(imageAction);
    storageLayout->addWidget(imageStorage);
    connect(m_openImageFolderButton, &ElaPushButton::clicked, this, [this]() {
        const QString path = m_openImageFolderButton->toolTip();
        if (!path.isEmpty() && QDir(path).exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });

    auto *commPanel = settingsGroupPanel(QStringLiteral("设备状态"),
                                          QStringLiteral(":/img/nav_detect.svg"), this);
    root->addWidget(commPanel, 1);
    commPanel->setMinimumWidth(0);
    commPanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *commLayout = qobject_cast<QVBoxLayout *>(commPanel->layout());
    auto *cameraNodeRow = makeDeviceStatusRow(QStringLiteral("相机节点"),
                                               &m_cameraNodeStatus,
                                               &m_cameraNodeToggle,
                                               &m_cameraNodeDetails,
                                               commPanel,
                                               QStringLiteral("等待"));
    auto *hikCameraRow = makeDeviceStatusRow(QStringLiteral("海康相机"),
                                              &m_hikCameraStatus,
                                             &m_hikCameraToggle,
                                             &m_hikCameraDetails,
                                              commPanel,
                                              QStringLiteral("未连接"));
    auto *plcRow = makeDeviceStatusRow(QStringLiteral("PLC"),
                                        &m_plcStatus,
                                      &m_plcToggle,
                                      &m_plcDetails,
                                        commPanel,
                                        QStringLiteral("待反馈"));

    auto *cameraDetailsLayout = new QVBoxLayout(m_cameraNodeDetails);
    cameraDetailsLayout->setContentsMargins(4, 6, 4, 2);
    cameraDetailsLayout->setSpacing(4);
    auto *cameraTopic = addDeviceInfoLine(cameraDetailsLayout, QStringLiteral("订阅 Topic"), m_cameraNodeDetails);
    cameraTopic->setText(QStringLiteral("/camera/status"));
    cameraTopic->setStyleSheet(QStringLiteral("font-family:Consolas,monospace;color:%1;font-size:11px;border:none;background:transparent;")
                                   .arg(Theme::kTextPrimary));
    auto *cameraNodeName = addDeviceInfoLine(cameraDetailsLayout, QStringLiteral("本机节点"), m_cameraNodeDetails);
    cameraNodeName->setText(QStringLiteral("/mz_tlz"));
    auto *rosDomain = addDeviceInfoLine(cameraDetailsLayout, QStringLiteral("Domain ID"), m_cameraNodeDetails);
    rosDomain->setText(QProcessEnvironment::systemEnvironment().value(QStringLiteral("ROS_DOMAIN_ID"),
                                                                       QStringLiteral("0")));
    auto *cameraType = addDeviceInfoLine(cameraDetailsLayout, QStringLiteral("消息类型"), m_cameraNodeDetails);
    cameraType->setText(QStringLiteral("mz_interfaces/msg/CameraStatus"));
    cameraType->setToolTip(cameraType->text());
    cameraType->setStyleSheet(QStringLiteral("font-family:Consolas,monospace;color:%1;font-size:10px;border:none;background:transparent;")
                                  .arg(Theme::kTextPrimary));
    auto *cameraQos = addDeviceInfoLine(cameraDetailsLayout, QStringLiteral("QoS"), m_cameraNodeDetails);
    cameraQos->setText(QStringLiteral("Reliable · Transient Local · Depth 1"));
    m_cameraPublisherCount = addDeviceInfoLine(cameraDetailsLayout, QStringLiteral("发布者"), m_cameraNodeDetails);
    m_cameraLastMessage = addDeviceInfoLine(cameraDetailsLayout, QStringLiteral("最近状态"), m_cameraNodeDetails);
    m_cameraDiagnosis = settingsHint(QString(), m_cameraNodeDetails);
    m_cameraDiagnosis->setWordWrap(true);
    cameraDetailsLayout->addWidget(m_cameraDiagnosis);

    auto *hikDetailsLayout = new QVBoxLayout(m_hikCameraDetails);
    hikDetailsLayout->setContentsMargins(4, 6, 4, 2);
    hikDetailsLayout->setSpacing(4);
    m_hikAddress = addDeviceInfoLine(hikDetailsLayout, QStringLiteral("IP 地址"), m_hikCameraDetails);
    m_hikEndpoint = addDeviceInfoLine(hikDetailsLayout, QStringLiteral("服务端口"), m_hikCameraDetails);
    m_hikUsername = addDeviceInfoLine(hikDetailsLayout, QStringLiteral("用户名"), m_hikCameraDetails);
    m_hikStream = addDeviceInfoLine(hikDetailsLayout, QStringLiteral("预览参数"), m_hikCameraDetails);
    m_hikConfigState = settingsHint(QString(), m_hikCameraDetails);
    m_hikConfigState->setWordWrap(true);
    hikDetailsLayout->addWidget(m_hikConfigState);
    auto *hikActions = new QHBoxLayout();
    hikActions->setContentsMargins(0, 3, 0, 0);
    hikActions->addStretch(1);
    m_hikConnectButton = new ElaPushButton(QStringLiteral("连接相机"), m_hikCameraDetails);
    m_hikConnectButton->setMinimumHeight(30);
    applyPrimaryButtonStyle(m_hikConnectButton);
    m_hikDisconnectButton = new ElaPushButton(QStringLiteral("断开"), m_hikCameraDetails);
    m_hikDisconnectButton->setMinimumHeight(30);
    applyOutlinedButtonStyle(m_hikDisconnectButton);
    hikActions->addWidget(m_hikConnectButton);
    hikActions->addWidget(m_hikDisconnectButton);
    hikDetailsLayout->addLayout(hikActions);

    // PLC 暂无明细字段，占位撑开详情区，展开高度与上方海康相机卡片一致。
    auto *plcDetailsLayout = new QVBoxLayout(m_plcDetails);
    plcDetailsLayout->setContentsMargins(4, 6, 4, 2);
    auto *plcPlaceholder = new QWidget(m_plcDetails);
    plcPlaceholder->setMinimumHeight(qMax(140, hikDetailsLayout->sizeHint().height()));
    plcDetailsLayout->addWidget(plcPlaceholder);

    connect(m_cameraNodeToggle, &QToolButton::toggled, this, [this](bool expanded) {
        m_cameraNodeToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        animateWidgetVisibility(m_cameraNodeDetails, expanded);
    });
    connect(m_hikCameraToggle, &QToolButton::toggled, this, [this](bool expanded) {
        m_hikCameraToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        animateWidgetVisibility(m_hikCameraDetails, expanded);
    });
    connect(m_plcToggle, &QToolButton::toggled, this, [this](bool expanded) {
        m_plcToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        animateWidgetVisibility(m_plcDetails, expanded);
    });
    connect(m_hikConnectButton, &ElaPushButton::clicked, this, [this]() {
        emit hikCameraConnectRequested();
    });
    connect(m_hikDisconnectButton, &ElaPushButton::clicked, this, [this]() {
        emit hikCameraDisconnectRequested();
        setHikCameraStatus(false, QStringLiteral("已从设备状态中断开预览"));
    });
    commLayout->addWidget(cameraNodeRow);
    commLayout->addWidget(hikCameraRow);
    commLayout->addWidget(plcRow);
    commLayout->addStretch(1);
    setDeviceStatusLabel(m_cameraNodeStatus,
                         nullptr,
                         QStringLiteral("等待状态"),
                         QString::fromLatin1(Theme::kStateUnknown),
                         QStringLiteral("等待 camera/status"));
    setHikCameraStatus(false, QStringLiteral("尚未启动海康预览"));
    setPlcDeviceStatus(QStringLiteral("待反馈"), QStringLiteral("PLC 状态数据来源待后端补充"));
    updateCameraNodeDiagnostics();
    refreshHikCameraInfo();
    reloadFromSource();
    Theme::watch(this, [this]() {
        applyPrimaryButtonStyle(m_startButton);
        applyOutlinedButtonStyle(m_stopButton);
        applyOutlinedButtonStyle(m_checkButton);
        applyOutlinedButtonStyle(m_databaseSchemaButton);
        applyOutlinedButtonStyle(m_openImageFolderButton);
        applyPrimaryButtonStyle(m_hikConnectButton);
        applyOutlinedButtonStyle(m_hikDisconnectButton);
        styleSchemaTable(m_databaseSchemaTable);
        setDeviceStatusLabel(m_cameraNodeStatus,
                             nullptr,
                             m_cameraStatusReceived
                                 ? (m_cameraNodeConnected ? QStringLiteral("在线") : QStringLiteral("离线"))
                                 : QStringLiteral("等待状态"),
                             m_cameraStatusReceived
                                 ? QString::fromLatin1(m_cameraNodeConnected ? Theme::kStateSuccess : Theme::kStateError)
                                 : QString::fromLatin1(Theme::kStateUnknown),
                             m_cameraStatusMessage);
        setDeviceStatusLabel(m_hikCameraStatus,
                             nullptr,
                             m_hikConnected ? QStringLiteral("在线")
                                            : (m_hikConnecting ? QStringLiteral("连接中") : QStringLiteral("未连接")),
                             QString::fromLatin1(m_hikConnected ? Theme::kStateSuccess
                                                                 : (m_hikConnecting ? Theme::kStateWarning
                                                                                    : Theme::kStateError)),
                             m_hikCameraStatus ? m_hikCameraStatus->toolTip() : QString());
        if (m_plcStatus) {
            setPlcDeviceStatus(m_plcStatus->text(), m_plcStatus->toolTip());
        }
        updateCameraNodeDiagnostics();
        refreshHikCameraInfo();
        updateNodeControls();
        reloadFromSource();
    });
}

QString NodeCommSettingsPage::bindingSummary() const
{
    return QStringLiteral("节点启停、数据库结构与相机/PLC 设备状态；PLC 状态来源待后端补充。");
}

void NodeCommSettingsPage::setNodesRunning(bool running)
{
    m_running = running;
    updateNodeControls();
}

void NodeCommSettingsPage::setNodesBusy(bool busy)
{
    m_busy = busy;
    updateNodeControls();
}

void NodeCommSettingsPage::setCameraNodeStatus(bool connected, const QString &detail)
{
    m_cameraStatusReceived = true;
    m_cameraNodeConnected = connected;
    m_cameraStatusMessage = detail.trimmed();
    m_cameraLastMessageAt = QDateTime::currentDateTime();
    setDeviceStatusLabel(m_cameraNodeStatus,
                         nullptr,
                         connected ? QStringLiteral("在线") : QStringLiteral("离线"),
                         QString::fromLatin1(connected ? Theme::kStateSuccess : Theme::kStateError),
                         detail);
    updateCameraNodeDiagnostics();
}

void NodeCommSettingsPage::setCameraTopicDiagnostics(bool bridgeRunning, int publisherCount)
{
    m_rosBridgeRunning = bridgeRunning;
    m_cameraStatusPublisherCount = publisherCount;
    updateCameraNodeDiagnostics();
}

void NodeCommSettingsPage::setHikCameraStatus(bool connected, const QString &detail)
{
    m_hikConnected = connected;
    m_hikConnecting = !connected && detail.contains(QStringLiteral("正在连接"));
    setDeviceStatusLabel(m_hikCameraStatus,
                         nullptr,
                         connected ? QStringLiteral("在线")
                                   : (m_hikConnecting ? QStringLiteral("连接中") : QStringLiteral("未连接")),
                         QString::fromLatin1(connected ? Theme::kStateSuccess
                                                       : (m_hikConnecting ? Theme::kStateWarning
                                                                          : Theme::kStateError)),
                         detail);
    if (m_hikConfigState) {
        const QString text = connected
            ? QStringLiteral("已登录并建立实时预览。最近反馈：%1").arg(detail)
            : QStringLiteral("连接状态：%1").arg(detail.isEmpty() ? QStringLiteral("未连接") : detail);
        m_hikConfigState->setText(text);
    }
    if (m_hikConnectButton) {
        m_hikConnectButton->setEnabled(!connected && !m_hikConnecting);
        m_hikConnectButton->setText(connected ? QStringLiteral("已连接")
                                              : (m_hikConnecting ? QStringLiteral("连接中…")
                                                                 : QStringLiteral("连接相机")));
    }
    if (m_hikDisconnectButton) {
        m_hikDisconnectButton->setEnabled(connected);
    }
}

void NodeCommSettingsPage::setPlcDeviceStatus(const QString &status, const QString &detail)
{
    const QString normalized = status.trimmed().isEmpty() ? QStringLiteral("待反馈") : status.trimmed();
    const bool online = normalized.contains(QStringLiteral("连接"))
                        || normalized.contains(QStringLiteral("在线"));
    const bool fault = normalized.contains(QStringLiteral("故障"))
                       || normalized.contains(QStringLiteral("错误"));
    setDeviceStatusLabel(m_plcStatus,
                         nullptr,
                         normalized,
                         fault ? QString::fromLatin1(Theme::kStateError)
                               : (online ? QString::fromLatin1(Theme::kStateSuccess)
                                          : QString::fromLatin1(Theme::kStateUnknown)),
                         detail);
}

void NodeCommSettingsPage::refreshNodeReadiness()
{
    checkNodeReadiness();
}

void NodeCommSettingsPage::updateNodeControls()
{
    if (m_nodeState) {
        QString text;
        QString color;
        if (m_busy) {
            text = QStringLiteral("● 脚本执行中…");
            color = Theme::kStateWarning;
        } else if (m_running) {
            text = QStringLiteral("● 节点运行中");
            color = Theme::kStateSuccess;
        } else {
            text = QStringLiteral("● 节点未运行");
            color = Theme::kStateError;
        }
        m_nodeState->setText(text);
        m_nodeState->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:600;border:none;background:transparent;")
                                       .arg(color));
    }
    if (m_startButton) {
        m_startButton->setEnabled(!m_busy && !m_running && m_allReady);
        m_startButton->setToolTip(m_allReady
                                      ? QStringLiteral("启动相机、后端和算法服务")
                                      : QStringLiteral("启动前置检查未通过，请先查看未就绪项"));
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(!m_busy && m_running);
    }
}

void NodeCommSettingsPage::updateCameraNodeDiagnostics()
{
    if (!m_cameraPublisherCount || !m_cameraLastMessage || !m_cameraDiagnosis) {
        return;
    }

    if (!m_rosBridgeRunning) {
        m_cameraPublisherCount->setText(QStringLiteral("ROS 2 订阅器未启动"));
        m_cameraLastMessage->setText(QStringLiteral("无"));
        m_cameraDiagnosis->setText(QStringLiteral("ROS 2 bridge 尚未启动；检查 ROS 2 初始化、工作区环境及启动日志。"));
        setDeviceStatusLabel(m_cameraNodeStatus,
                             nullptr,
                             QStringLiteral("ROS 2 未启动"),
                             QString::fromLatin1(Theme::kStateError),
                             m_cameraDiagnosis->text());
        return;
    }

    if (m_cameraStatusPublisherCount == 0) {
        m_cameraPublisherCount->setText(QStringLiteral("0 个"));
        if (m_cameraStatusReceived) {
            m_cameraLastMessage->setText(QStringLiteral("%1 · %2")
                                             .arg(m_cameraLastMessageAt.toString(QStringLiteral("HH:mm:ss")),
                                                  m_cameraStatusMessage.simplified()));
        } else {
            m_cameraLastMessage->setText(QStringLiteral("未收到"));
        }
        m_cameraDiagnosis->setText(QStringLiteral(
            "当前没有匹配的 /camera/status 发布者。检查相机节点是否启动、双方 ROS_DOMAIN_ID 是否一致，以及 topic 名称和 QoS 是否兼容。"));
        setDeviceStatusLabel(m_cameraNodeStatus,
                             nullptr,
                             QStringLiteral("无发布者"),
                             QString::fromLatin1(Theme::kStateError),
                             m_cameraDiagnosis->text());
        return;
    }

    if (m_cameraStatusPublisherCount > 0) {
        m_cameraPublisherCount->setText(QStringLiteral("%1 个").arg(m_cameraStatusPublisherCount));
    } else {
        m_cameraPublisherCount->setText(QStringLiteral("正在查询…"));
    }

    if (!m_cameraStatusReceived) {
        m_cameraLastMessage->setText(QStringLiteral("尚无消息"));
        m_cameraDiagnosis->setText(QStringLiteral(
            "topic 已有匹配发布者，但尚未收到状态消息。确认发布端持续发送 CameraStatus；本端 QoS 为 Reliable、Transient Local、Depth 1。"));
        setDeviceStatusLabel(m_cameraNodeStatus,
                             nullptr,
                             QStringLiteral("等待状态"),
                             QString::fromLatin1(Theme::kStateWarning),
                             m_cameraDiagnosis->text());
        return;
    }

    const QString time = m_cameraLastMessageAt.isValid()
        ? m_cameraLastMessageAt.toString(QStringLiteral("HH:mm:ss"))
        : QStringLiteral("—");
    const QString lastMessage = m_cameraStatusMessage.isEmpty()
        ? (m_cameraNodeConnected ? QStringLiteral("connected=true") : QStringLiteral("connected=false"))
        : m_cameraStatusMessage;
    m_cameraLastMessage->setText(QStringLiteral("%1 · %2")
                                     .arg(time, lastMessage.simplified()));
    m_cameraLastMessage->setToolTip(m_cameraStatusMessage);
    m_cameraDiagnosis->setText(m_cameraNodeConnected
        ? QStringLiteral("已收到相机状态，camera/status 报告设备在线。")
        : QStringLiteral("已收到状态消息，但 connected=false。检查相机节点日志、设备供电/网线及相机登录参数。"));
    setDeviceStatusLabel(m_cameraNodeStatus,
                         nullptr,
                         m_cameraNodeConnected ? QStringLiteral("在线") : QStringLiteral("相机离线"),
                         QString::fromLatin1(m_cameraNodeConnected ? Theme::kStateSuccess : Theme::kStateError),
                         m_cameraDiagnosis->text());
}

void NodeCommSettingsPage::refreshHikCameraInfo()
{
    QString loadError;
    const HikCameraConfig config = HikCameraConfig::load(HikCameraConfig::defaultFilePath(), &loadError);
    if (m_hikAddress) {
        m_hikAddress->setText(config.address.isEmpty() ? QStringLiteral("未配置") : config.address);
    }
    if (m_hikEndpoint) {
        m_hikEndpoint->setText(config.address.isEmpty()
                                   ? QStringLiteral("未配置")
                                   : QStringLiteral("%1:%2").arg(config.address).arg(config.port));
    }
    if (m_hikUsername) {
        m_hikUsername->setText(config.username.isEmpty() ? QStringLiteral("未配置") : config.username);
    }
    if (m_hikStream) {
        m_hikStream->setText(QStringLiteral("通道 %1 · %2 · %3")
                                 .arg(config.channel)
                                 .arg(config.streamType == 0 ? QStringLiteral("主码流") : QStringLiteral("子码流"))
                                 .arg(config.linkMode == 0 ? QStringLiteral("TCP") : QStringLiteral("UDP")));
    }
    if (!m_hikConnected && m_hikConfigState) {
        QString state;
        if (!loadError.isEmpty()) {
            state = loadError;
        } else if (!config.enabled) {
            state = QStringLiteral("网络预览未启用；「实时预览」配置入口待后续接入。配置文件：%1")
                        .arg(QDir::toNativeSeparators(HikCameraConfig::defaultFilePath()));
        } else {
            QString validationError;
            state = config.validate(&validationError)
                ? QStringLiteral("配置已启用，可以发起连接。")
                : validationError;
        }
        m_hikConfigState->setText(state);
    }
    if (m_hikConnectButton) {
        m_hikConnectButton->setEnabled(!m_hikConnected && !m_hikConnecting && config.enabled);
    }
    if (m_hikDisconnectButton) {
        m_hikDisconnectButton->setEnabled(m_hikConnected);
    }
}

void NodeCommSettingsPage::checkNodeReadiness()
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
    const NodeReadiness algorithm = checkBatchFiles(
        script, variables, {QStringLiteral("ALGO_EXE")});

    const auto joinMissing = [](const NodeReadiness &result) {
        return result.missing.join(QStringLiteral("；"));
    };
    setReadinessLabel(m_cameraReadiness, camera.ready, joinMissing(camera));
    setReadinessLabel(m_backendReadiness, backend.ready, joinMissing(backend));
    setReadinessLabel(m_algorithmReadiness, algorithm.ready, joinMissing(algorithm));
    m_cameraReadiness->setToolTip(camera.ready ? QStringLiteral("前置检查已通过") : joinMissing(camera));
    m_backendReadiness->setToolTip(backend.ready ? QStringLiteral("前置检查已通过") : joinMissing(backend));
    m_algorithmReadiness->setToolTip(algorithm.ready ? QStringLiteral("前置检查已通过") : joinMissing(algorithm));
    styleReadinessCard(m_cameraCard, camera.ready);
    styleReadinessCard(m_backendCard, backend.ready);
    styleReadinessCard(m_algorithmCard, algorithm.ready);

    const int readyCount = int(camera.ready) + int(backend.ready) + int(algorithm.ready);
    const bool allReady = readyCount == 3;
    m_allReady = allReady;
    if (m_readinessSummary) {
        m_readinessSummary->setText(QStringLiteral("%1 / 3 就绪").arg(readyCount));
        m_readinessSummary->setStyleSheet(
            QStringLiteral("color:%1;font-size:12px;font-weight:600;border:none;background:transparent;")
                .arg(allReady ? Theme::kStateSuccess : Theme::kStateWarning));
    }
    if (m_readinessSummary) {
        QStringList diagnosticDetails;
        diagnosticDetails << (scriptPath.isEmpty()
                                  ? QStringLiteral("脚本：未找到")
                                  : QStringLiteral("脚本：%1").arg(QDir::toNativeSeparators(scriptPath)));
        diagnosticDetails.append(variableWarnings);
        m_readinessSummary->setToolTip(diagnosticDetails.join(QStringLiteral("\n")));
    }
    if (m_startButton) {
        m_startButton->setEnabled(!m_busy && !m_running && allReady);
        m_startButton->setToolTip(allReady
                                      ? QStringLiteral("启动相机、后端和算法服务")
                                      : QStringLiteral("启动前置检查未通过，请先查看未就绪项"));
    }
}

void NodeCommSettingsPage::refreshDatabaseSchema()
{
    if (!m_databaseSchemaTable) {
        return;
    }

    const QSqlDatabase db = Database::instance().handle();
    if (!db.isOpen()) {
        if (m_databaseStatusValue) {
            m_databaseStatusValue->setText(QStringLiteral("显示内置表结构"));
        }
    }

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
    }

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
        tableItem->setFont(QFont(QString(), 10, QFont::DemiBold));
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
        m_databaseSchemaTable->setRowHeight(row, 56);
    }

    if (m_databaseStatusValue) {
        m_databaseStatusValue->setText(QStringLiteral("%1 张表 · 仅结构")
                                           .arg(tableNames.size()));
    }
}

void NodeCommSettingsPage::reloadFromSource()
{
    refreshHikCameraInfo();
    updateCameraNodeDiagnostics();
    checkNodeReadiness();
    QString health;
    const bool healthy = Database::instance().healthCheck(&health);
    if (!healthy && m_databaseStatusValue) {
        m_databaseStatusValue->setToolTip(health.isEmpty() ? QStringLiteral("数据库健康检查失败") : health);
    }
    refreshDatabaseSchema();

    if (!m_imageStorageValue || !m_openImageFolderButton) {
        return;
    }

    int imageRecords = 0;
    QSet<QString> availableImagePaths;
    QString imageFolder;
    const QSqlDatabase db = Database::instance().handle();
    if (db.isOpen()) {
        QSqlQuery paths(db);
        if (paths.exec(QStringLiteral(
                "SELECT OverviewImagePath FROM InspectionRecord "
                "WHERE OverviewImagePath IS NOT NULL AND trim(OverviewImagePath) <> '' "
                "ORDER BY Id DESC"))) {
            while (paths.next()) {
                ++imageRecords;
                QString path = paths.value(0).toString().trimmed();
                QFileInfo info(path);
                if (info.isRelative()) {
                    path = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(path);
                    info.setFile(path);
                }
                if (info.exists() && info.isFile()) {
                    availableImagePaths.insert(QDir::cleanPath(info.absoluteFilePath()).toCaseFolded());
                }
                if (imageFolder.isEmpty() && info.dir().exists()) {
                    imageFolder = info.dir().absolutePath();
                }
            }
        }
    }

    if (imageRecords > 0) {
        m_imageStorageValue->setText(QStringLiteral("%1 条图像记录 · %2 个文件可用")
                                         .arg(imageRecords)
                                         .arg(availableImagePaths.size()));
    } else {
        m_imageStorageValue->setText(QStringLiteral("暂无图像记录"));
    }
    const bool folderAvailable = !imageFolder.isEmpty() && QDir(imageFolder).exists();
    m_openImageFolderButton->setEnabled(folderAvailable);
    m_openImageFolderButton->setToolTip(
        folderAvailable
            ? QDir::toNativeSeparators(imageFolder)
            : QStringLiteral("检测记录还没有图像路径；需后端写入 OverviewImagePath 后才能定位实际目录。"));
    m_imageStorageValue->setToolTip(m_openImageFolderButton->toolTip());
}
