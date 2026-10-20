#include "ui/dialogs/LoginDialog.h"

#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "auth/AuthService.h"
#include "auth/PasswordPolicy.h"
#include "ui/settings/SettingsFormWidgets.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

constexpr int kDialogWidth = 500;

QString cardStyle()
{
    return QStringLiteral(
               "QFrame#loginCard { background:%1; border:1px solid %2; border-radius:12px; }"
               "QFrame#loginHeader { background:%3; border:none; border-bottom:1px solid %4;"
               "  border-top-left-radius:12px; border-top-right-radius:12px; }"
               "QLineEdit#loginField {"
               "  background:%5; color:%6; border:1px solid %2; border-radius:8px;"
               "  padding:0 12px; min-height:40px; font-size:15px; selection-background-color:%7;"
               "}"
               "QLineEdit#loginField:focus { border:1px solid %8; }"
               "QComboBox#loginField {"
               "  background:%5; color:%6; border:1px solid %2; border-radius:8px;"
               "  padding:0 12px; min-height:40px; font-size:15px;"
               "}"
               "QComboBox#loginField::drop-down { border:none; width:32px; }"
               "QComboBox#loginField::down-arrow { image:url(:/img/chevron_down.svg);"
               "  width:12px; height:12px; margin-right:12px; }"
               "QComboBox#loginField QAbstractItemView { background:%5; color:%6;"
               "  selection-background-color:%7; }"
               "QPushButton#loginModeTab {"
               "  background:transparent; color:%9; border:none; border-radius:6px;"
               "  font-size:15px; min-height:34px;"
               "}"
               "QPushButton#loginModeTab:checked { background:%8; color:#FFFFFF; }"
               "QPushButton#loginModeTab:hover:!checked { background:%10; }"
               "QPushButton#loginClose {"
               "  background:transparent; color:%9; border:none; border-radius:6px; font-size:20px;"
               "}"
               "QPushButton#loginClose:hover { background:%11; color:#FFFFFF; }")
        .arg(Theme::kPanelBg,
             Theme::kBorder,
             Theme::kStatusBarBg,
             Theme::kDivider,
             Theme::kControlBg,
             Theme::kTextPrimary,
             Theme::kSelectionBg,
             Theme::kAccent,
             Theme::kTextSecondary)
        .arg(Theme::kHoverBg, Theme::kStateError);
}

} // namespace

LoginDialog::LoginDialog(Mode mode, QWidget *parent)
    : QDialog(parent)
{
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("loginDialog"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSizeConstraint(QLayout::SetFixedSize);
    auto *card = new QFrame(this);
    card->setFixedWidth(kDialogWidth);
    card->setObjectName(QStringLiteral("loginCard"));
    card->setStyleSheet(cardStyle());
    outer->addWidget(card);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);
    cardLayout->addWidget(buildHeader());

    auto *body = new QWidget(card);
    auto *root = new QVBoxLayout(body);
    root->setContentsMargins(32, 20, 32, 24);
    root->setSpacing(10);
    cardLayout->addWidget(body);

    m_modeSwitch = buildModeSwitch();
    root->addWidget(m_modeSwitch);

    m_title = makeLabel(QString(), QStringLiteral("panelTitle"));
    m_title->setParent(body);
    m_title->setStyleSheet(QStringLiteral("color:%1;font-size:20px;font-weight:600;"
                                          "border:none;background:transparent;")
                               .arg(Theme::kTextPrimary));
    root->addWidget(m_title);
    m_subtitle = settingsHint(QString(), body);
    root->addWidget(m_subtitle);

    m_pages = new QStackedWidget(body);
    m_loginPage = buildLoginPage();
    m_registerPage = buildRegisterPage();
    m_forcedPage = buildForcedChangePage();
    m_bootstrapPage = buildBootstrapPage();
    m_changeOwnPage = buildChangeOwnPage();
    for (QWidget *page : {m_loginPage, m_registerPage, m_forcedPage, m_bootstrapPage, m_changeOwnPage}) {
        m_pages->addWidget(page);
    }
    root->addWidget(m_pages);

    m_message = settingsHint(QString(), body);
    m_message->hide();
    root->addWidget(m_message);

    m_submit = new ElaPushButton(QString(), body);
    m_submit->setMinimumHeight(44);
    applyPrimaryButtonStyle(m_submit);
    root->addSpacing(4);
    root->addWidget(m_submit);

    connect(m_submit, &ElaPushButton::clicked, this, &LoginDialog::submit);
    connect(this, &QDialog::finished, this, [this](int) { clearSecrets(); });

    switch (mode) {
    case Mode::Login:
        showStep(Step::Login);
        break;
    case Mode::Bootstrap:
        showStep(Step::Bootstrap);
        break;
    case Mode::ChangePassword:
        showStep(Step::ChangeOwn);
        break;
    }
}

