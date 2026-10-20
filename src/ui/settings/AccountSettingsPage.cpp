#include "ui/settings/AccountSettingsPage.h"

#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "auth/AuthService.h"
#include "auth/PasswordHasher.h"
#include "auth/PasswordPolicy.h"
#include "ui/dialogs/LoginDialog.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>

#include <functional>

namespace {

constexpr int kUserIdRole = Qt::UserRole;
constexpr int kUserLockedRole = Qt::UserRole + 1;
constexpr int kUserEnabledRole = Qt::UserRole + 2;
constexpr int kUserRoleRole = Qt::UserRole + 3;
constexpr int kAuditRows = 200;

QString tableStyle()
{
    return QStringLiteral(
                             "QTableWidget {"
                             "  background: %1;"
                             "  color: %2;"
                             "  border: 1px solid %3;"
                             "  gridline-color: %4;"
                             "  font-size: 12px;"
               "  selection-background-color: %6;"
                             "}"
                             "QHeaderView::section {"
                             "  background: %5;"
                             "  color: %2;"
                             "  border: none;"
                             "  padding: 4px;"
                             "}")
                             .arg(Theme::kControlBg,
                                  Theme::kTextPrimary,
                                  Theme::kBorder,
                                  Theme::kDivider,
             Theme::kPanelBg,
             Theme::kSelectionBg);
}

QTableWidget *makeTable(const QStringList &headers, QWidget *parent)
{
    auto *table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setWordWrap(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    Theme::bindStyle(table, []() { return tableStyle(); });
    return table;
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QString localTime(const QDateTime &time)
{
    return time.isValid() ? time.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                          : QStringLiteral("—");
}

QString actionDisplayName(const QString &action)
{
    static const QHash<QString, QString> kNames = {
        {QStringLiteral("login"), QStringLiteral("登录")},
        {QStringLiteral("logout"), QStringLiteral("退出登录")},
        {QStringLiteral("session.timeout"), QStringLiteral("会话超时")},
        {QStringLiteral("password.change"), QStringLiteral("修改密码")},
        {QStringLiteral("account.bootstrap"), QStringLiteral("创建首个管理员")},
        {QStringLiteral("account.create"), QStringLiteral("新建账户")},
        {QStringLiteral("account.register"), QStringLiteral("现场注册")},
        {QStringLiteral("account.reset_password"), QStringLiteral("重置密码")},
        {QStringLiteral("account.set_role"), QStringLiteral("修改角色")},
        {QStringLiteral("account.enable"), QStringLiteral("启用账户")},
        {QStringLiteral("account.disable"), QStringLiteral("停用账户")},
        {QStringLiteral("account.unlock"), QStringLiteral("解锁账户")},
        {QStringLiteral("security.session_timeout"), QStringLiteral("修改会话超时")},
        {QStringLiteral("settings.save"), QStringLiteral("保存设置")},
    };
    return kNames.value(action, action);
}

QString comboStyle()
{
    return QStringLiteral(
               "QComboBox {"
               "  background: %1;"
               "  color: %2;"
               "  border: 1px solid %3;"
               "  border-radius: 4px;"
               "  padding: 0 8px;"
               "  min-height: 28px;"
               "  font-size: 12px;"
               "}"
               "QComboBox QAbstractItemView {"
               "  background: %1;"
               "  color: %2;"
               "  selection-background-color: %4;"
               "}")
        .arg(Theme::kControlBg, Theme::kTextPrimary, Theme::kBorder, Theme::kSelectionBg);
}

bool confirmAction(QWidget *parent, const QString &title, const QString &text)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box.setButtonText(QMessageBox::Yes, QStringLiteral("确定"));
    box.setButtonText(QMessageBox::No, QStringLiteral("取消"));
    box.setDefaultButton(QMessageBox::No);
    box.setStyleSheet(QStringLiteral(
                          "QMessageBox { background-color: %1; }"
                          "QMessageBox QLabel { color: %2; min-width: 320px; font-size: 14px; }")
                          .arg(QLatin1String(Theme::kPanelBg), QLatin1String(Theme::kTextPrimary)));
    return box.exec() == QMessageBox::Yes;
}

// 表单对话框：提交失败时留在对话框里显示原因，成功才关闭。
class FormDialog
{
public:
    FormDialog(QWidget *parent, const QString &title, const QString &intro, const QString &okText)
        : m_dialog(parent)
    {
        m_dialog.setWindowTitle(title);
        m_dialog.setWindowFlags(m_dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
        m_dialog.setModal(true);
        m_dialog.setFixedWidth(420);
        m_dialog.setObjectName(QStringLiteral("accountFormDialog"));
        m_dialog.setStyleSheet(QStringLiteral("QDialog#accountFormDialog { background: %1; }")
                                   .arg(Theme::kPanelBg));
        m_layout = new QVBoxLayout(&m_dialog);
        m_layout->setContentsMargins(18, 14, 18, 14);
        m_layout->setSpacing(6);
        auto *heading = makeLabel(title, QStringLiteral("panelTitle"));
        heading->setParent(&m_dialog);
        m_layout->addWidget(heading);
        if (!intro.isEmpty()) {
            m_layout->addWidget(settingsHint(intro, &m_dialog));
        }

        m_error = settingsHint(QString(), &m_dialog);
        m_error->setStyleSheet(QStringLiteral(
                                   "color:%1;font-size:13px;border:none;background:transparent;")
                                   .arg(Theme::kStateError));
        m_error->hide();

        m_buttons = new QHBoxLayout();
        m_buttons->setContentsMargins(0, 8, 0, 0);
        m_buttons->addStretch(1);
        auto *cancel = new ElaPushButton(QStringLiteral("取消"), &m_dialog);
        m_ok = new ElaPushButton(okText, &m_dialog);
        cancel->setMinimumSize(84, 30);
        m_ok->setMinimumSize(98, 30);
        applySecondaryButtonStyle(cancel);
        applyPrimaryButtonStyle(m_ok);
        m_buttons->addWidget(cancel);
        m_buttons->addWidget(m_ok);
        QObject::connect(cancel, &ElaPushButton::clicked, &m_dialog, &QDialog::reject);
    }

    QLineEdit *addLine(const QString &label, bool password)
    {
        m_layout->addWidget(settingsFieldLabel(label, &m_dialog));
        auto *edit = settingsLineEdit(&m_dialog);
        edit->setMaximumWidth(QWIDGETSIZE_MAX);
        if (password) {
            edit->setEchoMode(QLineEdit::Password);
            edit->setMaxLength(PasswordPolicy::kMaxPasswordLength);
            edit->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhNoPredictiveText
                                      | Qt::ImhSensitiveData);
        } else {
            edit->setMaxLength(PasswordPolicy::kMaxUsernameLength);
        }
        m_layout->addWidget(edit);
        m_secrets.append(password ? edit : nullptr);
        return edit;
    }

    QComboBox *addRoleCombo(const QString &label, Role current)
    {
        m_layout->addWidget(settingsFieldLabel(label, &m_dialog));
        auto *combo = new QComboBox(&m_dialog);
        combo->setStyleSheet(comboStyle());
        for (Role role : Auth::assignableRoles()) {
            if (role == Role::Administrator) {
                continue;
            }
            combo->addItem(Auth::roleDisplayName(role), static_cast<int>(role));
        }
        const int index = combo->findData(static_cast<int>(current));
        combo->setCurrentIndex(index >= 0 ? index : 0);
        m_layout->addWidget(combo);
        return combo;
    }

    void addHint(const QString &text) { m_layout->addWidget(settingsHint(text, &m_dialog)); }

    void setError(const QString &text)
    {
        m_error->setText(text);
        m_error->setVisible(!text.isEmpty());
    }

    bool exec(const std::function<bool()> &submit)
    {
        m_layout->addWidget(m_error);
        m_layout->addLayout(m_buttons);
        QObject::connect(m_ok, &ElaPushButton::clicked, &m_dialog, [this, submit]() {
            if (submit()) {
                m_dialog.accept();
            }
        });
        const bool accepted = m_dialog.exec() == QDialog::Accepted;
        for (QLineEdit *edit : qAsConst(m_secrets)) {
            if (edit) {
                edit->clear();
            }
        }
        return accepted;
    }

private:
    QDialog m_dialog;
    QVBoxLayout *m_layout{nullptr};
    QHBoxLayout *m_buttons{nullptr};
    QLabel *m_error{nullptr};
    ElaPushButton *m_ok{nullptr};
    QVector<QLineEdit *> m_secrets;
};

ElaPushButton *makeActionButton(const QString &text, bool primary, QWidget *parent)
{
    auto *button = new ElaPushButton(text, parent);
    button->setMinimumSize(88, 30);
    if (primary) {
        applyPrimaryButtonStyle(button);
    } else {
        applySecondaryButtonStyle(button);
    }
    return button;
}

QLabel *makeLockedHint(const QString &text, QWidget *parent)
{
    auto *label = settingsHint(text, parent);
    Theme::bindStyle(label, []() {
        return QStringLiteral("color:%1;font-size:12px;border:none;border-radius:4px;"
                              "padding:5px 8px;background:%2;")
            .arg(Theme::kStateWarning, Theme::kWarningBg);
    });
    return label;
}

} // namespace

AccountSettingsPage::AccountSettingsPage(QWidget *parent)
    : SettingsSubPage(parent)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    // ---- 当前身份 ----
    auto *identityPanel = settingsGroupPanel(QStringLiteral("账号与角色"),
                                              QStringLiteral(":/img/nav_settings.svg"), this);
    root->addWidget(identityPanel, 1);
    auto *identityLayout = qobject_cast<QVBoxLayout *>(identityPanel->layout());
    m_identity = makeLabel(QString());
    m_identity->setParent(identityPanel);
    Theme::bindStyle(m_identity, []() {
        return QStringLiteral("color:%1;font-size:14px;border:none;background:transparent;")
            .arg(Theme::kTextPrimary);
    });
    identityLayout->addWidget(m_identity);
    m_identityScope = settingsHint(QString(), identityPanel);
    identityLayout->addWidget(m_identityScope);
    auto *identityButtons = new QHBoxLayout();
    identityButtons->setContentsMargins(0, 4, 0, 0);
    identityButtons->setSpacing(8);
    m_loginButton = makeActionButton(QStringLiteral("登录"), true, identityPanel);
    m_changePasswordButton = makeActionButton(QStringLiteral("修改密码"), false, identityPanel);
    m_logoutButton = makeActionButton(QStringLiteral("退出登录"), false, identityPanel);
    identityButtons->addWidget(m_loginButton);
    identityButtons->addWidget(m_changePasswordButton);
    identityButtons->addWidget(m_logoutButton);
    identityButtons->addStretch(1);
    identityLayout->addLayout(identityButtons);
    identityLayout->addWidget(settingsHint(
        QStringLiteral("监视画面、历史记录、启停节点不需要登录，与之前的现场操作一致。"
                       "登录后按角色解锁对应的设置项。"),
        identityPanel));
    identityLayout->addWidget(settingsSourceBanner(
        QStringLiteral("账户保存在 database.ini 指向的 SQLite 数据库。密码用 PBKDF2-SHA256 加 %1 字节随机盐、"
                       "%2 轮迭代保存，不存明文。账户由管理员在「账户管理」创建，或在登录界面「注册」并由管理员现场授权。")
            .arg(PasswordHasher::kSaltBytes)
            .arg(PasswordHasher::kDefaultIterations),
        identityPanel));

