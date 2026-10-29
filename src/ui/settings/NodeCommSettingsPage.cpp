#include "ui/settings/NodeCommSettingsPage.h"

#include "app/RuntimeIniPaths.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "data/Database.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSizePolicy>
#include <QSlider>
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
#include <QTimer>
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

// 所有颜色都来自语义主题；切换深浅色时由 applyTheme() 统一刷新。
QString navCardQss()
{
    return QStringLiteral(
               "QFrame#nodeNavCard { background:%1; border:1px solid %2; border-radius:8px; }")
        .arg(Theme::kCardBg, Theme::kBorder);
}

QString contentCardQss()
{
    return QStringLiteral(
               "QFrame#commCard { background:%1; border:1px solid %2; border-radius:8px; }")
        .arg(Theme::kCardBg, Theme::kBorder);
}

QString primaryBtnQss()
{
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %1; border-radius:6px;"
               " padding:6px 16px; min-height:36px; font-size:13px; font-weight:600; }"
               "QPushButton:hover { background:%3; border-color:%3; }"
               "QPushButton:pressed { background:%4; border-color:%4; }"
               "QPushButton:focus { border:2px solid %5; }"
               "QPushButton:disabled { background:%6; color:%7; border-color:%8; }")
        .arg(Theme::kAccentFill,
             Theme::kOnAccentFill,
             QColor(Theme::kAccentFill).darker(112).name(),
             QColor(Theme::kAccentFill).darker(128).name(),
             Theme::kFocusRing,
             Theme::kControlBg,
             Theme::kTextMuted,
             Theme::kBorder);
}

QString secondaryBtnQss()
{
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:6px;"
               " padding:5px 13px; min-height:36px; font-size:13px; }"
               "QPushButton:hover { background:%4; border-color:%5; color:%2; }"
               "QPushButton:pressed { background:%6; }"
               "QPushButton:focus { border:2px solid %5; }"
               "QPushButton:disabled { background:%1; color:%7; border-color:%3; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder,
             Theme::kHoverBg, Theme::kFocusRing, Theme::kPressedBg, Theme::kTextMuted);
}

QString segmentBtnQss()
{
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %3;"
               " border-radius:6px; padding:0px 14px; min-height:36px; max-height:36px; font-size:13px; }"
               "QPushButton:checked { background:%4; color:%5; border:1px solid %6; font-weight:600; }"
               "QPushButton:hover:!checked { background:%7; color:%8; }"
               "QPushButton:focus { border:2px solid %6; }"
               "QPushButton:disabled { color:%9; }"
               "QPushButton:disabled:checked { background:%4; color:%5; border:1px solid %6; font-weight:600; }")
        .arg(Theme::kControlBg, Theme::kTextSecondary, Theme::kBorder,
             Theme::kSelectionBg, Theme::markColor(), Theme::kFocusRing,
             Theme::kHoverBg, Theme::kTextPrimary, Theme::kTextMuted);
}

QString inputQss()
{
    return QStringLiteral(
               "QLineEdit { background:%1; color:%2; border:1px solid %3;"
               " border-radius:6px; padding:0px 11px; min-height:36px; font-size:13px;"
               " selection-background-color:%4; }"
               "QLineEdit:hover { border-color:%5; }"
               "QLineEdit:focus { border:2px solid %4; }"
               "QLineEdit:read-only { color:%6; background:%7; }"
               "QLineEdit:disabled { color:%6; background:%7; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder, Theme::kFocusRing,
             Theme::kTextMuted, Theme::kTextMuted, Theme::kHoverBg);
}

QString spinBoxQss(int height = 36, int horizontalPadding = 10)
{
    return QStringLiteral(
               "QSpinBox, QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
               " border-radius:6px; padding:0px %7px; margin:0;"
               " min-height:%8px; max-height:%8px; font-size:13px; }"
               "QSpinBox:hover, QDoubleSpinBox:hover { border-color:%5; }"
               "QSpinBox:focus, QDoubleSpinBox:focus { border:2px solid %4; }"
               "QSpinBox:disabled, QDoubleSpinBox:disabled { color:%5; background:%6; }"
               "QSpinBox::up-button, QSpinBox::down-button,"
               "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width:0; height:0; border:none; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder,
             Theme::kFocusRing, Theme::kTextMuted, Theme::kHoverBg)
        .arg(horizontalPadding)
        .arg(height);
}

QString comboBoxQss()
{
    return QStringLiteral(
               "QComboBox { background:%1; color:%2; border:1px solid %3; border-radius:6px;"
               " padding:0px 10px; min-height:36px; max-height:36px; font-size:13px; }"
               "QComboBox:hover { border-color:%4; }"
               "QComboBox:focus { border:2px solid %4; }"
               "QComboBox:disabled { color:%5; background:%6; }"
               "QComboBox::drop-down { width:28px; border:none; }"
               "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
               " selection-background-color:%7; selection-color:%2; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder, Theme::kFocusRing,
             Theme::kTextMuted, Theme::kHoverBg, Theme::kSelectionBg);
}

QString checkQss()
{
    return QStringLiteral("QCheckBox { color:%1; font-size:13px; spacing:8px; }")
        .arg(Theme::kTextPrimary);
}

QString nodeButtonQss()
{
    return QStringLiteral(
               "QPushButton#nodeItem { background:%1; border:1px solid %2;"
               " border-radius:8px; padding:0; text-align:left; }"
               "QPushButton#nodeItem:hover:!checked { background:%3; border-color:%4; }"
               "QPushButton#nodeItem:checked {"
               " background:%5; border:1px solid %4; border-left:4px solid %4; }"
               "QPushButton#nodeItem:focus { border-color:%4; }")
        .arg(Theme::kControlBg, Theme::kBorder, Theme::kHoverBg,
             Theme::markColor(), Theme::kSelectionBg);
}

QString progressQss(bool warning)
{
    const QString chunk = warning ? QLatin1String(Theme::kStateWarning) : QLatin1String(Theme::kAccentFill);
    return QStringLiteral(
               "QProgressBar { background:%1; border:1px solid %2; border-radius:6px;"
               " text-align:center; color:%3; font-size:12px; font-weight:600; min-height:26px; max-height:26px; }"
               "QProgressBar::chunk { background:%4; border-radius:5px; }")
        .arg(Theme::kControlBg, Theme::kBorder, Theme::kTextPrimary, chunk);
}

QString labelQss(const char *color, int size, int weight)
{
    return QStringLiteral("color:%1;font-size:%2px;font-weight:%3;border:none;background:transparent;padding:0px;")
        .arg(QLatin1String(color))
        .arg(size)
        .arg(weight);
}

QLabel *titleLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("commTitle"));
    label->setStyleSheet(labelQss(Theme::kTextPrimary, 15, 600));
    return label;
}

QLabel *fieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("commField"));
    label->setStyleSheet(labelQss(Theme::kTextSecondary, 13, 400));
    return label;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("commHint"));
    label->setStyleSheet(labelQss(Theme::kTextMuted, 12, 400));
    label->setWordWrap(true);
    return label;
}

QLabel *makeStatusDot(const QString &text, const char *color, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("statusDot"));
    label->setStyleSheet(labelQss(color, 12, 600));
    return label;
}

QLabel *makeIconLabel(const QString &resource, QWidget *parent, int size = 18)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("commIcon"));
    label->setProperty("iconResource", resource);
    label->setProperty("iconSize", size);
    label->setFixedSize(size + 8, size + 8);
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(tintedIcon(resource, QColor(Theme::kTextSecondary), size,
                                label->devicePixelRatioF()));
    return label;
}

