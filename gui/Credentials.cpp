#include "Credentials.h"

#include <QByteArray>
#include <QDebug>
#include <QString>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>

namespace tscrt {

namespace {

constexpr const char *kPrefix = "dpapi:";

QByteArray dpapiProtect(const QByteArray &plain)
{
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()));
    in.cbData = static_cast<DWORD>(plain.size());

    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"tscrt_win", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        qWarning("CryptProtectData failed: 0x%lx", GetLastError());
        return {};
    }
    QByteArray result(reinterpret_cast<const char *>(out.pbData),
                      static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}

QByteArray dpapiUnprotect(const QByteArray &cipher)
{
    if (cipher.isEmpty())
        return {};

    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(cipher.constData()));
    in.cbData = static_cast<DWORD>(cipher.size());

    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        qWarning("CryptUnprotectData failed: 0x%lx", GetLastError());
        return {};
    }
    QByteArray result(reinterpret_cast<const char *>(out.pbData),
                      static_cast<int>(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
}

} // namespace

bool isEncrypted(const QString &stored)
{
    return stored.startsWith(QString::fromLatin1(kPrefix));
}

QString encryptSecret(const QString &plaintext)
{
    if (plaintext.isEmpty())
        return {};
    if (isEncrypted(plaintext))
        return plaintext; // already wrapped

    const QByteArray cipher = dpapiProtect(plaintext.toUtf8());
    if (cipher.isEmpty())
        return plaintext; // fallback: store plaintext rather than losing data

    return QString::fromLatin1(kPrefix) +
           QString::fromLatin1(cipher.toBase64());
}

QString decryptSecret(const QString &stored)
{
    if (stored.isEmpty())
        return {};
    if (!isEncrypted(stored))
        return stored; // legacy plaintext

    const QByteArray b64 = stored.mid(int(qstrlen(kPrefix))).toLatin1();
    const QByteArray cipher = QByteArray::fromBase64(b64);
    const QByteArray plain  = dpapiUnprotect(cipher);
    if (plain.isEmpty()) {
        qWarning("DPAPI decryption failed for stored secret");
        return {};
    }
    return QString::fromUtf8(plain);
}

} // namespace tscrt
