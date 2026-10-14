#include "auth/PasswordHasher.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

namespace PasswordHasher {

namespace {

constexpr int kHmacBlockSize = 64;

QByteArray randomBytes(int count)
{
    QByteArray bytes(count, Qt::Uninitialized);
    QRandomGenerator *generator = QRandomGenerator::system();
    for (int i = 0; i < count; ++i) {
        bytes[i] = static_cast<char>(generator->bounded(256));
    }
    return bytes;
}

} // namespace

QByteArray pbkdf2HmacSha256(const QByteArray &password,
                            const QByteArray &salt,
                            int iterations,
                            int keyLength)
{
    if (iterations < 1 || keyLength < 1) {
        return QByteArray();
    }

    QByteArray key = password;
    if (key.size() > kHmacBlockSize) {
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    }
    key.append(QByteArray(kHmacBlockSize - key.size(), '\0'));

    QByteArray innerPad(kHmacBlockSize, '\0');
    QByteArray outerPad(kHmacBlockSize, '\0');
    for (int i = 0; i < kHmacBlockSize; ++i) {
        innerPad[i] = static_cast<char>(key.at(i) ^ 0x36);
        outerPad[i] = static_cast<char>(key.at(i) ^ 0x5c);
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto hmac = [&](const QByteArray &message) {
        hash.reset();
        hash.addData(innerPad);
        hash.addData(message);
        const QByteArray inner = hash.result();
        hash.reset();
        hash.addData(outerPad);
        hash.addData(inner);
        return hash.result();
    };

    QByteArray derived;
    for (quint32 block = 1; derived.size() < keyLength; ++block) {
        QByteArray message = salt;
        message.append(static_cast<char>((block >> 24) & 0xff));
        message.append(static_cast<char>((block >> 16) & 0xff));
        message.append(static_cast<char>((block >> 8) & 0xff));
        message.append(static_cast<char>(block & 0xff));

        QByteArray u = hmac(message);
        QByteArray t = u;
        for (int i = 1; i < iterations; ++i) {
            u = hmac(u);
            for (int j = 0; j < t.size(); ++j) {
                t[j] = static_cast<char>(t.at(j) ^ u.at(j));
            }
        }
        derived.append(t);
    }
    return derived.left(keyLength);
}

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    }
    return diff == 0;
}

Digest hashPassword(const QString &password, int iterations)
{
    Digest digest;
    digest.salt = randomBytes(kSaltBytes);
    digest.iterations = iterations;
    digest.hash = pbkdf2HmacSha256(password.toUtf8(), digest.salt, iterations, kDigestBytes);
    return digest;
}

bool verifyPassword(const QString &password, const Digest &stored)
{
    if (stored.salt.isEmpty() || stored.hash.isEmpty() || stored.iterations < 1) {
        return false;
    }
    const QByteArray candidate =
        pbkdf2HmacSha256(password.toUtf8(), stored.salt, stored.iterations, stored.hash.size());
    return constantTimeEquals(candidate, stored.hash);
}

} // namespace PasswordHasher