class ToggleSwitch : public QCheckBox
{
public:
    explicit ToggleSwitch(QWidget *parent = nullptr)
        : QCheckBox(parent)
    {
        setFixedSize(44, 26);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("点击切换启用状态"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF track(1.0, 2.0, width() - 2.0, height() - 4.0);
        QColor trackColor(isChecked() ? Theme::kAccentFill : Theme::kBorder);
        if (underMouse() && isEnabled()) {
            trackColor = QColor(isChecked() ? Theme::kAccent : Theme::kTextMuted);
        }
        if (!isEnabled()) {
            trackColor = QColor(Theme::kHoverBg);
        }
        if (hasFocus()) {
            painter.setPen(QPen(QColor(Theme::kFocusRing), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(track.adjusted(-1, -1, 1, 1), track.height() / 2.0,
                                    track.height() / 2.0);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(trackColor);
        painter.drawRoundedRect(track, track.height() / 2.0, track.height() / 2.0);
        const qreal diameter = track.height() - 6.0;
        const qreal x = isChecked() ? track.right() - diameter - 3.0 : track.left() + 3.0;
        painter.setBrush(isEnabled() ? QColor(Theme::kOnAccentFill) : QColor(Theme::kTextMuted));
        painter.drawEllipse(QRectF(x, track.top() + 3.0, diameter, diameter));
    }
};

QPushButton *makeSegmentButton(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("segmentBtn"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(36);
    button->setStyleSheet(segmentBtnQss());
    return button;
}

QString stepperBtnQss(int size = 36)
{
    return QStringLiteral(
               "QPushButton { background:%1; color:%2; border:1px solid %3;"
               " border-radius:6px; font-size:17px; font-weight:600; padding:0; min-width:%8px; max-width:%8px;"
               " min-height:%8px; max-height:%8px; }"
               "QPushButton:hover { background:%4; border-color:%5; }"
               "QPushButton:pressed { background:%6; }"
               "QPushButton:disabled { color:%7; }")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder,
             Theme::kHoverBg, Theme::markColor(), Theme::kPressedBg, Theme::kTextMuted)
        .arg(size);
}

QWidget *makeSpinStepper(QSpinBox *spin, QWidget *parent, const QString &suffix, int minWidth,
                         int controlHeight = 36, int horizontalPadding = 10,
                         Qt::Alignment alignment = Qt::AlignVCenter | Qt::AlignLeft)
{
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setFixedHeight(controlHeight);
    spin->setMinimumWidth(minWidth);
    spin->setProperty("controlHeight", controlHeight);
    spin->setProperty("horizontalPadding", horizontalPadding);
    spin->setStyleSheet(spinBoxQss(controlHeight, horizontalPadding));
    spin->setAlignment(alignment);
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
        button->setFixedSize(controlHeight, controlHeight);
        button->setProperty("stepperSize", controlHeight);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setObjectName(QStringLiteral("stepperBtn"));
        button->setStyleSheet(stepperBtnQss(controlHeight));
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

QString hikCardQss()
{
    return QStringLiteral(
        "QFrame#hikHeader, QFrame#hikSection, QFrame#hikFooter {"
        " background:%1; border:1px solid %2; border-radius:8px; }"
        "QFrame#hikSubPanel { background:%3; border:1px solid %2; border-radius:7px; }")
        .arg(Theme::kCardBg, Theme::kBorder, Theme::kControlBg);
}

QString hikBadgeQss(const QString &foreground, const QString &background)
{
    return QStringLiteral(
        "QLabel { color:%1; background:%2; border:1px solid %1; border-radius:10px;"
        " padding:3px 10px; font-size:11px; font-weight:600; }")
        .arg(foreground, background);
}

QString hikStatusBadgeQss(const QString &foreground, const QString &background)
{
    return QStringLiteral(
        "QLabel { color:%1; background:%2; border:1px solid %1; border-radius:13px;"
        " padding:0px 12px; font-size:12px; font-weight:600; }")
        .arg(foreground, background);
}

QWidget *makeChoiceGroup(QWidget *parent,
                         std::initializer_list<QPair<QString, int>> choices)
{
    auto *control = new QWidget(parent);
    control->setProperty("hikChoiceGroup", true);
    control->setFixedHeight(40);
    auto *layout = new QHBoxLayout(control);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(3);
    auto *group = new QButtonGroup(control);
    group->setExclusive(true);
    for (const auto &choice : choices) {
        auto *button = makeSegmentButton(choice.first, control);
        button->setFixedHeight(32);
        button->setMinimumWidth(0);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        group->addButton(button, choice.second);
        layout->addWidget(button, 1);
    }
    return control;
}

QButtonGroup *choiceButtonGroup(QWidget *control)
{
    return control ? control->findChild<QButtonGroup *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

int choiceValue(const QWidget *control)
{
    const QButtonGroup *group = choiceButtonGroup(const_cast<QWidget *>(control));
    return group ? group->checkedId() : -1;
}

bool setChoiceValue(QWidget *control, int value)
{
    QButtonGroup *group = choiceButtonGroup(control);
    if (!group) {
        return false;
    }
    if (QAbstractButton *button = group->button(value)) {
        button->setChecked(true);
        return true;
    }
    group->setExclusive(false);
    for (QAbstractButton *button : group->buttons()) {
        button->setChecked(false);
    }
    group->setExclusive(true);
    return false;
}

QString sliderQss(const QString &accent)
{
    return QStringLiteral(
        "QSlider::groove:horizontal { height:5px; background:%1; border-radius:2px; }"
        "QSlider::sub-page:horizontal { background:%2; border-radius:2px; }"
        "QSlider::handle:horizontal { width:16px; margin:-6px 0; border-radius:8px;"
        " background:%3; border:2px solid %2; }"
        "QSlider::handle:horizontal:disabled { background:%4; border-color:%1; }")
        .arg(Theme::kBorder, accent, Theme::kCardBg, Theme::kTextMuted);
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
        "QFrame#serviceBar { background:%1; border:1px solid %2; border-radius:8px; }")
        .arg(Theme::kCardBg, Theme::kBorder));
    serviceBar->setFixedHeight(72);
    auto *serviceLayout = new QHBoxLayout(serviceBar);
    serviceLayout->setContentsMargins(18, 10, 14, 10);
    serviceLayout->setSpacing(12);
    serviceLayout->addWidget(makeIconLabel(QStringLiteral(":/img/nav_settings.svg"), serviceBar, 22));
    auto *serviceText = new QVBoxLayout();
    serviceText->setContentsMargins(0, 0, 0, 0);
    serviceText->setSpacing(3);
    auto *serviceTitle = titleLabel(QStringLiteral("后台核心服务"), serviceBar);
    serviceText->addWidget(serviceTitle);
    m_serviceSummary = new QLabel(serviceBar);
    m_serviceSummary->setObjectName(QStringLiteral("serviceSummary"));
    m_serviceSummary->setStyleSheet(labelQss(Theme::kTextSecondary, 12, 400));
    m_serviceSummary->setText(QStringLiteral("正在检测相机、后端与算法服务…"));
    serviceText->addWidget(m_serviceSummary);
    serviceLayout->addLayout(serviceText, 1);
    m_startAllButton = new QPushButton(QStringLiteral("启动全部服务"), serviceBar);
    m_startAllButton->setObjectName(QStringLiteral("primaryBtn"));
    m_startAllButton->setCursor(Qt::PointingHandCursor);
    m_startAllButton->setStyleSheet(primaryBtnQss());
    serviceLayout->addWidget(m_startAllButton);
    m_redetectButton = new QPushButton(QStringLiteral("重新检测"), serviceBar);
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

    // ===== 2. 主工作区：四个设备节点对应四个独立页面 =====
    auto *workspace = new QHBoxLayout();
    workspace->setContentsMargins(0, 0, 0, 0);
    workspace->setSpacing(12);
    root->addLayout(workspace, 1);

    auto *leftCard = new QFrame(this);
    leftCard->setObjectName(QStringLiteral("nodeNavCard"));
    leftCard->setAttribute(Qt::WA_StyledBackground, true);
    leftCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    leftCard->setMinimumWidth(300);
    leftCard->setMaximumWidth(390);
    leftCard->setStyleSheet(navCardQss());
    auto *leftPanel = new QVBoxLayout(leftCard);
    leftPanel->setContentsMargins(14, 0, 14, 14);
    leftPanel->setSpacing(12);

    // 分组标题只使用底边框，不做成按钮或独立卡片。
    auto *leftHeading = new QFrame(leftCard);
    leftHeading->setObjectName(QStringLiteral("nodeGroupHeader"));
    leftHeading->setStyleSheet(QStringLiteral(
        "QFrame#nodeGroupHeader { background:transparent; border:none; border-bottom:1px solid %1; }")
        .arg(Theme::kBorder));
    auto *headingLayout = new QHBoxLayout(leftHeading);
    headingLayout->setContentsMargins(4, 10, 4, 10);
    headingLayout->setSpacing(8);
    headingLayout->addWidget(makeIconLabel(QStringLiteral(":/img/nav_detect.svg"), leftHeading, 18));
    auto *leftHeadingText = new QVBoxLayout();
    leftHeadingText->setSpacing(2);
    leftHeadingText->addWidget(titleLabel(QStringLiteral("设备节点"), leftHeading));
    leftHeadingText->addWidget(hintLabel(QStringLiteral("选择节点查看与配置"), leftHeading));
    headingLayout->addLayout(leftHeadingText, 1);
    leftPanel->addWidget(leftHeading);

    // 四个等高节点卡片均匀排列，底部留白由 stretch 吸收，不使用滚动容器。
    auto *nodeContainer = new QWidget(leftCard);
    auto *nodeLayout = new QVBoxLayout(nodeContainer);
    nodeLayout->setContentsMargins(0, 0, 0, 0);
    nodeLayout->setSpacing(10);
    auto *nodeGroup = new QButtonGroup(leftCard);
    nodeGroup->setExclusive(true);
    leftPanel->addWidget(nodeContainer, 1);
    workspace->addWidget(leftCard, 30);

    struct NodeDef {
        const char *icon;
        const char *name;
        const char *subtitle;
        QLabel **badge;
    };
    const NodeDef nodes[] = {
        {":/img/nav_detect.svg", "海康实时预览相机", "实时查看修磨过程", &m_cameraNodeBadge},
        {":/img/nav_settings.svg", "SICK 3D 相机节点", "线扫 3D 数据采集", &m_topicNodeBadge},
        {":/img/nav_process.svg", "PLC 控制系统", "西门子 S7 / Modbus", &m_plcBadge},
        {":/img/nav_report.svg", "数据与图像存储", "SQLite + 本地图像", &m_storageBadge},
    };
    for (int index = 0; index < 4; ++index) {
        const NodeDef &node = nodes[index];
        auto *button = new QPushButton(nodeContainer);
        button->setObjectName(QStringLiteral("nodeItem"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setFixedHeight(72);
        button->setStyleSheet(nodeButtonQss());
        nodeGroup->addButton(button, index);
        nodeLayout->addWidget(button);

        auto *row = new QVBoxLayout(button);
        row->setContentsMargins(14, 8, 14, 8);
        row->setSpacing(6);
        row->addStretch(1);
        auto *line1 = new QHBoxLayout();
        line1->setContentsMargins(0, 0, 0, 0);
        line1->setSpacing(8);
        auto *icon = makeIconLabel(QString::fromUtf8(node.icon), button, 20);
        auto *name = new QLabel(QString::fromUtf8(node.name), button);
        name->setObjectName(QStringLiteral("nodeName"));
        name->setStyleSheet(labelQss(Theme::kTextPrimary, 14, 600));
        *node.badge = makeStatusDot(QStringLiteral("● 未连接"), Theme::kStateError, button);
        line1->addWidget(icon);
        line1->addWidget(name, 1);
        line1->addWidget(*node.badge);
        row->addLayout(line1);
        auto *sub = new QLabel(QString::fromUtf8(node.subtitle), button);
        sub->setObjectName(QStringLiteral("nodeSub"));
        sub->setStyleSheet(labelQss(Theme::kTextMuted, 13, 400));
        sub->setContentsMargins(37, 0, 0, 0);
        row->addWidget(sub);
        row->addStretch(1);
    }
    nodeLayout->addStretch(1);
    nodeGroup->button(0)->setChecked(true);

    auto *rightPanel = new QVBoxLayout();
    rightPanel->setContentsMargins(0, 0, 0, 0);
    rightPanel->setSpacing(0);
    workspace->addLayout(rightPanel, 70);
    auto *configStack = new QStackedWidget(this);
    rightPanel->addWidget(configStack, 1);

    // ---- 相机配置卡 ----
    m_cameraCard = new QFrame(this);
    m_cameraCard->setObjectName(QStringLiteral("hikCameraWorkspace"));
    m_cameraCard->setStyleSheet(QStringLiteral("QFrame#hikCameraWorkspace { background:transparent; border:none; }"));
    auto *cameraLayout = new QVBoxLayout(m_cameraCard);
    cameraLayout->setContentsMargins(0, 0, 0, 0);
    cameraLayout->setSpacing(10);

    // 顶部状态头：设备身份、连接状态与主动回读。
    auto *header = new QFrame(m_cameraCard);
    header->setObjectName(QStringLiteral("hikHeader"));
    header->setFixedHeight(64);
    header->setStyleSheet(hikCardQss());
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 8, 14, 8);
    headerLayout->setSpacing(10);
    headerLayout->addWidget(makeIconLabel(QStringLiteral(":/img/nav_detect.svg"), header, 24));
    auto *headerText = new QVBoxLayout();
    headerText->setSpacing(2);
    headerText->addWidget(titleLabel(QStringLiteral("海康实时预览相机（HCNetSDK）"), header));
    headerLayout->addLayout(headerText, 1);
    m_hikHeaderStatus = new QLabel(QStringLiteral("● 待连接"), header);
    m_hikHeaderStatus->setFixedHeight(26);
    m_hikHeaderStatus->setStyleSheet(hikStatusBadgeQss(QLatin1String(Theme::kStateError), QLatin1String(Theme::kControlBg)));
    headerLayout->addWidget(m_hikHeaderStatus, 0, Qt::AlignVCenter);
    auto *protocolBadge = new QLabel(QStringLiteral("◆ HCNetSDK"), header);
    protocolBadge->setFixedHeight(26);
    protocolBadge->setStyleSheet(hikStatusBadgeQss(QLatin1String(Theme::kTextMuted), QLatin1String(Theme::kControlBg)));
    headerLayout->addWidget(protocolBadge, 0, Qt::AlignVCenter);
    m_hikRefreshButton = new QPushButton(QStringLiteral("读取相机当前参数"), header);
    m_hikRefreshButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_hikRefreshButton->setFixedHeight(32);
    m_hikRefreshButton->setStyleSheet(secondaryBtnQss()
        + QStringLiteral("QPushButton { min-height:32px; max-height:32px; }"));
    headerLayout->addWidget(m_hikRefreshButton, 0, Qt::AlignVCenter);
    cameraLayout->addWidget(header);

    const auto makeSectionTitle = [](const QString &text, QWidget *parent) {
        auto *row = new QHBoxLayout();
        row->setSpacing(8);
        auto *accent = new QFrame(parent);
        accent->setFixedSize(4, 17);
        accent->setStyleSheet(QStringLiteral("background:%1;border:none;border-radius:2px;").arg(Theme::kAccentFill));
        row->addWidget(accent);
        row->addWidget(titleLabel(text, parent));
        row->addStretch(1);
        return row;
    };

    // 第一舱：网络连接、超时与保活。
    auto *networkCard = new QFrame(m_cameraCard);
    networkCard->setObjectName(QStringLiteral("hikSection"));
    networkCard->setStyleSheet(hikCardQss());
    auto *networkLayout = new QVBoxLayout(networkCard);
    networkLayout->setContentsMargins(14, 10, 14, 9);
    networkLayout->setSpacing(8);
    networkLayout->addLayout(makeSectionTitle(QStringLiteral("① 网络连接与保活策略"), networkCard));
    auto *networkColumns = new QHBoxLayout();
    networkColumns->setSpacing(18);

    auto *ipField = new QVBoxLayout();
    ipField->setSpacing(5);
    ipField->addWidget(fieldLabel(QStringLiteral("设备 IPv4 地址"), networkCard));
    auto *ipRow = new QHBoxLayout();
    ipRow->setSpacing(6);
    auto *ipBadge = new QLabel(QStringLiteral("IPv4"), networkCard);
    ipBadge->setAlignment(Qt::AlignCenter);
    ipBadge->setFixedSize(42, 36);
    ipBadge->setStyleSheet(hikBadgeQss(QLatin1String(Theme::kAccent), QLatin1String(Theme::kSelectionBg))
        + QStringLiteral("QLabel { min-height:36px; max-height:36px; padding:0px; }"));
    ipRow->addWidget(ipBadge, 0, Qt::AlignVCenter);
    m_cameraIp = new QLineEdit(networkCard);
    m_cameraIp->setFixedSize(180, 36);
    m_cameraIp->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_cameraIp->setPlaceholderText(QStringLiteral("192.168.1.64"));
    m_cameraIp->setStyleSheet(inputQss() + QStringLiteral(
        "QLineEdit { min-height:36px; max-height:36px; padding:0px 14px; }"));
    ipRow->addWidget(m_cameraIp, 0, Qt::AlignVCenter);
    m_cameraTestButton = new QPushButton(QStringLiteral("▶ 测试连接"), networkCard);
    m_cameraTestButton->setObjectName(QStringLiteral("primaryBtn"));
    m_cameraTestButton->setFixedSize(96, 36);
    m_cameraTestButton->setStyleSheet(primaryBtnQss() + QStringLiteral(
        "QPushButton { min-height:36px; max-height:36px; padding:0px 14px; }"));
    ipRow->addWidget(m_cameraTestButton, 0, Qt::AlignVCenter);
    m_cameraPingButton = new QPushButton(QStringLiteral("● Ping"), networkCard);
    m_cameraPingButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_cameraPingButton->setFixedSize(76, 36);
    m_cameraPingButton->setStyleSheet(secondaryBtnQss() + QStringLiteral(
        "QPushButton { min-height:36px; max-height:36px; padding:0px 14px; }"));
    ipRow->addWidget(m_cameraPingButton, 0, Qt::AlignVCenter);
    ipField->addLayout(ipRow);
    networkColumns->addLayout(ipField, 5);

    auto *timeoutField = new QVBoxLayout();
    timeoutField->setSpacing(5);
    timeoutField->addWidget(fieldLabel(QStringLiteral("连接超时阈值"), networkCard));
    auto *timeoutRow = new QHBoxLayout();
    timeoutRow->setSpacing(4);
    m_timeoutSpin = new QSpinBox(networkCard);
    m_timeoutSpin->setRange(500, 10000);
    m_timeoutSpin->setSingleStep(100);
    timeoutRow->addWidget(makeSpinStepper(m_timeoutSpin, networkCard, QStringLiteral(" ms"), 110, 34));
    m_timeoutSpin->setFixedWidth(110);
    timeoutRow->addSpacing(4);
    auto *timeoutPresetGroup = new QButtonGroup(networkCard);
    timeoutPresetGroup->setExclusive(true);
    for (int seconds : {1, 3, 5}) {
        auto *preset = makeSegmentButton(QStringLiteral("%1s").arg(seconds), networkCard);
        preset->setProperty("timeoutPreset", true);
        preset->setFixedSize(42, 34);
        preset->setStyleSheet(segmentBtnQss() + QStringLiteral(
            "QPushButton { min-height:34px; max-height:34px; padding:0px; font-size:12px; }"));
        timeoutPresetGroup->addButton(preset, seconds * 1000);
        connect(preset, &QPushButton::clicked, m_timeoutSpin, [this, seconds]() {
            m_timeoutSpin->setValue(seconds * 1000);
        });
        timeoutRow->addWidget(preset);
    }
    timeoutPresetGroup->button(3000)->setChecked(true);
    connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            timeoutPresetGroup, [timeoutPresetGroup](int value) {
        if (QAbstractButton *preset = timeoutPresetGroup->button(value)) {
            preset->setChecked(true);
            return;
        }
        timeoutPresetGroup->setExclusive(false);
        for (QAbstractButton *preset : timeoutPresetGroup->buttons()) {
            preset->setChecked(false);
        }
        timeoutPresetGroup->setExclusive(true);
    });
    timeoutRow->addStretch(1);
    timeoutField->addLayout(timeoutRow);
    networkColumns->addLayout(timeoutField, 3);

    auto *reconnectPanel = new QFrame(networkCard);
    reconnectPanel->setObjectName(QStringLiteral("hikSubPanel"));
    reconnectPanel->setFixedHeight(58);
    reconnectPanel->setStyleSheet(hikCardQss());
    auto *reconnectRow = new QHBoxLayout(reconnectPanel);
    reconnectRow->setContentsMargins(12, 8, 12, 8);
    reconnectRow->setSpacing(8);
    auto *reconnectText = new QVBoxLayout();
    reconnectText->setSpacing(2);
    reconnectText->addWidget(fieldLabel(QStringLiteral("断线自动重连守护"), reconnectPanel));
    reconnectText->addWidget(hintLabel(QStringLiteral("网络闪断后按配置自动重连"), reconnectPanel));
    reconnectRow->addLayout(reconnectText, 1);
    m_autoReconnect = new ToggleSwitch(reconnectPanel);
    reconnectRow->addWidget(m_autoReconnect);
    networkColumns->addWidget(reconnectPanel, 3);
    networkLayout->addLayout(networkColumns);
    m_cameraStateHint = hintLabel(QStringLiteral("● 链路诊断就绪：修改 IP 后请先测试连接。"), networkCard);
    m_cameraStateHint->setWordWrap(false);
    networkLayout->addWidget(m_cameraStateHint);
    cameraLayout->addWidget(networkCard);

    // 第二、三舱：视频流/曝光与 ISP/HLC 双栏。
    auto *controlColumns = new QHBoxLayout();
    controlColumns->setSpacing(10);
    auto *streamCard = new QFrame(m_cameraCard);
    streamCard->setObjectName(QStringLiteral("hikSection"));
    streamCard->setStyleSheet(hikCardQss());
    auto *streamLayout = new QVBoxLayout(streamCard);
    streamLayout->setContentsMargins(14, 10, 14, 10);
    streamLayout->setSpacing(8);
    auto *streamTitle = makeSectionTitle(QStringLiteral("② 视频流规格与曝光控制"), streamCard);
    auto *liveBadge = new QLabel(QStringLiteral("★ 连接后实时生效"), streamCard);
    liveBadge->setStyleSheet(hikBadgeQss(QLatin1String(Theme::kAccent), QLatin1String(Theme::kSelectionBg)));
    streamTitle->addWidget(liveBadge);
    streamLayout->addLayout(streamTitle);
    streamLayout->addWidget(fieldLabel(QStringLiteral("预览码流通道"), streamCard));
    m_hikStreamType = makeChoiceGroup(streamCard, {
        {QStringLiteral("主码流（高清）"), 0}, {QStringLiteral("子码流（流畅）"), 1}});
    setChoiceValue(m_hikStreamType, 0);
    streamLayout->addWidget(m_hikStreamType);
    streamLayout->addWidget(fieldLabel(QStringLiteral("预览目标帧率"), streamCard));
    m_hikFrameRate = makeChoiceGroup(streamCard, {
        {QStringLiteral("全帧率"), 0}, {QStringLiteral("60 FPS"), 24},
        {QStringLiteral("30 FPS"), 18}, {QStringLiteral("25 FPS"), 17}, {QStringLiteral("15 FPS"), 14}});
    setChoiceValue(m_hikFrameRate, 18);
    streamLayout->addWidget(m_hikFrameRate);

    auto *exposurePanel = new QFrame(streamCard);
    exposurePanel->setObjectName(QStringLiteral("hikSubPanel"));
    exposurePanel->setStyleSheet(hikCardQss());
    auto *exposureLayout = new QVBoxLayout(exposurePanel);
    exposureLayout->setContentsMargins(10, 8, 10, 8);
    exposureLayout->setSpacing(7);
    exposureLayout->addWidget(fieldLabel(QStringLiteral("曝光控制模式与快门时间"), exposurePanel));
    m_hikExposureMode = makeChoiceGroup(exposurePanel, {
        {QStringLiteral("◎ 自动曝光（Auto AEC）"), 1}, {QStringLiteral("◆ 手动固定曝光"), 0}});
    setChoiceValue(m_hikExposureMode, 1);
    exposureLayout->addWidget(m_hikExposureMode);
    auto *exposureRow = new QHBoxLayout();
    exposureRow->setSpacing(8);
    auto *exposureText = new QVBoxLayout();
    exposureText->addWidget(fieldLabel(QStringLiteral("曝光时间上限"), exposurePanel));
    exposureText->addWidget(hintLabel(QStringLiteral("20 μs ~ 1 s"), exposurePanel));
    exposureRow->addLayout(exposureText);
    m_hikExposureSlider = new QSlider(Qt::Horizontal, exposurePanel);
    m_hikExposureSlider->setRange(20, 1000000);
    m_hikExposureSlider->setStyleSheet(sliderQss(QLatin1String(Theme::kAccentFill)));
    exposureRow->addWidget(m_hikExposureSlider, 1);
    m_hikExposureTime = new QSpinBox(exposurePanel);
    m_hikExposureTime->setRange(20, 1000000);
    m_hikExposureTime->setSingleStep(100);
    m_hikExposureTime->setKeyboardTracking(false);
    exposureRow->addWidget(makeSpinStepper(m_hikExposureTime, exposurePanel, QStringLiteral(" μs"), 82));
    exposureLayout->addLayout(exposureRow);
    streamLayout->addWidget(exposurePanel, 1);
    controlColumns->addWidget(streamCard, 48);

    auto *ispCard = new QFrame(m_cameraCard);
    ispCard->setObjectName(QStringLiteral("hikSection"));
    ispCard->setStyleSheet(hikCardQss());
    auto *ispLayout = new QVBoxLayout(ispCard);
    ispLayout->setContentsMargins(14, 10, 14, 10);
    ispLayout->setSpacing(14);
    auto *ispTitle = makeSectionTitle(QStringLiteral("③ ISP 图像画质与铜面强光抑制"), ispCard);
    auto *ispBadge = new QLabel(QStringLiteral("● 设备回读值"), ispCard);
    ispBadge->setStyleSheet(hikBadgeQss(QLatin1String(Theme::kTextMuted), QLatin1String(Theme::kControlBg)));
    ispTitle->addWidget(ispBadge);
    ispLayout->addLayout(ispTitle);

    const auto addSliderRow = [this, ispCard, ispLayout](const QString &title, const QString &subtitle,
                                                          QSlider *&slider, QSpinBox *&spin,
                                                          const QString &accent) {
        auto *row = new QHBoxLayout();
        row->setSpacing(9);
        auto *text = new QVBoxLayout();
        text->setSpacing(3);
        text->addWidget(fieldLabel(title, ispCard));
        text->addWidget(hintLabel(subtitle, ispCard));
        row->addLayout(text);
        slider = new QSlider(Qt::Horizontal, ispCard);
        slider->setRange(0, 100);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(50);
        slider->setStyleSheet(sliderQss(accent));
        row->addWidget(slider, 1);
        spin = new QSpinBox(ispCard);
        spin->setRange(0, 100);
        spin->setKeyboardTracking(false);
        row->addWidget(makeSpinStepper(spin, ispCard, QString(), 64, 34, 4, Qt::AlignCenter));
        ispLayout->addLayout(row);
    };
    addSliderRow(QStringLiteral("画面亮度"), QStringLiteral("Brightness · 0~100"),
                 m_hikBrightnessSlider, m_hikBrightness, QLatin1String(Theme::kAccentFill));
    addSliderRow(QStringLiteral("画面对比度"), QStringLiteral("Contrast · 0~100"),
                 m_hikContrastSlider, m_hikContrast, QLatin1String(Theme::kStateSuccess));

    auto *hlcPanel = new QFrame(ispCard);
    hlcPanel->setObjectName(QStringLiteral("hikSubPanel"));
    hlcPanel->setStyleSheet(hikCardQss());
    auto *hlcLayout = new QVBoxLayout(hlcPanel);
    hlcLayout->setContentsMargins(10, 8, 10, 8);
    hlcLayout->setSpacing(8);
    auto *hlcHeader = new QHBoxLayout();
    auto *hlcText = new QVBoxLayout();
    hlcText->setSpacing(2);
    hlcText->addWidget(fieldLabel(QStringLiteral("强光抑制（HLC）"), hlcPanel));
    hlcText->addWidget(hintLabel(QStringLiteral("抑制铣刀火花与铜面炫光"), hlcPanel));
    hlcHeader->addLayout(hlcText, 1);
    m_hikHighlightStateLabel = fieldLabel(QStringLiteral("未启用"), hlcPanel);
    hlcHeader->addWidget(m_hikHighlightStateLabel);
    m_hikHighlightEnabled = new ToggleSwitch(hlcPanel);
    hlcHeader->addWidget(m_hikHighlightEnabled);
    hlcLayout->addLayout(hlcHeader);
    m_hikHighlightLevel = makeChoiceGroup(hlcPanel, {
        {QStringLiteral("1 级（轻度）"), 1}, {QStringLiteral("2 级（标准抗反光）"), 2},
        {QStringLiteral("3 级（强力压暗）"), 3}});
    setChoiceValue(m_hikHighlightLevel, 2);
    hlcLayout->addWidget(m_hikHighlightLevel);
    ispLayout->addWidget(hlcPanel);

    ispLayout->addWidget(fieldLabel(QStringLiteral("光学场景快捷预设"), ispCard));
    auto *scenePresets = new QHBoxLayout();
    scenePresets->setSpacing(6);
    auto *scenePresetGroup = new QButtonGroup(ispCard);
    scenePresetGroup->setExclusive(true);
    const auto sceneDefinitions = {
        QPair<QString, QString>{QStringLiteral("常规车间照明"), QStringLiteral("lighting")},
        QPair<QString, QString>{QStringLiteral("强反光新铜面"), QStringLiteral("reflection")},
        QPair<QString, QString>{QStringLiteral("暗光高增益"), QStringLiteral("gain")},
    };
    for (const auto &scene : sceneDefinitions) {
        auto *preset = makeSegmentButton(scene.first, ispCard);
        preset->setFixedHeight(34);
        preset->setProperty("hikScenePreset", scene.second);
        preset->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        scenePresetGroup->addButton(preset, scenePresets->count());
        scenePresets->addWidget(preset, 1);
    }
    scenePresetGroup->button(0)->setChecked(true);
    ispLayout->addLayout(scenePresets);
    ispLayout->addStretch(1);
    controlColumns->addWidget(ispCard, 52);
    cameraLayout->addLayout(controlColumns, 1);

    auto *footer = new QFrame(m_cameraCard);
    footer->setObjectName(QStringLiteral("hikFooter"));
    footer->setFixedHeight(48);
    footer->setStyleSheet(hikCardQss());
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 5, 10, 5);
    footerLayout->setSpacing(10);
    m_hikParameterHint = hintLabel(QStringLiteral("● 相机未连接 · 请先测试连接，再同步画质参数。"), footer);
    m_hikParameterHint->setWordWrap(false);
    footerLayout->addWidget(m_hikParameterHint, 1);
    auto *cameraDefaultsButton = new QPushButton(QStringLiteral("↺ 恢复默认"), footer);
    cameraDefaultsButton->setObjectName(QStringLiteral("secondaryBtn"));
    cameraDefaultsButton->setStyleSheet(secondaryBtnQss());
    connect(cameraDefaultsButton, &QPushButton::clicked, this, &NodeCommSettingsPage::restoreDefaults);
    footerLayout->addWidget(cameraDefaultsButton);
    m_cameraApplyButton = new QPushButton(QStringLiteral("✓ 应用相机配置"), footer);
    m_cameraApplyButton->setObjectName(QStringLiteral("primaryBtn"));
    m_cameraApplyButton->setStyleSheet(primaryBtnQss());
    footerLayout->addWidget(m_cameraApplyButton);
    cameraLayout->addWidget(footer);
    m_cameraCard->setMinimumHeight(0);
    m_cameraCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    configStack->addWidget(m_cameraCard);
    updateHikParameterStates();

    // ---- SICK 3D 相机：真实节点状态与采集参数 ----
    m_sickCard = new QFrame(configStack);
    m_sickCard->setObjectName(QStringLiteral("sickWorkspace"));
    m_sickCard->setStyleSheet(QStringLiteral("QFrame#sickWorkspace { background:transparent; border:none; }"));
    auto *sickLayout = new QVBoxLayout(m_sickCard);
    sickLayout->setContentsMargins(0, 0, 0, 0);
    sickLayout->setSpacing(12);

    auto *sickHeader = new QFrame(m_sickCard);
    sickHeader->setObjectName(QStringLiteral("hikHeader"));
    sickHeader->setFixedHeight(64);
    sickHeader->setStyleSheet(hikCardQss());
    auto *sickHeaderLayout = new QHBoxLayout(sickHeader);
    sickHeaderLayout->setContentsMargins(16, 8, 16, 8);
    sickHeaderLayout->setSpacing(10);
    sickHeaderLayout->addWidget(makeIconLabel(QStringLiteral(":/img/nav_settings.svg"), sickHeader, 24));
    auto *sickHeaderText = new QVBoxLayout();
    sickHeaderText->setSpacing(2);
    sickHeaderText->addWidget(titleLabel(QStringLiteral("SICK 3D 线激光相机"), sickHeader));
    sickHeaderText->addWidget(hintLabel(QStringLiteral("Ranger3 · 编码器触发线扫采集"), sickHeader));
    sickHeaderLayout->addLayout(sickHeaderText, 1);
    m_sickHeaderStatus = new QLabel(QStringLiteral("● 等待状态"), sickHeader);
    m_sickHeaderStatus->setFixedHeight(26);
    sickHeaderLayout->addWidget(m_sickHeaderStatus, 0, Qt::AlignVCenter);
    auto *sickProtocol = new QLabel(QStringLiteral("◆ ROS 2"), sickHeader);
    sickProtocol->setFixedHeight(26);
    sickProtocol->setStyleSheet(hikStatusBadgeQss(QLatin1String(Theme::kTextMuted), QLatin1String(Theme::kControlBg)));
    sickHeaderLayout->addWidget(sickProtocol, 0, Qt::AlignVCenter);
    sickLayout->addWidget(sickHeader);

    auto *statusCard = new QFrame(m_sickCard);
    statusCard->setObjectName(QStringLiteral("hikSection"));
    statusCard->setStyleSheet(hikCardQss());
    auto *statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(16, 12, 16, 12);
    statusLayout->setSpacing(10);
    statusLayout->addLayout(makeSectionTitle(QStringLiteral("① 节点与相机状态监控"), statusCard));
    auto *statusColumns = new QHBoxLayout();
    statusColumns->setSpacing(12);
    const auto addStatusCell = [statusCard, statusColumns](const QString &title, QLabel **value) {
        auto *panel = new QFrame(statusCard);
        panel->setObjectName(QStringLiteral("hikSubPanel"));
        panel->setStyleSheet(hikCardQss());
        panel->setMinimumHeight(64);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(4);
        layout->addWidget(fieldLabel(title, panel));
        *value = titleLabel(QStringLiteral("等待反馈"), panel);
        layout->addWidget(*value);
        statusColumns->addWidget(panel, 1);
    };
    addStatusCell(QStringLiteral("状态发布者"), &m_sickPublisherValue);
    addStatusCell(QStringLiteral("最近状态接收"), &m_sickLastMessageValue);
    statusLayout->addLayout(statusColumns);
    m_sickNodeDetail = hintLabel(QStringLiteral("尚未收到 /camera/status，无法确认相机连接状态。"), statusCard);
    statusLayout->addWidget(m_sickNodeDetail);
    sickLayout->addWidget(statusCard);

    auto *parameterCard = new QFrame(m_sickCard);
    parameterCard->setObjectName(QStringLiteral("hikSection"));
    parameterCard->setStyleSheet(hikCardQss());
    auto *parameterLayout = new QVBoxLayout(parameterCard);
    parameterLayout->setContentsMargins(16, 12, 16, 14);
    parameterLayout->setSpacing(12);
    auto *parameterTitle = makeSectionTitle(QStringLiteral("② 线激光采集参数"), parameterCard);
    auto *readbackBadge = new QLabel(QStringLiteral("● 设备实际值"), parameterCard);
    readbackBadge->setStyleSheet(hikBadgeQss(QLatin1String(Theme::kAccent), QLatin1String(Theme::kSelectionBg)));
    parameterTitle->addWidget(readbackBadge);
    parameterLayout->addLayout(parameterTitle);

    auto *parameterGrid = new QGridLayout();
    parameterGrid->setContentsMargins(0, 0, 0, 0);
    parameterGrid->setHorizontalSpacing(18);
    parameterGrid->setVerticalSpacing(12);
    parameterGrid->setColumnStretch(0, 1);
    parameterGrid->setColumnStretch(1, 1);
    const auto addParameterCell = [parameterCard, parameterGrid](int row, int column,
                                                                  const QString &title,
                                                                  const QString &hint,
                                                                  QWidget *control) {
        auto *panel = new QFrame(parameterCard);
        panel->setObjectName(QStringLiteral("hikSubPanel"));
        panel->setStyleSheet(hikCardQss());
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(7);
        layout->addWidget(fieldLabel(title, panel));
        layout->addWidget(control);
        layout->addWidget(hintLabel(hint, panel));
        parameterGrid->addWidget(panel, row, column);
    };

    m_sickScanLines = new QSpinBox(parameterCard);
    m_sickScanLines->setRange(0, 3800);
    m_sickScanLines->setSingleStep(10);
    m_sickScanLines->setSpecialValueText(QStringLiteral("等待设备回读"));
    m_sickScanLines->setValue(0);
    m_sickScanLines->setKeyboardTracking(false);
    addParameterCell(0, 0, QStringLiteral("单帧采集行数"), QStringLiteral("设备允许范围：1～3800 行"),
                     makeSpinStepper(m_sickScanLines, parameterCard, QStringLiteral(" 行"), 120));

    m_sickExposureTime = new QDoubleSpinBox(parameterCard);
    m_sickExposureTime->setRange(0.0, 1000000000.0);
    m_sickExposureTime->setDecimals(3);
    m_sickExposureTime->setSingleStep(10.0);
    m_sickExposureTime->setSpecialValueText(QStringLiteral("等待设备回读"));
    m_sickExposureTime->setValue(0.0);
    m_sickExposureTime->setSuffix(QStringLiteral(" μs"));
    m_sickExposureTime->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_sickExposureTime->setFixedHeight(36);
    m_sickExposureTime->setKeyboardTracking(false);
    addParameterCell(0, 1, QStringLiteral("曝光时间"), QStringLiteral("同时应用于 Region0 与 Region1"), m_sickExposureTime);

    m_sickYResolution = new QDoubleSpinBox(parameterCard);
    m_sickYResolution->setRange(0.0, 1000000000.0);
    m_sickYResolution->setDecimals(5);
    m_sickYResolution->setSingleStep(0.00001);
    m_sickYResolution->setSpecialValueText(QStringLiteral("等待设备回读"));
    m_sickYResolution->setValue(0.0);
    m_sickYResolution->setSuffix(QStringLiteral(" mm/行"));
    m_sickYResolution->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_sickYResolution->setFixedHeight(36);
    m_sickYResolution->setKeyboardTracking(false);
    addParameterCell(1, 0, QStringLiteral("Y 轴分辨率"), QStringLiteral("编码器分辨率 × 编码器分频"), m_sickYResolution);

    m_sickHdrMode = makeChoiceGroup(parameterCard, {
        {QStringLiteral("关闭 HDR（Off）"), 0},
        {QStringLiteral("开启 HDR（PresetMedium）"), 1}});
    setChoiceValue(m_sickHdrMode, -1);
    addParameterCell(1, 1, QStringLiteral("HDR 反光抑制"), QStringLiteral("仅显示设备支持的两种 MultiSlope 模式"), m_sickHdrMode);
    parameterLayout->addLayout(parameterGrid);
    m_sickScanLength = hintLabel(QStringLiteral("扫描物理长度　等待采集行数与 Y 分辨率回读"), parameterCard);
    m_sickScanLength->setObjectName(QStringLiteral("commHint"));
    parameterLayout->addWidget(m_sickScanLength);
    parameterLayout->addStretch(1);
    sickLayout->addWidget(parameterCard, 1);

    auto *sickFooter = new QFrame(m_sickCard);
    sickFooter->setObjectName(QStringLiteral("hikFooter"));
    sickFooter->setFixedHeight(50);
    sickFooter->setStyleSheet(hikCardQss());
    auto *sickFooterLayout = new QHBoxLayout(sickFooter);
    sickFooterLayout->setContentsMargins(14, 6, 12, 6);
    sickFooterLayout->setSpacing(12);
    m_sickParameterHint = hintLabel(QStringLiteral("● 等待设备回读 · 当前不会发送任何采集参数。"), sickFooter);
    sickFooterLayout->addWidget(m_sickParameterHint, 1);
    m_sickApplyButton = new QPushButton(QStringLiteral("✓ 应用采集参数"), sickFooter);
    m_sickApplyButton->setObjectName(QStringLiteral("primaryBtn"));
    m_sickApplyButton->setStyleSheet(primaryBtnQss());
    sickFooterLayout->addWidget(m_sickApplyButton);
    sickLayout->addWidget(sickFooter);
    configStack->addWidget(m_sickCard);

    // PLC 页面暂时没有已确认的配置内容，保留空白工作区。
    auto *emptyPage = new QFrame(configStack);
    emptyPage->setObjectName(QStringLiteral("commCard"));
    emptyPage->setAttribute(Qt::WA_StyledBackground, true);
    emptyPage->setStyleSheet(contentCardQss());
    configStack->addWidget(emptyPage);
    updateSickParameterStates();

    // ---- 数据存储卡 ----
    m_storageCard = new QFrame(this);
    m_storageCard->setObjectName(QStringLiteral("commCard"));
    m_storageCard->setStyleSheet(contentCardQss());
    auto *storageLayout = new QVBoxLayout(m_storageCard);
    storageLayout->setContentsMargins(18, 16, 18, 16);
    storageLayout->setSpacing(13);
    auto *storageHeading = new QHBoxLayout();
    storageHeading->setSpacing(8);
    storageHeading->addWidget(makeIconLabel(QStringLiteral(":/img/nav_report.svg"), m_storageCard, 20));
    auto *storageHeadingText = new QVBoxLayout();
    storageHeadingText->setSpacing(2);
    storageHeadingText->addWidget(titleLabel(QStringLiteral("数据与图像存储"), m_storageCard));
    storageHeadingText->addWidget(hintLabel(QStringLiteral("SQLite、存图策略与磁盘保护"), m_storageCard));
    storageHeading->addLayout(storageHeadingText, 1);
    storageLayout->addLayout(storageHeading);

    auto *dbRow = new QHBoxLayout();
    dbRow->setContentsMargins(0, 0, 0, 0);
    dbRow->addWidget(fieldLabel(QStringLiteral("本地数据库 SQLite"), m_storageCard));
    m_dbStatusValue = makeStatusDot(QStringLiteral("● 已连接"), Theme::kStateSuccess, m_storageCard);
    dbRow->addWidget(m_dbStatusValue);
    dbRow->addStretch(1);
    m_dbViewButton = new QPushButton(QStringLiteral("查看数据表（0）"), m_storageCard);
    m_dbViewButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_dbViewButton->setCursor(Qt::PointingHandCursor);
    m_dbViewButton->setStyleSheet(secondaryBtnQss());
    dbRow->addWidget(m_dbViewButton);
    storageLayout->addLayout(dbRow);

    auto *rootRow = new QHBoxLayout();
    rootRow->setContentsMargins(0, 0, 0, 0);
    rootRow->setSpacing(10);
    auto *rootLabel = fieldLabel(QStringLiteral("图像保存目录"), m_storageCard);
    rootLabel->setFixedWidth(96);
    rootRow->addWidget(rootLabel);
    m_imageRoot = new QLineEdit(m_storageCard);
    m_imageRoot->setStyleSheet(inputQss());
    m_imageRoot->setMinimumHeight(36);
    m_browseButton = new QPushButton(QStringLiteral("选择目录"), m_storageCard);
    m_browseButton->setObjectName(QStringLiteral("secondaryBtn"));
    m_browseButton->setCursor(Qt::PointingHandCursor);
    m_browseButton->setStyleSheet(secondaryBtnQss());
    rootRow->addWidget(m_imageRoot, 1);
    rootRow->addWidget(m_browseButton);
    storageLayout->addLayout(rootRow);

    auto *saveModeRow = new QHBoxLayout();
    saveModeRow->setContentsMargins(0, 0, 0, 0);
    saveModeRow->setSpacing(8);
    auto *saveModeLabel = fieldLabel(QStringLiteral("存图模式"), m_storageCard);
    saveModeLabel->setFixedWidth(96);
    saveModeRow->addWidget(saveModeLabel);
    m_saveNgOnlyButton = makeSegmentButton(QStringLiteral("仅 NG / 粒子图"), m_storageCard);
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

    storageLayout->addWidget(fieldLabel(QStringLiteral("图像保存盘使用情况"), m_storageCard));
    m_diskBar = new QProgressBar(m_storageCard);
    m_diskBar->setRange(0, 100);
    m_diskBar->setValue(0);
    m_diskBar->setTextVisible(true);
    m_diskBar->setFixedHeight(26);
    m_diskBar->setStyleSheet(progressQss(false));
    storageLayout->addWidget(m_diskBar);

    auto *cleanSwitchRow = new QHBoxLayout();
    cleanSwitchRow->setContentsMargins(0, 0, 0, 0);
    cleanSwitchRow->setSpacing(10);
    m_autoClean = new ToggleSwitch(m_storageCard);
    cleanSwitchRow->addWidget(m_autoClean);
    cleanSwitchRow->addWidget(fieldLabel(QStringLiteral("启用磁盘自动循环清理"), m_storageCard));
    cleanSwitchRow->addWidget(hintLabel(QStringLiteral("空间不足或数据超期时覆盖最早记录"), m_storageCard), 1);
    storageLayout->addLayout(cleanSwitchRow);

    m_cleanMinGbSpin = new QSpinBox(m_storageCard);
    m_cleanMinGbSpin->setRange(1, 999);
    m_cleanMinGbSpin->setValue(50);
    m_cleanMaxDaysSpin = new QSpinBox(m_storageCard);
    m_cleanMaxDaysSpin->setRange(1, 365);
    m_cleanMaxDaysSpin->setValue(30);

    auto *cleanRow = new QHBoxLayout();
    cleanRow->setContentsMargins(0, 0, 0, 0);
    cleanRow->setSpacing(6);
    auto *cleanPrefix = hintLabel(QStringLiteral("剩余空间低于"), m_storageCard);
    cleanPrefix->setWordWrap(false);
    cleanRow->addWidget(cleanPrefix);
    cleanRow->addWidget(makeSpinStepper(m_cleanMinGbSpin, m_storageCard, QStringLiteral(" GB"), 95));
    auto *cleanOr = hintLabel(QStringLiteral("或保存超过"), m_storageCard);
    cleanOr->setWordWrap(false);
    cleanRow->addWidget(cleanOr);
    cleanRow->addWidget(makeSpinStepper(m_cleanMaxDaysSpin, m_storageCard, QStringLiteral(" 天"), 95));
    auto *cleanSuffix = hintLabel(QStringLiteral("时触发"), m_storageCard);
    cleanSuffix->setWordWrap(false);
    cleanRow->addWidget(cleanSuffix);
    cleanRow->addStretch(1);
    storageLayout->addLayout(cleanRow);
    storageLayout->addStretch(1);
    m_storageCard->setMinimumHeight(0);
    m_storageCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    configStack->addWidget(m_storageCard);

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
        const QString current = m_imageRoot->text().trimmed();
        const QString directory = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择图像保存目录"), current, QFileDialog::ShowDirsOnly);
        if (!directory.isEmpty()) {
            m_imageRoot->setText(QDir::toNativeSeparators(directory));
        }
    });
    connect(m_dbViewButton, &QPushButton::clicked, this, [this]() {
        reloadFromSource();
        m_databaseSchemaDialog->exec();
    });
    connect(nodeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this,
            [configStack](int index) {
        if (index >= 0 && index < configStack->count()) {
            configStack->setCurrentIndex(index);
        }
    });

    connect(m_hikRefreshButton, &QPushButton::clicked, this, [this]() {
        m_hikParameterHint->setText(QStringLiteral("● 正在重新读取相机当前参数…"));
        emit hikCameraRefreshParametersRequested();
    });

    connect(m_sickScanLines, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NodeCommSettingsPage::updateSickScanLength);
    connect(m_sickYResolution, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &NodeCommSettingsPage::updateSickScanLength);
    connect(m_sickApplyButton, &QPushButton::clicked, this, [this]() {
        if (!m_cameraNodeConnected || !m_rosBridgeRunning || !m_sickParametersReceived || m_sickWritePending) {
            return;
        }
        const int hdr = choiceValue(m_sickHdrMode);
        if (m_sickScanLines->value() < 1 || m_sickExposureTime->value() <= 0.0
            || m_sickYResolution->value() <= 0.0 || (hdr != 0 && hdr != 1)) {
            m_sickParameterHint->setText(QStringLiteral("● 参数不完整或超出允许范围，未发送。"));
            return;
        }
        SickCameraParametersVM next;
        next.scanLines = static_cast<quint32>(m_sickScanLines->value());
        next.exposureTimeUs = m_sickExposureTime->value();
        next.yResolutionMm = m_sickYResolution->value();
        next.hdrMode = hdr == 1 ? QStringLiteral("PresetMedium") : QStringLiteral("Off");
        m_sickWritePending = true;
        m_sickParameterHint->setText(QStringLiteral("● 正在写入设备，等待 /sick_camera/parameter_result 返回…"));
        updateSickParameterStates();
        emit sickCameraParametersUpdateRequested(next);
        QTimer::singleShot(6000, this, [this]() {
            if (!m_sickWritePending) {
                return;
            }
            m_sickWritePending = false;
            m_sickParameterHint->setText(QStringLiteral("● 写入结果超时，已恢复编辑；请检查相机节点通讯。"));
            updateSickParameterStates();
        });
    });

    // 实时预览参数不进入普通表单的未保存状态；每次修改直接写入设备并等待回读。
    const auto requestHikUpdate = [this]() {
        if (m_updatingHikParameters || !m_hikConnected || !m_hikParametersReceived) {
            return;
        }
        updateHikParameterStates();
        HikCameraParameters next = m_hikParameters;
        next.streamType = choiceValue(m_hikStreamType);
        next.frameRateCode = choiceValue(m_hikFrameRate);
        next.brightness = m_hikBrightness->value();
        next.contrast = m_hikContrast->value();
        next.exposureMode = choiceValue(m_hikExposureMode);
        next.exposureTimeUs = m_hikExposureTime->value();
        next.highlightEnabled = m_hikHighlightEnabled->isChecked();
        next.highlightLevel = choiceValue(m_hikHighlightLevel);
        emit hikCameraParametersUpdateRequested(next);
    };
    for (QWidget *choice : {m_hikStreamType, m_hikFrameRate, m_hikExposureMode, m_hikHighlightLevel}) {
        connect(choiceButtonGroup(choice), QOverload<int>::of(&QButtonGroup::idClicked),
                this, [requestHikUpdate](int) { requestHikUpdate(); });
    }
    connect(m_hikBrightnessSlider, &QSlider::valueChanged, m_hikBrightness, &QSpinBox::setValue);
    connect(m_hikBrightness, QOverload<int>::of(&QSpinBox::valueChanged),
            m_hikBrightnessSlider, &QSlider::setValue);
    connect(m_hikContrastSlider, &QSlider::valueChanged, m_hikContrast, &QSpinBox::setValue);
    connect(m_hikContrast, QOverload<int>::of(&QSpinBox::valueChanged),
            m_hikContrastSlider, &QSlider::setValue);
    connect(m_hikExposureSlider, &QSlider::valueChanged, m_hikExposureTime, &QSpinBox::setValue);
    connect(m_hikExposureTime, QOverload<int>::of(&QSpinBox::valueChanged),
            m_hikExposureSlider, &QSlider::setValue);
    for (QSpinBox *spin : {m_hikBrightness, m_hikContrast, m_hikExposureTime}) {
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [requestHikUpdate](int) { requestHikUpdate(); });
    }
    connect(m_hikHighlightEnabled, &QCheckBox::toggled,
            this, [requestHikUpdate](bool) { requestHikUpdate(); });