    // ---- 账户管理 ----
    identityLayout->addWidget(settingsSectionHeading(QStringLiteral("账户管理"), identityPanel));
    m_usersLocked = makeLockedHint(QStringLiteral("需要管理员登录后才能查看和管理账户。"), identityPanel);
    identityLayout->addWidget(m_usersLocked);
    auto *userButtons = new QHBoxLayout();
    userButtons->setContentsMargins(0, 0, 0, 0);
    userButtons->setSpacing(8);
    m_createButton = makeActionButton(QStringLiteral("新建账户"), true, identityPanel);
    m_resetButton = makeActionButton(QStringLiteral("重置密码"), false, identityPanel);
    m_roleButton = makeActionButton(QStringLiteral("修改角色"), false, identityPanel);
    m_enableButton = makeActionButton(QStringLiteral("停用"), false, identityPanel);
    m_unlockButton = makeActionButton(QStringLiteral("解锁"), false, identityPanel);
    for (ElaPushButton *button :
         {m_createButton, m_resetButton, m_roleButton, m_enableButton, m_unlockButton}) {
        userButtons->addWidget(button);
    }
    userButtons->addStretch(1);
    identityLayout->addLayout(userButtons);
    m_users = makeTable({QStringLiteral("用户名"),
                         QStringLiteral("角色"),
                         QStringLiteral("状态"),
                         QStringLiteral("最后登录"),
                         QStringLiteral("创建者")},
                        identityPanel);
    m_users->setWordWrap(false);
    m_users->verticalHeader()->setDefaultSectionSize(36);
    m_users->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    identityLayout->addWidget(m_users, 1);
    identityLayout->addWidget(settingsHint(
        QStringLiteral("系统只有一个管理员，新建账户只能是操作员或技术员；管理员不能修改自己的角色、不能停用自己。"
                       "账户不提供删除，离职人员请停用，以保留审计记录的对应关系。"),
        identityPanel));

