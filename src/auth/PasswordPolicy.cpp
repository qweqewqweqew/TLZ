#include "auth/PasswordPolicy.h"

#include <QStringList>

namespace PasswordPolicy {

namespace {

bool isAllowedUsernameChar(QChar ch)
{
    return ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('-')
           || ch == QLatin1Char('.');
}

bool isCommonPassword(const QString &password)
{
    static const QStringList kCommon = {
        QStringLiteral("password1"),   QStringLiteral("password12"),
        QStringLiteral("password123"), QStringLiteral("passw0rd"),
        QStringLiteral("p@ssw0rd"),    QStringLiteral("admin123"),
        QStringLiteral("admin1234"),   QStringLiteral("admin12345"),
        QStringLiteral("abc12345"),    QStringLiteral("abcd1234"),
        QStringLiteral("a1234567"),    QStringLiteral("a12345678"),
        QStringLiteral("12345678a"),   QStringLiteral("1234567a"),
        QStringLiteral("qwer1234"),    QStringLiteral("qwerty123"),
        QStringLiteral("qwerty12"),    QStringLiteral("1qaz2wsx"),
        QStringLiteral("asdf1234"),    QStringLiteral("zxcv1234"),
        QStringLiteral("1234qwer"),    QStringLiteral("88888888a"),
        QStringLiteral("11111111a"),   QStringLiteral("iloveyou1"),
        QStringLiteral("welcome1"),    QStringLiteral("test1234"),
    };
    return kCommon.contains(password.toLower());
}

} // namespace

QString validateUsername(const QString &username)
{
    if (username.isEmpty()) {
        return QStringLiteral("用户名不能为空。");
    }
    if (username != username.trimmed()) {
        return QStringLiteral("用户名首尾不能有空格。");
    }
    if (username.size() < kMinUsernameLength || username.size() > kMaxUsernameLength) {
        return QStringLiteral("用户名长度须为 %1~%2 个字符。")
            .arg(kMinUsernameLength)
            .arg(kMaxUsernameLength);
    }
    for (const QChar ch : username) {
        if (!isAllowedUsernameChar(ch)) {
            return QStringLiteral("用户名只能包含字母、数字、汉字和 _ - . 符号。");
        }
    }
    return QString();
}

QString validatePassword(const QString &username, const QString &password)
{
    if (password.size() < kMinPasswordLength) {
        return QStringLiteral("密码至少 %1 位。").arg(kMinPasswordLength);
    }
    if (password.size() > kMaxPasswordLength) {
        return QStringLiteral("密码不能超过 %1 位。").arg(kMaxPasswordLength);
    }
    if (password != password.trimmed()) {
        return QStringLiteral("密码首尾不能有空格。");
    }

    bool hasLetter = false;
    bool hasDigit = false;
    for (const QChar ch : password) {
        if (ch.isLetter()) {
            hasLetter = true;
        } else if (ch.isDigit()) {
            hasDigit = true;
        }
    }
    if (!hasLetter || !hasDigit) {
        return QStringLiteral("密码必须同时包含字母和数字。");
    }

    if (password.count(password.at(0)) == password.size()) {
        return QStringLiteral("密码不能由同一个字符重复组成。");
    }

    const QString name = username.trimmed();
    if (!name.isEmpty() && password.contains(name, Qt::CaseInsensitive)) {
        return QStringLiteral("密码不能包含用户名。");
    }

    if (isCommonPassword(password)) {
        return QStringLiteral("该密码过于常见，请换一个。");
    }
    return QString();
}

QString usernameRequirementText()
{
    return QStringLiteral("用户名 %1~%2 个字符，可用字母、数字、汉字和 _ - .，不区分大小写。")
        .arg(kMinUsernameLength)
        .arg(kMaxUsernameLength);
}

QString passwordRequirementText()
{
    return QStringLiteral("密码 %1~%2 位，必须同时包含字母和数字，不能包含用户名，不能使用常见弱密码。")
        .arg(kMinPasswordLength)
        .arg(kMaxPasswordLength);
}

} // namespace PasswordPolicy