    // 场景预设复用真实参数写入链路；一次预设只触发一次设备更新。
    for (QPushButton *preset : ispCard->findChildren<QPushButton *>()) {
        const QString scene = preset->property("hikScenePreset").toString();
        if (scene.isEmpty()) {
            continue;
        }
        connect(preset, &QPushButton::clicked, this, [this, scene, requestHikUpdate]() {
            if (scene == QStringLiteral("gain")) {
                return;
            }
            m_updatingHikParameters = true;
            if (scene == QStringLiteral("lighting")) {
                m_hikBrightness->setValue(50);
                m_hikContrast->setValue(55);
            } else if (scene == QStringLiteral("reflection")) {
                m_hikHighlightEnabled->setChecked(true);
                setChoiceValue(m_hikHighlightLevel, 2);
            }
            m_updatingHikParameters = false;
            requestHikUpdate();
        });
    }

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
    connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(m_autoReconnect, &QCheckBox::toggled, this, markDirty);
    connect(m_imageRoot, &QLineEdit::textChanged, this, markDirty);
    connect(m_cleanMinGbSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(m_cleanMaxDaysSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, markDirty);
    connect(m_autoClean, &QCheckBox::toggled, this, markDirty);
    connect(m_saveModeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, markDirty);

    // 初始加载
    m_loading = true;
    const DeviceCommConfig comm = loadDeviceCommConfig();
    m_cameraIp->setText(m_config.address.isEmpty() ? QStringLiteral("192.168.1.64") : m_config.address);
    m_timeoutSpin->setValue(m_config.loginTimeoutMs > 0 ? m_config.loginTimeoutMs : 3000);
    m_autoReconnect->setChecked(comm.autoReconnect);
    m_imageRoot->setText(comm.imageRoot);
    m_cleanMinGbSpin->setValue(comm.cleanMinGb);
    m_cleanMaxDaysSpin->setValue(comm.cleanMaxDays);
    m_autoClean->setChecked(comm.autoClean);
    if (comm.saveMode == QStringLiteral("all")) {
        m_saveAllButton->setChecked(true);
    } else if (comm.saveMode == QStringLiteral("none")) {
        m_saveNoneButton->setChecked(true);
    } else {
        m_saveNgOnlyButton->setChecked(true);
    }
    m_loading = false;

    updateServiceStatusBar();
    applyTheme();
    Theme::watch(this, [this]() { applyTheme(); });
}

void NodeCommSettingsPage::applyTheme()
{
    setStyleSheet(QStringLiteral("QWidget#nodeCommPage { background:%1; }").arg(Theme::kWindowBg));
    if (QFrame *bar = findChild<QFrame *>(QStringLiteral("serviceBar"))) {
        bar->setStyleSheet(QStringLiteral(
                               "QFrame#serviceBar { background:%1; border:1px solid %2; border-radius:8px; }")
                               .arg(Theme::kCardBg, Theme::kBorder));
    }
    const auto restyle = [](QWidget *widget, const QString &sheet) {
        if (widget) {
            widget->setStyleSheet(sheet);
        }
    };
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("commTitle"))) {
        restyle(label, labelQss(Theme::kTextPrimary, 15, 600));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("commField"))) {
        restyle(label, labelQss(Theme::kTextSecondary, 13, 400));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("commHint"))) {
        restyle(label, labelQss(Theme::kTextMuted, 12, 400));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("nodeName"))) {
        restyle(label, labelQss(Theme::kTextPrimary, 14, 600));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("nodeSub"))) {
        restyle(label, labelQss(Theme::kTextMuted, 13, 400));
    }
    for (QLabel *label : findChildren<QLabel *>(QStringLiteral("commIcon"))) {
        const int size = label->property("iconSize").toInt();
        label->setPixmap(tintedIcon(label->property("iconResource").toString(),
                                    QColor(Theme::kTextSecondary), size,
                                    label->devicePixelRatioF()));
    }
    restyle(m_serviceSummary, labelQss(Theme::kTextSecondary, 12, 400));
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("primaryBtn"))) {
        QString sheet = primaryBtnQss();
        if (button == m_cameraTestButton) {
            sheet += QStringLiteral(
                "QPushButton { min-height:36px; max-height:36px; padding:0px 14px; }");
        }
        restyle(button, sheet);
    }
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("secondaryBtn"))) {
        QString sheet = secondaryBtnQss();
        if (button == m_hikRefreshButton) {
            sheet += QStringLiteral("QPushButton { min-height:32px; max-height:32px; }");
        } else if (button == m_cameraPingButton) {
            sheet += QStringLiteral(
                "QPushButton { min-height:36px; max-height:36px; padding:0px 14px; }");
        }
        restyle(button, sheet);
    }
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("segmentBtn"))) {
        QString sheet = segmentBtnQss();
        if (button->property("timeoutPreset").toBool()) {
            sheet += QStringLiteral(
                "QPushButton { min-height:34px; max-height:34px; padding:0px; font-size:12px; }");
        }
        restyle(button, sheet);
    }
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("stepperBtn"))) {
        restyle(button, stepperBtnQss(button->property("stepperSize").toInt()));
    }
    for (QFrame *card : findChildren<QFrame *>(QStringLiteral("commCard"))) {
        restyle(card, contentCardQss());
    }
    for (const QString &name : {QStringLiteral("hikHeader"), QStringLiteral("hikSection"),
                                QStringLiteral("hikFooter"), QStringLiteral("hikSubPanel")}) {
        for (QFrame *card : findChildren<QFrame *>(name)) {
            restyle(card, hikCardQss());
        }
    }
    for (QWidget *choice : findChildren<QWidget *>()) {
        if (choice->property("hikChoiceGroup").toBool()) {
            choice->setStyleSheet(QStringLiteral(
                "QWidget { background:%1; border:1px solid %2; border-radius:6px; }")
                .arg(Theme::kControlBg, Theme::kBorder));
        }
    }
    restyle(m_hikBrightnessSlider, sliderQss(QLatin1String(Theme::kAccentFill)));
    restyle(m_hikContrastSlider, sliderQss(QLatin1String(Theme::kStateSuccess)));
    restyle(m_hikExposureSlider, sliderQss(QLatin1String(Theme::kAccentFill)));
    if (m_cameraCard) {
        for (QLabel *badge : m_cameraCard->findChildren<QLabel *>()) {
            if (badge == m_hikHeaderStatus) {
                continue;
            }
            if (badge->text().contains(QStringLiteral("★")) || badge->text() == QStringLiteral("IPv4")) {
                QString sheet = hikBadgeQss(QLatin1String(Theme::kAccent), QLatin1String(Theme::kSelectionBg));
                if (badge->text() == QStringLiteral("IPv4")) {
                    sheet += QStringLiteral("QLabel { min-height:36px; max-height:36px; padding:0px; }");
                }
                badge->setStyleSheet(sheet);
            } else if (badge->text().contains(QStringLiteral("HCNetSDK"))) {
                badge->setStyleSheet(hikStatusBadgeQss(QLatin1String(Theme::kTextMuted), QLatin1String(Theme::kControlBg)));
            } else if (badge->text().contains(QStringLiteral("设备回读值"))) {
                badge->setStyleSheet(hikBadgeQss(QLatin1String(Theme::kTextMuted), QLatin1String(Theme::kControlBg)));
            }
        }
    }
    if (QFrame *nav = findChild<QFrame *>(QStringLiteral("nodeNavCard"))) {
        restyle(nav, navCardQss());
    }
    if (QFrame *header = findChild<QFrame *>(QStringLiteral("nodeGroupHeader"))) {
        restyle(header, QStringLiteral(
            "QFrame#nodeGroupHeader { background:transparent; border:none; border-bottom:1px solid %1; }")
            .arg(Theme::kBorder));
    }
    for (QLineEdit *edit : findChildren<QLineEdit *>()) {
        QString sheet = inputQss();
        if (edit == m_cameraIp) {
            sheet += QStringLiteral(
                "QLineEdit { min-height:36px; max-height:36px; padding:0px 14px; }");
        }
        restyle(edit, sheet);
    }
    for (QSpinBox *spin : findChildren<QSpinBox *>()) {
        const int height = spin->property("controlHeight").isValid()
            ? spin->property("controlHeight").toInt() : 36;
        const int padding = spin->property("horizontalPadding").isValid()
            ? spin->property("horizontalPadding").toInt() : 10;
        restyle(spin, spinBoxQss(height, padding));
    }
    for (QDoubleSpinBox *spin : findChildren<QDoubleSpinBox *>()) {
        restyle(spin, spinBoxQss(36, 10));
    }
    for (QComboBox *combo : findChildren<QComboBox *>()) {
        restyle(combo, comboBoxQss());
    }
    if (QFrame *strip = findChild<QFrame *>(QStringLiteral("diagStrip"))) {
        strip->setStyleSheet(QStringLiteral(
            "QFrame#diagStrip { background:%1; border:1px solid %2; border-radius:6px; }")
            .arg(Theme::kControlBg, Theme::kBorder));
    }
    for (QCheckBox *box : findChildren<QCheckBox *>()) {
        restyle(box, checkQss());
        box->update();
    }
    for (QPushButton *button : findChildren<QPushButton *>(QStringLiteral("nodeItem"))) {
        restyle(button, nodeButtonQss());
    }
    if (m_diskBar) {
        m_diskBar->setStyleSheet(QString());
    }
    refreshNodeReadiness();
    reloadFromSource();
    updateHikParameterStates();
    updateSickParameterStates();
}

