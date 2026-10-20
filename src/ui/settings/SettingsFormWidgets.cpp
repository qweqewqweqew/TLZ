#include "ui/settings/SettingsFormWidgets.h"

#include "ElaCheckBox.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"

#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QString fieldStyle()
{
    return QStringLiteral(
               "QLineEdit, QSpinBox {"
               "  background: %1;"
               "  color: %2;"
               "  border: 1px solid %3;"
               "  border-radius: 5px;"
               "  padding: 0 10px;"
               "  min-height: 32px;"
               "  font-size: 13px;"
               "  selection-background-color: %4;"
               "}"
               "QLineEdit:focus, QSpinBox:focus { border: 1px solid %5; }"
               "QLineEdit:read-only, QLineEdit:disabled, QSpinBox:disabled {"
               "  color: %6;"
               "  background: %7;"
               "}"
               "QSpinBox::up-button, QSpinBox::down-button {"
               "  width: 14px;"
               "  background: %8;"
               "  border: none;"
               "}")
        .arg(Theme::kControlBg,
             Theme::kTextPrimary,
             Theme::kBorder,
             Theme::kSelectionBg,
             Theme::kAccent,
             Theme::kTextMuted,
             Theme::kPanelBg,
             Theme::kHoverBg);
}

} // namespace

QLabel *settingsHint(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, QStringLiteral("sectionHint"));
    label->setParent(parent);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    Theme::bindStyle(label, []() {
            return QStringLiteral("color:%1;font-size:12px;border:none;background:transparent;")
            .arg(Theme::kTextMuted);
    });
    return label;
}

QLabel *settingsFieldLabel(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, QStringLiteral("metricName"));
    label->setParent(parent);
    Theme::bindStyle(label, []() {
        return QStringLiteral("color:%1;font-size:12px;border:none;background:transparent;")
            .arg(Theme::kTextSecondary);
    });
    return label;
}