    // ---- 角色与权限 ----
    auto *securityPanel = settingsGroupPanel(QStringLiteral("安全联锁与会话"),
                                              QStringLiteral(":/img/kpi_alert.svg"), this);
    root->addWidget(securityPanel, 1);
    auto *securityLayout = qobject_cast<QVBoxLayout *>(securityPanel->layout());
    securityLayout->addWidget(settingsSectionHeading(QStringLiteral("角色与权限"), securityPanel));
    const QVector<Permission> permissions = Auth::allPermissions();
    auto *matrix = makeTable({QStringLiteral("权限点"),
                              Auth::roleDisplayName(Role::Operator),
                              Auth::roleDisplayName(Role::Technician),
                              Auth::roleDisplayName(Role::Administrator)},
                             securityPanel);
    matrix->setSelectionMode(QAbstractItemView::NoSelection);
    matrix->setFocusPolicy(Qt::NoFocus);
    matrix->setWordWrap(false);
    matrix->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    matrix->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    matrix->setRowCount(permissions.size());
    for (int row = 0; row < permissions.size(); ++row) {
        const Permission permission = permissions.at(row);
        auto *name = readOnlyItem(Auth::permissionDisplayName(permission));
        name->setToolTip(name->text());
        matrix->setItem(row, 0, name);
        int column = 1;
        for (Role role : Auth::assignableRoles()) {
            const bool allowed = Auth::roleAllows(role, permission);
            auto *cell = readOnlyItem(allowed ? QStringLiteral("✓") : QStringLiteral("—"));
            cell->setTextAlignment(Qt::AlignCenter);
            cell->setForeground(QColor(allowed ? Theme::kStateSuccess : Theme::kTextMuted));
            matrix->setItem(row, column++, cell);
        }
    }
    constexpr int kMatrixRowHeight = 30;
    constexpr int kMatrixRoleColumnWidth = 84;
    QHeaderView *matrixHeader = matrix->horizontalHeader();
    matrixHeader->setStretchLastSection(false);
    matrixHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < matrix->columnCount(); ++column) {
        matrixHeader->setSectionResizeMode(column, QHeaderView::Fixed);
        matrix->setColumnWidth(column, kMatrixRoleColumnWidth);
    }
    matrixHeader->setFixedHeight(30);
    matrix->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    matrix->verticalHeader()->setDefaultSectionSize(kMatrixRowHeight);
    matrix->setFixedHeight(matrixHeader->height() + permissions.size() * kMatrixRowHeight
                           + 2 * matrix->frameWidth());
    securityLayout->addWidget(matrix);
    securityLayout->addWidget(settingsHint(
        QStringLiteral("角色逐级包含。没有权限的设置项照样可以查看，但控件置灰；"
                       "保存时还会再校验一次，并记入操作审计。「确认报警」在报警页接入后生效。"),
        securityPanel));

    // ---- 会话与锁定 ----
    securityLayout->addWidget(settingsSectionHeading(QStringLiteral("会话与登录保护"), securityPanel));
    auto *sessionHost = new QWidget(securityPanel);
    QFormLayout *sessionForm = settingsFormLayout(sessionHost);
    m_sessionTimeout = settingsSpinBox(sessionHost);
    m_sessionTimeout->setRange(AuthService::kMinSessionTimeoutMinutes,
                               AuthService::kMaxSessionTimeoutMinutes);
    m_sessionTimeout->setSuffix(QStringLiteral(" 分钟"));
    addSettingsField(sessionForm,
                     QStringLiteral("空闲超时"),
                     m_sessionTimeout,
                     QStringLiteral("空闲超过该时长自动退出登录，界面退回只读，未保存的修改会被丢弃。"
                                    "只有管理员能修改，改完点页底「保存」。"),
                     sessionHost);
    securityLayout->addWidget(sessionHost);

    auto *lockHost = new QWidget(securityPanel);
    QFormLayout *lockForm = settingsFormLayout(lockHost);
    const auto addFact = [lockForm, lockHost](const QString &name, const QString &value) {
        auto *label = settingsHint(value, lockHost);
        Theme::bindStyle(label, []() {
            return QStringLiteral("color:%1;font-size:14px;border:none;background:transparent;")
                .arg(Theme::kTextPrimary);
        });
        addSettingsField(lockForm, name, label, QString(), lockHost);
    };
    addFact(QStringLiteral("失败锁定"),
            QStringLiteral("连续输错 %1 次锁定 %2 分钟，不存在的用户名同样计数")
                .arg(AuthService::kMaxFailedAttempts)
                .arg(AuthService::kLockoutMinutes));
    addFact(QStringLiteral("首次改密"), QStringLiteral("新建账户或被重置密码后，首次登录必须先改密"));
    addFact(QStringLiteral("密码规则"), PasswordPolicy::passwordRequirementText());
    securityLayout->addWidget(lockHost);
    securityLayout->addStretch(1);

    // ---- 操作审计 ----
    auto *auditPanel = settingsGroupPanel(QStringLiteral("操作审计"),
                                           QStringLiteral(":/img/kpi_check.svg"), this);
    root->addWidget(auditPanel, 1);
    auto *auditLayout = qobject_cast<QVBoxLayout *>(auditPanel->layout());
    m_auditLocked = makeLockedHint(QStringLiteral("需要管理员登录后才能查看操作审计。"), auditPanel);
    auditLayout->addWidget(m_auditLocked);
    auto *auditHeader = new QHBoxLayout();
    auditHeader->setContentsMargins(0, 0, 0, 0);
    auditHeader->setSpacing(8);
    m_auditCaption = settingsHint(QStringLiteral("最近 %1 条，只追加，不记录任何密码。").arg(kAuditRows), auditPanel);
    auditHeader->addWidget(m_auditCaption, 1);
    m_auditRefreshButton = makeActionButton(QStringLiteral("刷新"), false, auditPanel);
    auditHeader->addWidget(m_auditRefreshButton);
    auditLayout->addLayout(auditHeader);
    m_audit = makeTable({QStringLiteral("时间"),
                         QStringLiteral("用户"),
                         QStringLiteral("角色"),
                         QStringLiteral("操作"),
                         QStringLiteral("对象"),
                         QStringLiteral("结果"),
                         QStringLiteral("详情")},
                        auditPanel);
    m_audit->setSelectionMode(QAbstractItemView::NoSelection);
    m_audit->setWordWrap(false);
    m_audit->setTextElideMode(Qt::ElideRight);
    m_audit->verticalHeader()->setDefaultSectionSize(32);
    auditLayout->addWidget(m_audit, 1);

    connect(m_loginButton, &ElaPushButton::clicked, this, [this]() { LoginDialog::runLogin(this); });
    connect(m_changePasswordButton, &ElaPushButton::clicked, this, [this]() {
        if (LoginDialog::runChangePassword(this)) {
            showResult(true, QStringLiteral("密码已修改。"));
        }
    });
    connect(m_logoutButton, &ElaPushButton::clicked, this, []() { AuthService::instance().logout(); });
    connect(m_sessionTimeout, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { emit dirtyChanged(isDirty()); });
    connect(m_users, &QTableWidget::itemSelectionChanged, this, &AccountSettingsPage::updateUserActions);
    connect(m_createButton, &ElaPushButton::clicked, this, &AccountSettingsPage::createUser);
    connect(m_resetButton, &ElaPushButton::clicked, this, &AccountSettingsPage::resetPassword);
    connect(m_roleButton, &ElaPushButton::clicked, this, &AccountSettingsPage::changeRole);
    connect(m_enableButton, &ElaPushButton::clicked, this, &AccountSettingsPage::toggleEnabled);
    connect(m_unlockButton, &ElaPushButton::clicked, this, &AccountSettingsPage::unlockUser);
    connect(m_auditRefreshButton, &ElaPushButton::clicked, this, &AccountSettingsPage::refreshAudit);

    AuthService &auth = AuthService::instance();
    connect(&auth, &AuthService::sessionChanged, this, [this]() {
        if (!isDirty()) {
            reloadFromSource();
        } else {
            refreshIdentity();
            refreshUsers();
            refreshAudit();
        }
    });
    connect(&auth, &AuthService::accountsChanged, this, [this]() {
        refreshUsers();
        refreshAudit();
    });
    reloadFromSource();
}