LoginDialog::~LoginDialog()
{
    clearSecrets();
}

bool LoginDialog::runLogin(QWidget *parent)
{
    AuthService &auth = AuthService::instance();
    if (!auth.isAvailable()) {
        ElaMessageBar::warning(ElaMessageBarType::BottomRight,
                               QStringLiteral("暂时无法登录"),
                               QStringLiteral("数据库尚未就绪，账户功能不可用。请稍后重试。"),
                               3600,
                               parent);
        return false;
    }
    LoginDialog dialog(auth.hasAccounts() ? Mode::Login : Mode::Bootstrap, parent);
    return dialog.exec() == QDialog::Accepted;
}

bool LoginDialog::runChangePassword(QWidget *parent)
{
    if (!AuthService::instance().isLoggedIn()) {
        return false;
    }
    LoginDialog dialog(Mode::ChangePassword, parent);
    return dialog.exec() == QDialog::Accepted;
}

QWidget *LoginDialog::buildHeader()
{
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("loginHeader"));
    header->setFixedHeight(72);
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(24, 0, 12, 0);
    layout->setSpacing(12);

    auto *logo = new QLabel(header);
    logo->setFixedSize(40, 40);
    logo->setPixmap(QPixmap(QStringLiteral(":/img/logo.png"))
                        .scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    layout->addWidget(logo);

    auto *texts = new QVBoxLayout();
    texts->setContentsMargins(0, 0, 0, 0);
    texts->setSpacing(2);
    auto *system = makeLabel(QStringLiteral("阴极铜板粒子检测及修磨系统"));
    system->setParent(header);
    system->setStyleSheet(QStringLiteral("color:%1;font-size:16px;font-weight:600;"
                                         "border:none;background:transparent;")
                              .arg(Theme::kTextPrimary));
    auto *caption = makeLabel(QStringLiteral("账户与权限"));
    caption->setParent(header);
    caption->setStyleSheet(QStringLiteral("color:%1;font-size:12px;border:none;background:transparent;")
                               .arg(Theme::kTextMuted));
    texts->addWidget(system);
    texts->addWidget(caption);
    layout->addLayout(texts, 1);

    auto *close = new QPushButton(QStringLiteral("×"), header);
    close->setObjectName(QStringLiteral("loginClose"));
    close->setFixedSize(36, 36);
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(QStringLiteral("关闭（Esc）"));
    close->setFocusPolicy(Qt::NoFocus);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(close, 0, Qt::AlignVCenter);
    return header;
}

QWidget *LoginDialog::buildModeSwitch()
{
    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("loginModeSwitch"));
    frame->setStyleSheet(QStringLiteral(
                             "QFrame#loginModeSwitch { background:%1; border:1px solid %2; border-radius:8px; }")
                             .arg(Theme::kControlBg, Theme::kBorder));
    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    m_loginTab = new QPushButton(QStringLiteral("登录"), frame);
    m_registerTab = new QPushButton(QStringLiteral("注册"), frame);
    auto *group = new QButtonGroup(frame);
    for (QPushButton *tab : {m_loginTab, m_registerTab}) {
        tab->setObjectName(QStringLiteral("loginModeTab"));
        tab->setCheckable(true);
        tab->setCursor(Qt::PointingHandCursor);
        tab->setFocusPolicy(Qt::NoFocus);
        group->addButton(tab);
        layout->addWidget(tab, 1);
    }
    connect(m_loginTab, &QPushButton::clicked, this, [this]() { showStep(Step::Login); });
    connect(m_registerTab, &QPushButton::clicked, this, [this]() { showStep(Step::Register); });
    return frame;
}

QWidget *LoginDialog::newPage(QVBoxLayout **layout)
{
    auto *page = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 4, 0, 0);
    pageLayout->setSpacing(6);
    *layout = pageLayout;
    return page;
}