QLabel *settingsSourceBanner(const QString &text, QWidget *parent)
{
    auto *label = settingsHint(text, parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    Theme::bindStyle(label, []() {
        return QStringLiteral("color:%1;font-size:12px;background:transparent;"
                              "border:none;border-left:3px solid %2;padding:4px 10px;")
            .arg(Theme::kTextMuted, Theme::kBorder);
    });
    return label;
}

QLineEdit *settingsLineEdit(QWidget *parent)
{
    auto *edit = new QLineEdit(parent);
    edit->setMaxLength(256);
    edit->setMaximumWidth(420);
    Theme::bindStyle(edit, []() { return fieldStyle(); });
    return edit;
}

QSpinBox *settingsSpinBox(QWidget *parent)
{
    auto *spin = new QSpinBox(parent);
    spin->setMaximumWidth(160);
    Theme::bindStyle(spin, []() { return fieldStyle(); });
    return spin;
}

QPlainTextEdit *settingsReadOnlyText(QWidget *parent)
{
    auto *edit = new QPlainTextEdit(parent);
    edit->setReadOnly(true);
    edit->setMinimumHeight(108);
    Theme::bindStyle(edit, []() {
        return QStringLiteral(
                   "QPlainTextEdit {"
                   "  background: %1;"
                   "  color: %2;"
                   "  border: 1px solid %3;"
                   "  border-radius: 4px;"
                   "  padding: 6px;"
                   "  font-size: 12px;"
                   "}")
            .arg(Theme::kLogBg, Theme::kTextSecondary, Theme::kDivider);
    });
    return edit;
}

ElaCheckBox *settingsCheckBox(const QString &text, QWidget *parent)
{
    auto *box = new ElaCheckBox(text, parent);
    Theme::bindStyle(box, []() {
        return QStringLiteral("color:%1;font-size:12px;border:none;background:transparent;")
            .arg(Theme::kTextPrimary);
    });
    return box;
}

QWidget *settingsPendingBlock(const QString &title, const QString &note, QWidget *parent)
{
    auto *row = new QWidget(parent);
    row->setObjectName(QStringLiteral("settingsPendingRow"));
    Theme::bindStyle(row, []() {
        return QStringLiteral("QWidget#settingsPendingRow { background:%1; border:1px solid %2;"
                              " border-left:3px solid %3; border-radius:6px; }")
            .arg(Theme::kPanelBg, Theme::kBorder, Theme::kStateWarning);
    });
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(10, 7, 10, 7);
    layout->setSpacing(4);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);

    auto *name = settingsSectionHeading(title, row);
    header->addWidget(name, 1);

    auto *badge = makeLabel(QStringLiteral("待对接"));
    badge->setParent(row);
    badge->setAlignment(Qt::AlignCenter);
    badge->setMinimumWidth(54);
    badge->setFixedHeight(20);
    Theme::bindStyle(badge, []() {
        return QStringLiteral("color:%1;background:%2;border:1px solid %3;border-radius:4px;"
                              "font-size:11px;padding:1px 7px;")
            .arg(Theme::kStateWarning, Theme::kWarningBg, Theme::kBorder);
    });
    header->addWidget(badge, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    layout->addWidget(settingsHint(note, row));
    return row;
}

QFrame *settingsGroupPanel(const QString &title, const QString &iconResource, QWidget *parent)
{
    auto *panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("settingsGroupPanel"));
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    Theme::bindStyle(panel, []() {
        return QStringLiteral("QFrame#settingsGroupPanel { background:%1; border:1px solid %2;"
                              " border-top:3px solid %3; border-radius:7px; }")
            .arg(Theme::kPanelBg, Theme::kBorder, Theme::markColor());
    });
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);

    auto *header = new QWidget(panel);
    header->setObjectName(QStringLiteral("settingsPanelHeader"));
    Theme::bindStyle(header, []() {
        return QStringLiteral("QWidget#settingsPanelHeader { background:transparent; border:none; }");
    });
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);

    auto *iconBadge = new QFrame(header);
    iconBadge->setObjectName(QStringLiteral("settingsPanelIcon"));
    iconBadge->setFixedSize(34, 34);
    Theme::bindStyle(iconBadge, []() {
        return QStringLiteral("QFrame#settingsPanelIcon { background:%1; border:1px solid %2; border-radius:6px; }")
            .arg(Theme::kControlBg, Theme::kBorder);
    });
    auto *icon = makeLabel(QString());
    icon->setParent(iconBadge);
    icon->setGeometry(8, 8, 18, 18);
    const auto updateIcon = [icon, iconResource]() {
        icon->setPixmap(tintedIcon(iconResource, QColor(Theme::markColor()), 18));
    };
    updateIcon();
    Theme::watch(icon, updateIcon);

    auto *copy = new QWidget(header);
    copy->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    auto *copyLayout = new QVBoxLayout(copy);
    copyLayout->setContentsMargins(0, 0, 0, 0);
    copyLayout->setSpacing(2);
    auto *titleLabel = makeLabel(title, QStringLiteral("panelTitle"));
    titleLabel->setParent(copy);
    auto *subtitle = settingsHint(QString(), copy);
    static const QHash<QString, QString> subtitles = {
        {QStringLiteral("节点与服务"), QStringLiteral("运行控制与启动配置")},
        {QStringLiteral("数据库与存储"), QStringLiteral("连接状态与本地数据源")},
        {QStringLiteral("设备通信"), QStringLiteral("现场设备链路配置")},
        {QStringLiteral("扫描采集"), QStringLiteral("相机连接与图像采集")},
        {QStringLiteral("检测判定"), QStringLiteral("粒子识别与质量规则")},
        {QStringLiteral("坐标标定"), QStringLiteral("相机、工件与机床坐标")},
        {QStringLiteral("坐标与示教"), QStringLiteral("补偿参数与现场示教")},
        {QStringLiteral("修磨参数"), QStringLiteral("加工参数与安全边界")},
        {QStringLiteral("策略与复检"), QStringLiteral("任务策略与质量复核")},
        {QStringLiteral("账号与角色"), QStringLiteral("当前身份与用户管理")},
        {QStringLiteral("安全联锁与会话"), QStringLiteral("权限范围与登录保护")},
        {QStringLiteral("操作审计"), QStringLiteral("关键操作记录与追溯")},
    };
    subtitle->setText(subtitles.value(title));
    Theme::bindStyle(subtitle, []() {
        return QStringLiteral("color:%1;font-size:11px;border:none;background:transparent;")
            .arg(Theme::kTextMuted);
    });
    copyLayout->addWidget(titleLabel);
    copyLayout->addWidget(subtitle);
    headerLayout->addWidget(iconBadge, 0, Qt::AlignVCenter);
    headerLayout->addWidget(copy, 1, Qt::AlignVCenter);
    layout->addWidget(header);

    auto *divider = new QFrame(panel);
    divider->setFixedHeight(1);
    Theme::bindStyle(divider, []() {
        return QStringLiteral("background:%1;border:none;").arg(Theme::kDivider);
    });
    layout->addWidget(divider);
    return panel;
}