QString AccountSettingsPage::bindingSummary() const
{
    return QStringLiteral("绑定 SQLite 账户表。三级角色已生效；会话超时由管理员在本页修改。");
}

bool AccountSettingsPage::isEditable() const
{
    return true;
}

bool AccountSettingsPage::isDirty() const
{
    return m_sessionTimeout && m_sessionTimeout->value() != m_loadedTimeout;
}

SettingsSaveOutcome AccountSettingsPage::save()
{
    const int minutes = m_sessionTimeout->value();
    const AuthResult result = AuthService::instance().setSessionTimeoutMinutes(minutes);
    if (!result.ok) {
        return {SettingsSaveStatus::Rejected, result.message};
    }
    m_loadedTimeout = minutes;
    emit dirtyChanged(false);
    refreshAudit();
    return {SettingsSaveStatus::Saved, result.message};
}

void AccountSettingsPage::revert()
{
    reloadFromSource();
}

void AccountSettingsPage::restoreDefaults()
{
    m_sessionTimeout->setValue(AuthService::kDefaultSessionTimeoutMinutes);
}

void AccountSettingsPage::setWriteAllowed(bool allowed)
{
    if (m_sessionTimeout) {
        m_sessionTimeout->setEnabled(allowed);
    }
}

QString AccountSettingsPage::pendingChangeSummary() const
{
    if (!isDirty()) {
        return QString();
    }
    return QStringLiteral("会话超时 %1 → %2 分钟").arg(m_loadedTimeout).arg(m_sessionTimeout->value());
}