QLineEdit *LoginDialog::addField(QVBoxLayout *layout,
                                 const QString &label,
                                 const QString &placeholder,
                                 bool password)
{
    QWidget *page = layout->parentWidget();
    layout->addWidget(settingsFieldLabel(label, page));
    auto *edit = new QLineEdit(page);
    edit->setObjectName(QStringLiteral("loginField"));
    edit->setPlaceholderText(placeholder);
    edit->setMaxLength(password ? PasswordPolicy::kMaxPasswordLength
                                : PasswordPolicy::kMaxUsernameLength);
    if (password) {
        edit->setEchoMode(QLineEdit::Password);
        edit->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhNoPredictiveText
                                  | Qt::ImhSensitiveData | Qt::ImhNoAutoUppercase);
    }
    layout->addWidget(edit);
    layout->addSpacing(4);
    connect(edit, &QLineEdit::returnPressed, this, &LoginDialog::submit);
    return edit;
}

void LoginDialog::addSectionLabel(QVBoxLayout *layout, const QString &text)
{
    QWidget *page = layout->parentWidget();
    layout->addSpacing(6);
    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    auto *label = makeLabel(text);
    label->setParent(page);
    label->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:600;"
                                        "border:none;background:transparent;")
                             .arg(Theme::kAccent));
    auto *line = new QFrame(page);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background:%1;border:none;").arg(Theme::kDivider));
    row->addWidget(label);
    row->addWidget(line, 1, Qt::AlignVCenter);
    layout->addLayout(row);
}

QWidget *LoginDialog::buildLoginPage()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = newPage(&layout);
    m_loginUser = addField(layout, QStringLiteral("用户名"), QStringLiteral("请输入用户名"), false);
    m_loginPassword = addField(layout, QStringLiteral("密码"), QStringLiteral("请输入密码"), true);
    m_showPassword = settingsCheckBox(QStringLiteral("显示密码"), page);
    connect(m_showPassword, &QCheckBox::toggled, this, [this](bool show) {
        m_loginPassword->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    });
    layout->addWidget(m_showPassword);
    layout->addWidget(settingsHint(
        QStringLiteral("连续输错 %1 次锁定 %2 分钟。监视画面、启停节点不需要登录。")
            .arg(AuthService::kMaxFailedAttempts)
            .arg(AuthService::kLockoutMinutes),
        page));
    return page;
}

QWidget *LoginDialog::buildRegisterPage()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = newPage(&layout);
    addSectionLabel(layout, QStringLiteral("新账户"));
    m_regUser = addField(layout, QStringLiteral("用户名"), QStringLiteral("3~32 位，字母 / 数字 / 汉字"), false);
    layout->addWidget(settingsFieldLabel(QStringLiteral("角色"), page));
    m_regRole = new QComboBox(page);
    m_regRole->setObjectName(QStringLiteral("loginField"));
    m_regRole->addItem(Auth::roleDisplayName(Role::Operator), static_cast<int>(Role::Operator));
    m_regRole->addItem(Auth::roleDisplayName(Role::Technician), static_cast<int>(Role::Technician));
    layout->addWidget(m_regRole);
    layout->addSpacing(4);
    m_regPassword = addField(layout, QStringLiteral("密码"), QStringLiteral("8~64 位，含字母和数字"), true);
    m_regConfirm = addField(layout, QStringLiteral("确认密码"), QStringLiteral("再次输入密码"), true);

    addSectionLabel(layout, QStringLiteral("管理员授权"));
    layout->addWidget(settingsHint(
        QStringLiteral("请管理员在现场输入自己的账号和密码确认。授权不会切换当前登录状态。"),
        page));
    m_regAdminUser = addField(layout, QStringLiteral("管理员账号"), QStringLiteral("管理员用户名"), false);
    m_regAdminPassword = addField(layout, QStringLiteral("管理员密码"), QStringLiteral("管理员密码"), true);
    return page;
}

QWidget *LoginDialog::buildForcedChangePage()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = newPage(&layout);
    m_forcedNew = addField(layout, QStringLiteral("新密码"), QStringLiteral("8~64 位，含字母和数字"), true);
    m_forcedConfirm = addField(layout, QStringLiteral("确认新密码"), QStringLiteral("再次输入新密码"), true);
    layout->addWidget(settingsHint(PasswordPolicy::passwordRequirementText(), page));
    return page;
}

