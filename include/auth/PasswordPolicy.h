#ifndef PASSWORDPOLICY_H
#define PASSWORDPOLICY_H

#include <QString>

// 返回空串表示通过；否则返回给用户看的原因。
namespace PasswordPolicy {

constexpr int kMinUsernameLength = 3;
constexpr int kMaxUsernameLength = 32;
constexpr int kMinPasswordLength = 8;
constexpr int kMaxPasswordLength = 64;

QString validateUsername(const QString &username);
QString validatePassword(const QString &username, const QString &password);
QString usernameRequirementText();
QString passwordRequirementText();

} // namespace PasswordPolicy

#endif // PASSWORDPOLICY_H