void AccountSettingsPage::reloadFromSource()
{
    m_loadedTimeout = AuthService::instance().sessionTimeoutMinutes();
    {
        const QSignalBlocker blocker(m_sessionTimeout);
        m_sessionTimeout->setValue(m_loadedTimeout);
    }
    emit dirtyChanged(false);
    refreshIdentity();
    refreshUsers();
    refreshAudit();
}

void AccountSettingsPage::refreshIdentity()
{
    AuthService &auth = AuthService::instance();
    if (!auth.isAvailable()) {
        m_identity->setText(QStringLiteral("数据库未就绪，账户功能暂不可用。"));
    } else if (!auth.hasAccounts()) {
        m_identity->setText(QStringLiteral("系统中还没有账户。点「登录」创建第一个管理员。"));
    } else {
        m_identity->setText(QStringLiteral("当前身份：%1").arg(auth.identityText()));
    }
    const bool loggedIn = auth.isLoggedIn();
    QStringList scope;
    for (Permission permission : Auth::allPermissions()) {
        if (auth.can(permission)) {
            scope << Auth::permissionDisplayName(permission);
        }
    }
    m_identityScope->setText(loggedIn ? QStringLiteral("可执行：%1").arg(scope.join(QStringLiteral("；")))
                                      : QStringLiteral("未登录时所有受控设置为只读。"));
    m_loginButton->setVisible(!loggedIn);
    m_loginButton->setEnabled(auth.isAvailable());
    m_loginButton->setText(auth.isAvailable() && !auth.hasAccounts() ? QStringLiteral("创建管理员")
                                                                     : QStringLiteral("登录"));
    m_changePasswordButton->setVisible(loggedIn);
    m_logoutButton->setVisible(loggedIn);
}

