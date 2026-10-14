#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>

class ElaPushButton;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class QWidget;

// 登录、现场注册、首次创建管理员、强制改密、主动改密共用一个对话框。
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Login,
        Bootstrap,
        ChangePassword,
    };

    explicit LoginDialog(Mode mode, QWidget *parent = nullptr);
    ~LoginDialog() override;

    // 数据库里还没有账户时自动转为「创建管理员」。返回 true 表示已登录。
    static bool runLogin(QWidget *parent);
    static bool runChangePassword(QWidget *parent);

private:
    enum class Step {
        Login,
        Register,
        ForcedChange,
        Bootstrap,
        ChangeOwn,
    };

    QWidget *buildHeader();
    QWidget *buildModeSwitch();
    QWidget *buildLoginPage();
    QWidget *buildRegisterPage();
    QWidget *buildForcedChangePage();
    QWidget *buildBootstrapPage();
    QWidget *buildChangeOwnPage();
    QWidget *newPage(QVBoxLayout **layout);
    QLineEdit *addField(QVBoxLayout *layout, const QString &label, const QString &placeholder, bool password);
    void addSectionLabel(QVBoxLayout *layout, const QString &text);
    void showStep(Step step);
    void submit();
    void setMessage(const QString &text, bool ok = false);
    void clearSecrets();

    Step m_step{Step::Login};
    QStackedWidget *m_pages{nullptr};
    QWidget *m_modeSwitch{nullptr};
    QPushButton *m_loginTab{nullptr};
    QPushButton *m_registerTab{nullptr};
    QLabel *m_title{nullptr};
    QLabel *m_subtitle{nullptr};
    QLabel *m_message{nullptr};
    ElaPushButton *m_submit{nullptr};

    QWidget *m_loginPage{nullptr};
    QWidget *m_registerPage{nullptr};
    QWidget *m_forcedPage{nullptr};
    QWidget *m_bootstrapPage{nullptr};
    QWidget *m_changeOwnPage{nullptr};

    QLineEdit *m_loginUser{nullptr};
    QLineEdit *m_loginPassword{nullptr};
    QCheckBox *m_showPassword{nullptr};
    QLineEdit *m_regUser{nullptr};
    QComboBox *m_regRole{nullptr};
    QLineEdit *m_regPassword{nullptr};
    QLineEdit *m_regConfirm{nullptr};
    QLineEdit *m_regAdminUser{nullptr};
    QLineEdit *m_regAdminPassword{nullptr};
    QLineEdit *m_forcedNew{nullptr};
    QLineEdit *m_forcedConfirm{nullptr};
    QLineEdit *m_bootUser{nullptr};
    QLineEdit *m_bootPassword{nullptr};
    QLineEdit *m_bootConfirm{nullptr};
    QLineEdit *m_ownCurrent{nullptr};
    QLineEdit *m_ownNew{nullptr};
    QLineEdit *m_ownConfirm{nullptr};

    QString m_pendingUsername;
    QString m_pendingPassword;
};

#endif // LOGINDIALOG_H