QWidget *LoginDialog::buildBootstrapPage()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = newPage(&layout);
    m_bootUser = addField(layout, QStringLiteral("管理员用户名"), QStringLiteral("3~32 位，字母 / 数字 / 汉字"), false);
    m_bootPassword = addField(layout, QStringLiteral("密码"), QStringLiteral("8~64 位，含字母和数字"), true);
    m_bootConfirm = addField(layout, QStringLiteral("确认密码"), QStringLiteral("再次输入密码"), true);
    layout->addWidget(settingsHint(PasswordPolicy::passwordRequirementText(), page));
    return page;
}

QWidget *LoginDialog::buildChangeOwnPage()
{
    QVBoxLayout *layout = nullptr;
    QWidget *page = newPage(&layout);
    m_ownCurrent = addField(layout, QStringLiteral("当前密码"), QStringLiteral("请输入当前密码"), true);
    m_ownNew = addField(layout, QStringLiteral("新密码"), QStringLiteral("8~64 位，含字母和数字"), true);
    m_ownConfirm = addField(layout, QStringLiteral("确认新密码"), QStringLiteral("再次输入新密码"), true);
    layout->addWidget(settingsHint(PasswordPolicy::passwordRequirementText(), page));
    return page;
}

void LoginDialog::showStep(Step step)
{
    m_step = step;
    setMessage(QString());
    const bool switchable = step == Step::Login || step == Step::Register;
    m_modeSwitch->setVisible(switchable);
    m_loginTab->setChecked(step == Step::Login);
    m_registerTab->setChecked(step == Step::Register);

    QWidget *page = nullptr;
    QLineEdit *focus = nullptr;
    switch (step) {
    case Step::Login:
        setWindowTitle(QStringLiteral("登录"));
        m_title->setText(QStringLiteral("欢迎登录"));
        m_subtitle->setText(QStringLiteral("登录后按角色解锁对应的设置项。"));
        m_submit->setText(QStringLiteral("登 录"));
        page = m_loginPage;
        focus = m_loginUser->text().isEmpty() ? m_loginUser : m_loginPassword;
        break;
    case Step::Register:
        setWindowTitle(QStringLiteral("注册"));
        m_title->setText(QStringLiteral("注册新账户"));
        m_subtitle->setText(QStringLiteral("只能注册操作员或技术员，需要管理员现场授权。"));
        m_submit->setText(QStringLiteral("注 册"));
        page = m_registerPage;
        focus = m_regUser;
        break;
    case Step::ForcedChange:
        setWindowTitle(QStringLiteral("设置新密码"));
        m_title->setText(QStringLiteral("设置新密码"));
        m_subtitle->setText(QStringLiteral("账户 %1 首次登录或密码已被管理员重置，必须先设置新密码。")
                                .arg(m_pendingUsername));
        m_submit->setText(QStringLiteral("保存并登录"));
        page = m_forcedPage;
        focus = m_forcedNew;
        break;
    case Step::Bootstrap:
        setWindowTitle(QStringLiteral("创建管理员"));
        m_title->setText(QStringLiteral("首次使用：创建管理员"));
        m_subtitle->setText(QStringLiteral(
            "系统中还没有任何账户。请创建唯一的管理员，之后其他账户都由管理员创建或授权注册。"));
        m_submit->setText(QStringLiteral("创建并登录"));
        page = m_bootstrapPage;
        focus = m_bootUser;
        break;
    case Step::ChangeOwn:
        setWindowTitle(QStringLiteral("修改密码"));
        m_title->setText(QStringLiteral("修改密码"));
        m_subtitle->setText(QStringLiteral("当前账户：%1").arg(AuthService::instance().identityText()));
        m_submit->setText(QStringLiteral("保 存"));
        page = m_changeOwnPage;
        focus = m_ownCurrent;
        break;
    }
    // 栈默认按最高的页（注册页）撑高，登录页的字段之间会被拉开；按当前页固定高度。
    m_pages->setCurrentWidget(page);
    page->layout()->activate();
    m_pages->setFixedHeight(page->sizeHint().height());
    if (focus) {
        focus->setFocus();
    }
}