void AccountSettingsPage::refreshUsers()
{
    AuthService &auth = AuthService::instance();
    const bool allowed = auth.can(Permission::ManageAccounts);
    m_usersLocked->setVisible(!allowed);
    m_users->setVisible(allowed);
    for (ElaPushButton *button :
         {m_createButton, m_resetButton, m_roleButton, m_enableButton, m_unlockButton}) {
        button->setVisible(allowed);
    }
    if (!allowed) {
        m_users->setRowCount(0);
        return;
    }

    const int previous = selectedUserId();
    QString error;
    const QVector<UserRecord> users = auth.listUsers(&error);
    m_users->setRowCount(users.size());
    const QDateTime now = QDateTime::currentDateTimeUtc();
    int reselect = -1;
    for (int row = 0; row < users.size(); ++row) {
        const UserRecord &user = users.at(row);
        const bool locked = user.lockedUntil.isValid() && user.lockedUntil > now;
        QStringList state;
        if (!user.enabled) {
            state << QStringLiteral("已停用");
        }
        if (locked) {
            state << QStringLiteral("锁定至 %1")
                         .arg(user.lockedUntil.toLocalTime().toString(QStringLiteral("HH:mm")));
        } else if (user.failedLoginCount > 0) {
            state << QStringLiteral("失败 %1 次").arg(user.failedLoginCount);
        }
        if (user.mustChangePassword) {
            state << QStringLiteral("待首次改密");
        }
        if (state.isEmpty()) {
            state << QStringLiteral("正常");
        }

        QString name = user.username;
        if (user.username.compare(auth.currentUsername(), Qt::CaseInsensitive) == 0) {
            name += QStringLiteral("（当前）");
        }
        auto *nameItem = readOnlyItem(name);
        nameItem->setData(kUserIdRole, user.id);
        nameItem->setData(kUserLockedRole, locked || user.failedLoginCount > 0);
        nameItem->setData(kUserEnabledRole, user.enabled);
        nameItem->setData(kUserRoleRole, static_cast<int>(user.role));
        m_users->setItem(row, 0, nameItem);
        m_users->setItem(row, 1, readOnlyItem(Auth::roleDisplayName(user.role)));
        m_users->setItem(row, 2, readOnlyItem(state.join(QStringLiteral("，"))));
        m_users->setItem(row, 3, readOnlyItem(localTime(user.lastLoginAt)));
        m_users->setItem(row, 4, readOnlyItem(user.createdBy.isEmpty() ? QStringLiteral("首次初始化")
                                                                       : user.createdBy));
        if (user.id == previous) {
            reselect = row;
        }
    }
    if (reselect >= 0) {
        m_users->selectRow(reselect);
    }
    if (!error.isEmpty()) {
        showResult(false, QStringLiteral("读取账户失败: %1").arg(error));
    }
    updateUserActions();
}