QLabel *settingsSectionHeading(const QString &title, QWidget *parent)
{
    auto *heading = makeLabel(title, QStringLiteral("sectionTitle"));
    heading->setParent(parent);
    Theme::bindStyle(heading, []() {
        return QStringLiteral("color:%1;font-size:13px;font-weight:600;border:none;background:transparent;")
            .arg(Theme::kTextPrimary);
    });
    return heading;
}

QFormLayout *settingsFormLayout(QWidget *host)
{
    auto *form = new QFormLayout(host);
    form->setContentsMargins(0, 2, 0, 2);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

void addSettingsField(QFormLayout *form,
                      const QString &label,
                      QWidget *field,
                      const QString &note,
                      QWidget *parent)
{
    auto *name = settingsFieldLabel(label, parent);
    name->setMinimumWidth(112);
    name->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addRow(name, field);
    if (!note.isEmpty()) {
        auto *spacer = new QLabel(parent);
        spacer->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
        form->addRow(spacer, settingsHint(note, parent));
    }
}

QTabWidget *settingsTabs(QWidget *parent)
{
    auto *tabs = new QTabWidget(parent);
    tabs->setObjectName(QStringLiteral("settingsTabs"));
    tabs->setDocumentMode(true);
    tabs->tabBar()->setDrawBase(false);
    tabs->tabBar()->setExpanding(false);
    tabs->tabBar()->setIconSize(QSize(16, 16));
    Theme::bindStyle(tabs, []() {
        const char *selected = Theme::current() == Theme::Id::Light ? Theme::kTitle : Theme::kAccent;
        return QStringLiteral(
                   "QTabWidget#settingsTabs::pane { border:none; top:0px; }"
                   "QTabWidget#settingsTabs > QStackedWidget { background:transparent; }"
                   "QTabWidget#settingsTabs QTabBar::tab {"
                   "  background:%1; color:%2; border:1px solid %3; border-radius:5px;"
                   "  padding:8px 16px; margin:0 8px 10px 0; min-width:92px; min-height:20px;"
                   "  font-size:13px;"
                   "}"
                   "QTabWidget#settingsTabs QTabBar::tab:selected {"
                   "  background:%4; color:%5; border-color:%5;"
                   "}"
                   "QTabWidget#settingsTabs QTabBar::tab:hover:!selected { background:%6; }")
            .arg(Theme::kPanelBg,
                 Theme::kTextSecondary,
                 Theme::kBorder,
                 Theme::kControlBg,
                 selected,
                 Theme::kHoverBg);
    });
    return tabs;
}

QVBoxLayout *addSettingsTab(QTabWidget *tabs,
                            const QString &title,
                            bool scrollable,
                            const QString &iconPath)
{
    auto *content = new QWidget();
    content->setObjectName(QStringLiteral("settingsTabContent"));
    content->setStyleSheet(QStringLiteral("QWidget#settingsTabContent { background:transparent; }"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, scrollable ? 8 : 0, 0);
    layout->setSpacing(12);
    if (!scrollable) {
        tabs->addTab(content, iconPath.isEmpty() ? QIcon() : QIcon(iconPath), title);
        return layout;
    }

    auto *scroll = new QScrollArea();
    scroll->setObjectName(QStringLiteral("settingsTabScroll"));
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea#settingsTabScroll { background:transparent; border:none; }"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(content);
    tabs->addTab(scroll, iconPath.isEmpty() ? QIcon() : QIcon(iconPath), title);
    return layout;
}