void NodeCommSettingsPage::updateServiceStatusBar()
{
    if (m_startAllButton) {
        m_startAllButton->setText(m_running ? QStringLiteral("停止全部服务")
                                            : QStringLiteral("启动全部服务"));
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
        const bool online = m_cameraNodeConnected || hasPublisher;
        setBadge(m_topicNodeBadge,
                 m_cameraNodeConnected ? QStringLiteral("● 已连接")
                     : (hasPublisher ? QStringLiteral("● 节点在线") : QStringLiteral("● 未连接")),
                 online ? QLatin1String(Theme::kStateSuccess) : QLatin1String(Theme::kStateError));
    }
    if (m_plcBadge) {
        setBadge(m_plcBadge,
                 QStringLiteral("● %1").arg(m_plcStatusText.isEmpty() ? QStringLiteral("待反馈") : m_plcStatusText),
                 QLatin1String(Theme::kTextMuted));
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
        QStringLiteral("<span style='color:%1;font-size:13px;'>●</span> %2/3 项服务就绪")
            .arg(dotColor)
            .arg(readyCount);
    const auto readyName = [](bool ready) { return ready ? QStringLiteral("已就绪") : QStringLiteral("未就绪"); };
    const QString detail = QStringLiteral("　相机 %1 · 后端 %2 · 算法 %3")
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

void NodeCommSettingsPage::updateHikParameterStates()
{
    if (!m_hikStreamType) {
        return;
    }
    const bool ready = m_hikConnected && m_hikParametersReceived;
    const bool imageEditable = ready && m_hikParameters.imageParametersValid;
    const bool exposureEditable = ready && m_hikParameters.exposureValid;
    const bool highlightEditable = ready && m_hikParameters.highlightValid;

    const bool manualExposure = choiceValue(m_hikExposureMode) == 0;
    m_hikStreamType->setEnabled(ready && m_hikParameters.streamTypeValid);
    m_hikFrameRate->setEnabled(ready && m_hikParameters.frameRateValid);
    m_hikBrightness->setEnabled(imageEditable);
    m_hikBrightnessSlider->setEnabled(imageEditable);
    m_hikContrast->setEnabled(imageEditable);
    m_hikContrastSlider->setEnabled(imageEditable);
    m_hikExposureMode->setEnabled(exposureEditable);
    m_hikExposureTime->setEnabled(exposureEditable && manualExposure);
    m_hikExposureSlider->setEnabled(exposureEditable && manualExposure);
    m_hikHighlightEnabled->setEnabled(highlightEditable);
    m_hikHighlightLevel->setEnabled(highlightEditable && m_hikHighlightEnabled->isChecked());
    const bool highlightOn = m_hikHighlightEnabled->isChecked();
    m_hikHighlightStateLabel->setText(highlightOn ? QStringLiteral("已启用") : QStringLiteral("未启用"));
    m_hikHighlightStateLabel->setStyleSheet(labelQss(
        highlightOn ? Theme::kAccent : Theme::kTextMuted, 13, highlightOn ? 600 : 400));
    m_hikRefreshButton->setEnabled(m_hikConnected);
    for (QPushButton *preset : m_cameraCard->findChildren<QPushButton *>()) {
        const QString scene = preset->property("hikScenePreset").toString();
        if (scene == QStringLiteral("lighting")) {
            preset->setEnabled(imageEditable);
            preset->setToolTip(imageEditable ? QStringLiteral("亮度 50、对比度 55")
                                             : QStringLiteral("设备不支持或参数读取失败"));
        } else if (scene == QStringLiteral("reflection")) {
            preset->setEnabled(highlightEditable);
            preset->setToolTip(highlightEditable ? QStringLiteral("开启强光抑制并设为 2 级")
                                                 : QStringLiteral("设备不支持或参数读取失败"));
        } else if (scene == QStringLiteral("gain")) {
            preset->setEnabled(false);
            preset->setToolTip(QStringLiteral("高增益参数尚未接入"));
        }
    }

    // makeSpinStepper 的加减按钮是独立控件，必须与输入框同步禁用。
    for (QSpinBox *spin : {m_hikBrightness, m_hikContrast, m_hikExposureTime}) {
        if (QWidget *wrapper = spin->parentWidget()) {
            for (QPushButton *button : wrapper->findChildren<QPushButton *>()) {
                button->setEnabled(spin->isEnabled());
            }
        }
    }

    const QString unsupported = QStringLiteral("设备不支持或参数读取失败");
    m_hikFrameRate->setToolTip(m_hikFrameRate->isEnabled() ? QString() : unsupported);
    m_hikExposureMode->setToolTip(exposureEditable ? QString() : unsupported);
    m_hikExposureTime->setToolTip(exposureEditable ? QString() : unsupported);
    m_hikHighlightEnabled->setToolTip(highlightEditable ? QString() : unsupported);
    m_hikHighlightLevel->setToolTip(highlightEditable ? QString() : unsupported);
    m_hikHeaderStatus->setText(m_hikConnected ? QStringLiteral("● 已连接") : QStringLiteral("● 待连接"));
    m_hikHeaderStatus->setStyleSheet(hikStatusBadgeQss(
        QLatin1String(m_hikConnected ? Theme::kStateSuccess : Theme::kStateError),
        QLatin1String(Theme::kControlBg)));

    if (!m_hikConnected) {
        m_hikParameterHint->setText(QStringLiteral("● 相机未连接 · 请确认设备与工控机处于同一网段，然后测试连接。"));
    } else if (!m_hikParametersReceived) {
        m_hikParameterHint->setText(QStringLiteral("● 正在读取相机实际预览参数…"));
    } else if (!m_hikParameters.frameRateValid || !m_hikParameters.imageParametersValid
               || !m_hikParameters.exposureValid || !m_hikParameters.highlightValid) {
        m_hikParameterHint->setText(QStringLiteral("● 部分参数不受支持或读取失败，对应控件已禁用。"));
    } else {
        m_hikParameterHint->setText(QStringLiteral("● 已读取设备参数 · 滑块与分段选择修改后立即生效，无需保存。"));
    }
}

void NodeCommSettingsPage::updateSickParameterStates()
{
    if (!m_sickCard || !m_sickScanLines) {
        return;
    }
    const bool ready = m_cameraNodeConnected && m_rosBridgeRunning
        && m_sickParametersReceived && !m_sickWritePending;
    for (QWidget *control : {static_cast<QWidget *>(m_sickScanLines),
                             static_cast<QWidget *>(m_sickExposureTime),
                             static_cast<QWidget *>(m_sickYResolution),
                             static_cast<QWidget *>(m_sickHdrMode)}) {
        control->setEnabled(ready);
    }
    m_sickApplyButton->setEnabled(ready);
    if (QWidget *wrapper = m_sickScanLines->parentWidget()) {
        for (QPushButton *button : wrapper->findChildren<QPushButton *>()) {
            button->setEnabled(ready);
        }
    }

    const bool online = m_cameraStatusReceived && m_cameraNodeConnected;
    m_sickHeaderStatus->setText(online ? QStringLiteral("● 相机已连接") : QStringLiteral("● 相机未连接"));
    m_sickHeaderStatus->setStyleSheet(hikStatusBadgeQss(
        QLatin1String(online ? Theme::kStateSuccess : Theme::kStateError),
        QLatin1String(Theme::kControlBg)));
    if (!m_rosBridgeRunning) {
        m_sickPublisherValue->setText(QStringLiteral("通讯桥未运行"));
    } else if (m_cameraPublisherCount < 0) {
        m_sickPublisherValue->setText(QStringLiteral("正在检测"));
    } else {
        m_sickPublisherValue->setText(QStringLiteral("%1 个").arg(m_cameraPublisherCount));
    }
    m_sickLastMessageValue->setText(m_cameraLastMessageAt.isValid()
        ? m_cameraLastMessageAt.toString(QStringLiteral("HH:mm:ss"))
        : QStringLiteral("尚未接收"));
    m_sickNodeDetail->setText(!m_cameraStatusMessage.isEmpty()
        ? m_cameraStatusMessage
        : QStringLiteral("尚未收到 /camera/status，无法确认相机连接状态。"));

    if (m_sickWritePending) {
        return;
    }
    if (!m_rosBridgeRunning) {
        m_sickParameterHint->setText(QStringLiteral("● ROS 2 通讯桥未运行，采集参数不可写入。"));
    } else if (!m_cameraNodeConnected) {
        m_sickParameterHint->setText(QStringLiteral("● 相机未连接 · 参数仅在实际连接并回读后允许修改。"));
    } else if (!m_sickParametersReceived) {
        m_sickParameterHint->setText(QStringLiteral("● 正在等待 /sick_camera/parameters 回读，不会使用模拟默认值。"));
    } else {
        m_sickParameterHint->setText(QStringLiteral("● 已读取设备实际参数 · 应用后等待节点确认，不会写入本地模拟配置。"));
    }
}

void NodeCommSettingsPage::updateSickScanLength()
{
    if (!m_sickScanLength) {
        return;
    }
    if (!m_sickParametersReceived || m_sickScanLines->value() <= 0 || m_sickYResolution->value() <= 0.0) {
        m_sickScanLength->setText(QStringLiteral("扫描物理长度　等待采集行数与 Y 分辨率回读"));
        return;
    }
    const double length = m_sickScanLines->value() * m_sickYResolution->value();
    m_sickScanLength->setText(QStringLiteral("扫描物理长度　%1 行 × %2 mm/行 = %3 mm")
        .arg(m_sickScanLines->value())
        .arg(QString::number(m_sickYResolution->value(), 'f', 5))
        .arg(length, 0, 'f', 5));
}

void NodeCommSettingsPage::refreshHikCameraInfo()
{
    if (m_cameraStateHint && !m_hikConnected) {
        QString validationError;
        const bool ok = m_config.enabled && m_config.validate(&validationError);
        m_cameraStateHint->setText(ok
            ? QStringLiteral("● 链路诊断就绪：修改 IP 后请先测试连接。")
            : QStringLiteral("● 相机尚未启用或连接配置不完整，请检查设备网络与配置。"));
    }
}

void NodeCommSettingsPage::updateCameraNodeDiagnostics()
{
    const bool hasPublisher = m_rosBridgeRunning && m_cameraPublisherCount > 0;
    if (m_topicNodeBadge) {
        const QString text = m_cameraNodeConnected ? QStringLiteral("● 已连接")
            : (hasPublisher ? QStringLiteral("● 节点在线") : QStringLiteral("● 未连接"));
        const char *color = (m_cameraNodeConnected || hasPublisher)
            ? Theme::kStateSuccess : Theme::kStateError;
        m_topicNodeBadge->setText(text);
        m_topicNodeBadge->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:600;border:none;background:transparent;")
                                            .arg(QLatin1String(color)));
    }
    updateSickParameterStates();
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
    // Topic 与触发模式属于 SICK 采集链路，不在海康预览页展示或修改。
    next.topic = comm.topic;
    next.triggerMode = comm.triggerMode;
    next.autoReconnect = m_autoReconnect->isChecked();
    next.imageRoot = m_imageRoot->text().trimmed();
    next.saveMode = m_saveAllButton->isChecked() ? QStringLiteral("all")
                                                  : (m_saveNoneButton->isChecked() ? QStringLiteral("none")
                                                                                   : QStringLiteral("ng"));
    next.autoClean = m_autoClean->isChecked();
    next.cleanMinGb = m_cleanMinGbSpin->value();
    next.cleanMaxDays = m_cleanMaxDaysSpin->value();

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
    m_timeoutSpin->setValue(m_config.loginTimeoutMs > 0 ? m_config.loginTimeoutMs : 3000);
    m_autoReconnect->setChecked(comm.autoReconnect);
    m_imageRoot->setText(comm.imageRoot);
    m_cleanMinGbSpin->setValue(comm.cleanMinGb);
    m_cleanMaxDaysSpin->setValue(comm.cleanMaxDays);
    m_autoClean->setChecked(comm.autoClean);
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
    m_timeoutSpin->setValue(3000);
    m_autoReconnect->setChecked(true);
    m_imageRoot->setText(QStringLiteral("D:/MZ-VISION/captured_images/"));
    m_cleanMinGbSpin->setValue(50);
    m_cleanMaxDaysSpin->setValue(30);
    m_autoClean->setChecked(true);
    m_saveNgOnlyButton->setChecked(true);
    m_loading = false;
    m_dirty = true;
    emit dirtyChanged(true);
}

QString NodeCommSettingsPage::pendingChangeSummary() const
{
    return QStringLiteral("IP=%1，超时=%2ms，存图=%3，清理阈值=%4GB/%5天")
        .arg(m_cameraIp->text().trimmed())
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
    m_cameraStatusReceived = true;
    m_cameraStatusMessage = detail.trimmed().replace(QLatin1Char('\n'), QStringLiteral(" · "));
    m_cameraLastMessageAt = QDateTime::currentDateTime();
    if (!connected) {
        m_sickWritePending = false;
    }
    updateNodeList();
    updateCameraNodeDiagnostics();
}

void NodeCommSettingsPage::setCameraTopicDiagnostics(bool bridgeRunning, int publisherCount)
{
    m_rosBridgeRunning = bridgeRunning;
    m_cameraPublisherCount = publisherCount;
    if (!bridgeRunning) {
        m_sickWritePending = false;
    }
    updateCameraNodeDiagnostics();
    updateNodeList();
}

void NodeCommSettingsPage::setHikCameraStatus(bool connected, const QString &detail)
{
    if (connected != m_hikConnected) {
        m_hikParametersReceived = false;
        m_hikParameters = HikCameraParameters{};
    }
    m_hikConnected = connected;
    if (m_cameraStateHint) {
        m_cameraStateHint->setText(connected
            ? QStringLiteral("海康相机已连接：%1").arg(detail)
            : QStringLiteral("海康相机未连接：%1").arg(detail.isEmpty() ? QStringLiteral("等待连接") : detail));
    }
    updateHikParameterStates();
    updateNodeList();
}

void NodeCommSettingsPage::setHikCameraParameters(const HikCameraParameters &parameters)
{
    if (!m_hikConnected || !m_hikStreamType) {
        return;
    }
    m_hikParameters = parameters;
    m_hikParametersReceived = true;
    m_updatingHikParameters = true;

    setChoiceValue(m_hikStreamType, parameters.streamType);
    const bool knownFrameRate = setChoiceValue(m_hikFrameRate, parameters.frameRateCode);
    m_hikBrightness->setValue(parameters.brightness);
    m_hikContrast->setValue(parameters.contrast);
    setChoiceValue(m_hikExposureMode, parameters.exposureMode);
    m_hikExposureTime->setValue(parameters.exposureTimeUs);
    m_hikHighlightEnabled->setChecked(parameters.highlightEnabled);
    setChoiceValue(m_hikHighlightLevel, parameters.highlightLevel);

    m_updatingHikParameters = false;
    updateHikParameterStates();
    if (!knownFrameRate && parameters.frameRateValid) {
        m_hikFrameRate->setToolTip(QStringLiteral("设备当前帧率编码为 %1，不在快捷选项中；可直接选择目标帧率。")
                                       .arg(parameters.frameRateCode));
    }
}

void NodeCommSettingsPage::setSickCameraParameters(const SickCameraParametersVM &parameters)
{
    if (!m_sickScanLines) {
        return;
    }
    m_sickParameters = parameters;
    m_sickParametersReceived = true;
    m_updatingSickParameters = true;
    m_sickScanLines->setValue(static_cast<int>(parameters.scanLines));
    m_sickExposureTime->setValue(parameters.exposureTimeUs);
    m_sickYResolution->setValue(parameters.yResolutionMm);
    const int hdr = parameters.hdrMode == QStringLiteral("PresetMedium") ? 1
        : (parameters.hdrMode == QStringLiteral("Off") ? 0 : -1);
    setChoiceValue(m_sickHdrMode, hdr);
    m_updatingSickParameters = false;
    updateSickScanLength();
    updateSickParameterStates();
}

void NodeCommSettingsPage::setSickCameraParameterResult(bool success,
                                                        const QString &message,
                                                        const SickCameraParametersVM &parameters)
{
    m_sickWritePending = false;
    setSickCameraParameters(parameters);
    const QString result = message.trimmed().isEmpty()
        ? (success ? QStringLiteral("设备已确认参数更新") : QStringLiteral("设备拒绝了参数更新"))
        : message.trimmed();
    m_sickParameterHint->setText(success
        ? QStringLiteral("● 写入成功 · %1").arg(result)
        : QStringLiteral("● 写入失败 · %1 · 已恢复设备实际值").arg(result));
}

void NodeCommSettingsPage::setPlcDeviceStatus(const QString &status, const QString &detail)
{
    m_plcStatusText = status.trimmed().isEmpty() ? QStringLiteral("待反馈") : status.trimmed();
    m_plcStatusDetail = detail;
    updateNodeList();
}