void AccountSettingsPage::refreshAudit()
{
    AuthService &auth = AuthService::instance();
    const bool allowed = auth.can(Permission::ViewAuditLog);
    m_auditLocked->setVisible(!allowed);
    m_audit->setVisible(allowed);
    m_auditCaption->setVisible(allowed);
    m_auditRefreshButton->setVisible(allowed);
    if (!allowed) {
        m_audit->setRowCount(0);
        return;
    }
    QString error;
    const QVector<AuditEntry> entries = auth.recentAudit(kAuditRows, &error);
    m_audit->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        const AuditEntry &entry = entries.at(row);
        m_audit->setItem(row, 0, readOnlyItem(localTime(entry.occurredAt)));
        m_audit->setItem(row, 1, readOnlyItem(entry.username.isEmpty() ? QStringLiteral("—")
                                                                       : entry.username));
        m_audit->setItem(row, 2, readOnlyItem(entry.role == Role::None
                                                  ? QStringLiteral("—")
                                                  : Auth::roleDisplayName(entry.role)));
        m_audit->setItem(row, 3, readOnlyItem(actionDisplayName(entry.action)));
        m_audit->setItem(row, 4, readOnlyItem(entry.target));
        auto *result = readOnlyItem(entry.success ? QStringLiteral("成功") : QStringLiteral("失败"));
        result->setForeground(QColor(entry.success ? Theme::kStateSuccess : Theme::kStateError));
        m_audit->setItem(row, 5, result);
        m_audit->setItem(row, 6, readOnlyItem(entry.detail));
    }
    if (!error.isEmpty()) {
        showResult(false, QStringLiteral("读取审计失败: %1").arg(error));
    }
}

void AccountSettingsPage::updateUserActions()
{
    const int userId = selectedUserId();
    const bool hasSelection = userId > 0;
    QTableWidgetItem *item = hasSelection ? m_users->item(m_users->currentRow(), 0) : nullptr;
    const bool isSelf = item
                        && selectedUsername().compare(AuthService::instance().currentUsername(),
                                                      Qt::CaseInsensitive)
                               == 0;
    const bool enabled = item && item->data(kUserEnabledRole).toBool();
    const bool locked = item && item->data(kUserLockedRole).toBool();

    m_resetButton->setEnabled(hasSelection && !isSelf);
    m_roleButton->setEnabled(hasSelection && !isSelf);
    m_enableButton->setEnabled(hasSelection && !isSelf);
    m_enableButton->setText(item && !enabled ? QStringLiteral("启用") : QStringLiteral("停用"));
    m_unlockButton->setEnabled(hasSelection && locked);
}

int AccountSettingsPage::selectedUserId() const
{
    const QList<QTableWidgetItem *> selected = m_users->selectedItems();
    if (selected.isEmpty()) {
        return 0;
    }
    QTableWidgetItem *item = m_users->item(selected.first()->row(), 0);
    return item ? item->data(kUserIdRole).toInt() : 0;
}

QString AccountSettingsPage::selectedUsername() const
{
    const QList<QTableWidgetItem *> selected = m_users->selectedItems();
    if (selected.isEmpty()) {
        return QString();
    }
    QTableWidgetItem *item = m_users->item(selected.first()->row(), 0);
    if (!item) {
        return QString();
    }
    QString name = item->text();
    name.remove(QStringLiteral("（当前）"));
    return name;
}

void AccountSettingsPage::showResult(bool ok, const QString &message)
{
    if (ok) {
        ElaMessageBar::success(ElaMessageBarType::BottomRight, QStringLiteral("账户"), message, 2600, this);
    } else {
        ElaMessageBar::error(ElaMessageBarType::BottomRight, QStringLiteral("账户"), message, 3600, this);
    }
}

