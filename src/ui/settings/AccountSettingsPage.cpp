#include "ui/settings/AccountSettingsPage.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

void addRole(QVBoxLayout *root, QWidget *parent, const QString &title, const QString &body)
{
    auto *panel = createPanel(title, nullptr, parent);
    auto *layout = qobject_cast<QVBoxLayout *>(panel->layout());
    layout->addWidget(settingsHint(body, panel));
    root->addWidget(panel);
}

} // namespace

AccountSettingsPage::AccountSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);
    root->addWidget(settingsSourceBanner(
        QStringLiteral("数据来源：无。工程里没有登录、账户表、改密或会话超时接口。"
                       "本页是标书三级权限的骨架，不写入任何 ini，也不能登录。"),
        this));
    root->addWidget(settingsHint(
        QStringLiteral("当前身份：未登录。已对接的设置项保持改前的可写性，不会被假登录锁住。"),
        this));

    addRole(root,
            this,
            QStringLiteral("操作员"),
            QStringLiteral("监视主页、查看历史。不改工艺偏移、相机登录和节点脚本。"));
    addRole(root,
            this,
            QStringLiteral("技术员"),
            QStringLiteral("可改加工偏移、路径下发和三维显示裁切。相机账号与节点脚本只读。"));
    addRole(root,
            this,
            QStringLiteral("管理员"),
            QStringLiteral("在技术员之上可改海康登录参数；节点脚本与数据库路径计划可写，但本页尚未提供写入。"));

    auto *login = createPanel(QStringLiteral("登录 / 改密 / 会话"), nullptr, this);
    auto *loginLayout = qobject_cast<QVBoxLayout *>(login->layout());
    loginLayout->addWidget(settingsPendingBlock(
        QStringLiteral("用户名"),
        QStringLiteral("待对接 账户存储 / 登录接口（工程内无 auth hook）。"),
        login));
    loginLayout->addWidget(settingsPendingBlock(
        QStringLiteral("密码"),
        QStringLiteral("待对接 账户存储 / 登录接口。不会假装登录成功，也不保存密码。"),
        login));
    loginLayout->addWidget(settingsPendingBlock(
        QStringLiteral("会话超时"),
        QStringLiteral("待对接 会话超时钩子（无超时配置键）。"),
        login));
    root->addWidget(login);

    auto *matrixPanel = createPanel(QStringLiteral("可写项权限矩阵（计划，未生效）"), nullptr, this);
    auto *matrixLayout = qobject_cast<QVBoxLayout *>(matrixPanel->layout());
    auto *table = new QTableWidget(5, 5, matrixPanel);
    table->setHorizontalHeaderLabels({QStringLiteral("设置项"),
                                      QStringLiteral("操作员"),
                                      QStringLiteral("技术员"),
                                      QStringLiteral("管理员"),
                                      QStringLiteral("当前")});
    const QStringList rows[] = {
        {QStringLiteral("XYZ 偏移、下发路径到 PLC"),
         QStringLiteral("只读"),
         QStringLiteral("可写"),
         QStringLiteral("可写"),
         QStringLiteral("未锁定，与改前一样可写")},
        {QStringLiteral("海康登录 / 码流 / 传输"),
         QStringLiteral("不可见"),
         QStringLiteral("只读"),
         QStringLiteral("可写"),
         QStringLiteral("未锁定，当前可写")},
        {QStringLiteral("三维 FlipX 与左右裁列"),
         QStringLiteral("只读"),
         QStringLiteral("可写"),
         QStringLiteral("可写"),
         QStringLiteral("未锁定，当前可写")},
        {QStringLiteral("节点脚本、数据库路径"),
         QStringLiteral("不可见"),
         QStringLiteral("只读"),
         QStringLiteral("计划可写"),
         QStringLiteral("界面保持只读")},
        {QStringLiteral("登录、改密、会话超时"),
         QStringLiteral("—"),
         QStringLiteral("—"),
         QStringLiteral("计划可写"),
         QStringLiteral("待对接，无账户存储")},
    };
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 5; ++column) {
            auto *item = new QTableWidgetItem(rows[row].at(column));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            table->setItem(row, column, item);
        }
    }
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setWordWrap(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setStretchLastSection(true);
    table->resizeRowsToContents();
    table->setMinimumHeight(220);
    table->setStyleSheet(QStringLiteral(
                             "QTableWidget {"
                             "  background: %1;"
                             "  color: %2;"
                             "  border: 1px solid %3;"
                             "  gridline-color: %4;"
                             "  font-size: 13px;"
                             "}"
                             "QHeaderView::section {"
                             "  background: %5;"
                             "  color: %2;"
                             "  border: none;"
                             "  padding: 6px;"
                             "}")
                             .arg(Theme::kControlBg,
                                  Theme::kTextPrimary,
                                  Theme::kBorder,
                                  Theme::kDivider,
                                  Theme::kPanelBg));
    matrixLayout->addWidget(table);
    matrixLayout->addWidget(settingsHint(
        QStringLiteral("矩阵只说明计划权限。当前列写明实际是否锁定，避免把未接入的权限当成已经生效。"),
        matrixPanel));
    root->addWidget(matrixPanel);
    root->addStretch(1);
}

QString AccountSettingsPage::bindingSummary() const
{
    return QStringLiteral("权限骨架未对接账户存储。矩阵是计划，不是已经生效的锁。");
}
