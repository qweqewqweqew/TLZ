#ifndef PASSWORDHASHER_H
#define PASSWORDHASHER_H

#include <QByteArray>
#include <QString>

// PBKDF2-HMAC-SHA256。只依赖 QtCore，不引入 QtNetwork 的 QPasswordDigestor。
namespace PasswordHasher {

constexpr int kDefaultIterations = 120000;
constexpr int kSaltBytes = 16;
constexpr int kDigestBytes = 32;

struct Digest
{
    QByteArray salt;
    QByteArray hash;
    int iterations{0};
};

Digest hashPassword(const QString &password, int iterations = kDefaultIterations);
bool verifyPassword(const QString &password, const Digest &stored);

QByteArray pbkdf2HmacSha256(const QByteArray &password,
                            const QByteArray &salt,
                            int iterations,
                            int keyLength);
bool constantTimeEquals(const QByteArray &a, const QByteArray &b);

} // namespace PasswordHasher

#endif // PASSWORDHASHER_H