void AccountSettingsPage::createUser()
{
    FormDialog form(this,
                    QStringLiteral("新建账户"),
                    QStringLiteral("新账户首次登录时必须修改密码。请当面告知初始密码，不要写在设备上。"),
                    QStringLiteral("创建"));
    QLineEdit *username = form.addLine(QStringLiteral("用户名"), false);
    form.addHint(PasswordPolicy::usernameRequirementText());
    QComboBox *role = form.addRoleCombo(QStringLiteral("角色"), Role::Operator);
    QLineEdit *password = form.addLine(QStringLiteral("初始密码"), true);
    QLineEdit *confirm = form.addLine(QStringLiteral("再次输入初始密码"), true);
    form.addHint(PasswordPolicy::passwordRequirementText());

    QString message;
    const bool created = form.exec([&]() {
        if (password->text() != confirm->text()) {
            form.setError(QStringLiteral("两次输入的密码不一致。"));
            confirm->clear();
            return false;
        }
        const AuthResult result = AuthService::instance().createUser(
            username->text(), static_cast<Role>(role->currentData().toInt()), password->text());
        if (!result.ok) {
            form.setError(result.message);
            return false;
        }
        message = result.message;
        return true;
    });
    if (created) {
        showResult(true, message);
    }
}

void AccountSettingsPage::resetPassword()
{
    const int userId = selectedUserId();
    const QString name = selectedUsername();
    if (userId <= 0) {
        return;
    }
    FormDialog form(this,
                    QStringLiteral("重置密码：%1").arg(name),
                    QStringLiteral("设置一个临时密码。对方下次登录时必须改成自己的密码。"),
                    QStringLiteral("重置"));
    QLineEdit *password = form.addLine(QStringLiteral("临时密码"), true);
    QLineEdit *confirm = form.addLine(QStringLiteral("再次输入临时密码"), true);
    form.addHint(PasswordPolicy::passwordRequirementText());

    QString message;
    const bool done = form.exec([&]() {
        if (password->text() != confirm->text()) {
            form.setError(QStringLiteral("两次输入的密码不一致。"));
            confirm->clear();
            return false;
        }
        const AuthResult result = AuthService::instance().resetPassword(userId, password->text());
        if (!result.ok) {
            form.setError(result.message);
            return false;
        }
        message = result.message;
        return true;
    });
    if (done) {
        showResult(true, message);
    }
}

void AccountSettingsPage::changeRole()
{
    const int userId = selectedUserId();
    if (userId <= 0) {
        return;
    }
    QTableWidgetItem *item = m_users->item(m_users->currentRow(), 0);
    const Role current = item ? static_cast<Role>(item->data(kUserRoleRole).toInt()) : Role::Operator;
    const QString name = selectedUsername();
    FormDialog form(this,
                    QStringLiteral("修改角色：%1").arg(name),
                    QStringLiteral("角色变更在对方下次登录时生效。"),
                    QStringLiteral("保存"));
    QComboBox *role = form.addRoleCombo(QStringLiteral("角色"), current);

    QString message;
    const bool done = form.exec([&]() {
        const AuthResult result =
            AuthService::instance().setRole(userId, static_cast<Role>(role->currentData().toInt()));
        if (!result.ok) {
            form.setError(result.message);
            return false;
        }
        message = result.message;
        return true;
    });
    if (done) {
        showResult(true, message);
    }
}

void AccountSettingsPage::toggleEnabled()
{
    const int userId = selectedUserId();
    QTableWidgetItem *item = userId > 0 ? m_users->item(m_users->currentRow(), 0) : nullptr;
    if (!item) {
        return;
    }
    const bool enable = !item->data(kUserEnabledRole).toBool();
    const QString name = selectedUsername();
    const QString question = enable
                                 ? QStringLiteral("确定要启用账户 %1 吗？").arg(name)
                                 : QStringLiteral("确定要停用账户 %1 吗？停用后该账户无法登录，审计记录保留。")
                                       .arg(name);
    if (!confirmAction(this, enable ? QStringLiteral("启用账户") : QStringLiteral("停用账户"), question)) {
        return;
    }
    const AuthResult result = AuthService::instance().setEnabled(userId, enable);
    showResult(result.ok, result.message);
}

void AccountSettingsPage::unlockUser()
{
    const int userId = selectedUserId();
    if (userId <= 0) {
        return;
    }
    const AuthResult result = AuthService::instance().unlock(userId);
    showResult(result.ok, result.message);
}