void LoginDialog::submit()
{
    AuthService &auth = AuthService::instance();
    switch (m_step) {
    case Step::Login: {
        const QString username = m_loginUser->text().trimmed();
        const QString password = m_loginPassword->text();
        const LoginResult result = auth.login(username, password);
        m_loginPassword->clear();
        if (result.status == LoginStatus::Success) {
            accept();
            return;
        }
        if (result.status == LoginStatus::PasswordChangeRequired) {
            m_pendingUsername = username;
            m_pendingPassword = password;
            showStep(Step::ForcedChange);
            m_subtitle->setText(result.message);
            return;
        }
        setMessage(result.message);
        m_loginPassword->setFocus();
        return;
    }
    case Step::Register: {
        if (m_regPassword->text() != m_regConfirm->text()) {
            setMessage(QStringLiteral("两次输入的密码不一致。"));
            m_regConfirm->clear();
            m_regConfirm->setFocus();
            return;
        }
        const QString username = m_regUser->text().trimmed();
        const AuthResult result = auth.registerWithApproval(m_regAdminUser->text(),
                                                            m_regAdminPassword->text(),
                                                            username,
                                                            static_cast<Role>(m_regRole->currentData().toInt()),
                                                            m_regPassword->text());
        m_regAdminPassword->clear();
        if (!result.ok) {
            setMessage(result.message);
            return;
        }
        for (QLineEdit *edit : {m_regUser, m_regPassword, m_regConfirm, m_regAdminUser}) {
            edit->clear();
        }
        m_loginUser->setText(username);
        showStep(Step::Login);
        setMessage(result.message, true);
        return;
    }
    case Step::ForcedChange: {
        if (m_forcedNew->text() != m_forcedConfirm->text()) {
            setMessage(QStringLiteral("两次输入的新密码不一致。"));
            m_forcedConfirm->clear();
            m_forcedConfirm->setFocus();
            return;
        }
        const AuthResult result =
            auth.completeRequiredPasswordChange(m_pendingUsername, m_pendingPassword, m_forcedNew->text());
        if (!result.ok) {
            setMessage(result.message);
            m_forcedNew->clear();
            m_forcedConfirm->clear();
            m_forcedNew->setFocus();
            return;
        }
        accept();
        return;
    }
    case Step::Bootstrap: {
        if (m_bootPassword->text() != m_bootConfirm->text()) {
            setMessage(QStringLiteral("两次输入的密码不一致。"));
            m_bootConfirm->clear();
            m_bootConfirm->setFocus();
            return;
        }
        const AuthResult result =
            auth.createInitialAdministrator(m_bootUser->text(), m_bootPassword->text());
        if (!result.ok) {
            setMessage(result.message);
            m_bootPassword->clear();
            m_bootConfirm->clear();
            m_bootPassword->setFocus();
            return;
        }
        accept();
        return;
    }
    case Step::ChangeOwn: {
        if (m_ownNew->text() != m_ownConfirm->text()) {
            setMessage(QStringLiteral("两次输入的新密码不一致。"));
            m_ownConfirm->clear();
            m_ownConfirm->setFocus();
            return;
        }
        const AuthResult result = auth.changeOwnPassword(m_ownCurrent->text(), m_ownNew->text());
        m_ownCurrent->clear();
        if (!result.ok) {
            setMessage(result.message);
            m_ownNew->clear();
            m_ownConfirm->clear();
            if (!auth.isLoggedIn()) {
                ElaMessageBar::error(ElaMessageBarType::BottomRight,
                                     QStringLiteral("已退出登录"),
                                     result.message,
                                     4000,
                                     parentWidget());
                reject();
                return;
            }
            m_ownCurrent->setFocus();
            return;
        }
        accept();
        return;
    }
    }
}

void LoginDialog::setMessage(const QString &text, bool ok)
{
    m_message->setStyleSheet(QStringLiteral(
                                 "color:%1;font-size:13px;border:none;border-radius:6px;"
                                 "padding:6px 10px;background:%2;")
                                 .arg(ok ? Theme::kStateSuccess : Theme::kStateError,
                                      ok ? Theme::kSuccessBg : Theme::kErrorBg));
    m_message->setText(text);
    m_message->setVisible(!text.isEmpty());
}

void LoginDialog::clearSecrets()
{
    m_pendingPassword.fill(QLatin1Char('\0'));
    m_pendingPassword.clear();
    for (QLineEdit *edit : {m_loginPassword, m_regPassword, m_regConfirm, m_regAdminPassword,
                            m_forcedNew, m_forcedConfirm, m_bootPassword, m_bootConfirm,
                            m_ownCurrent, m_ownNew, m_ownConfirm}) {
        if (edit) {
            edit->clear();
        }
    }
}
