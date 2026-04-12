#include "Credentials.h"

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QUuid>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <wincrypt.h>
#elif defined(__APPLE__)
  #include <CoreFoundation/CoreFoundation.h>
  #include <Security/Security.h>
#endif

namespace tscrt {

namespace {

constexpr const char *kDpapiPrefix    = "dpapi:";
constexpr const char *kKeychainPrefix = "keychain:";

#ifdef _WIN32
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
#elif defined(__APPLE__)
// Generic-password keychain backing. Each encrypted secret is stored as a
// separate item keyed by a freshly-minted UUID that is written into the
// profile as "keychain:<uuid>". Re-encrypting a field leaks the previous
// item (intentional — cheap and safer than orphan-chasing deletes).

static CFStringRef kServiceName = CFSTR("com.tepseg.tscrt");

static CFStringRef cfStr(const QString &s)
{
    const QByteArray b = s.toUtf8();
    return CFStringCreateWithBytes(
        nullptr,
        reinterpret_cast<const UInt8 *>(b.constData()),
        b.size(), kCFStringEncodingUTF8, false);
}

static bool keychainStore(const QString &account, const QByteArray &secret)
{
    CFStringRef accountRef = cfStr(account);
    CFDataRef   dataRef    = CFDataCreate(
        nullptr,
        reinterpret_cast<const UInt8 *>(secret.constData()),
        secret.size());

    const void *keys[]   = { kSecClass, kSecAttrService,
                             kSecAttrAccount, kSecValueData };
    const void *values[] = { kSecClassGenericPassword, kServiceName,
                             accountRef, dataRef };
    CFDictionaryRef attrs = CFDictionaryCreate(
        nullptr, keys, values, 4,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    OSStatus status = SecItemAdd(attrs, nullptr);

    CFRelease(attrs);
    CFRelease(dataRef);
    CFRelease(accountRef);
    return status == errSecSuccess;
}

static QByteArray keychainLoad(const QString &account)
{
    CFStringRef accountRef = cfStr(account);

    const void *keys[]   = { kSecClass, kSecAttrService, kSecAttrAccount,
                             kSecReturnData, kSecMatchLimit };
    const void *values[] = { kSecClassGenericPassword, kServiceName,
                             accountRef, kCFBooleanTrue, kSecMatchLimitOne };
    CFDictionaryRef query = CFDictionaryCreate(
        nullptr, keys, values, 5,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);

    QByteArray out;
    if (status == errSecSuccess && result) {
        CFDataRef data = static_cast<CFDataRef>(result);
        out = QByteArray(
            reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
            static_cast<int>(CFDataGetLength(data)));
        CFRelease(result);
    }
    CFRelease(query);
    CFRelease(accountRef);
    return out;
}
#endif

} // namespace

bool isEncrypted(const QString &stored)
{
    return stored.startsWith(QString::fromLatin1(kDpapiPrefix))
        || stored.startsWith(QString::fromLatin1(kKeychainPrefix));
}

QString encryptSecret(const QString &plaintext)
{
    if (plaintext.isEmpty())
        return {};
    if (isEncrypted(plaintext))
        return plaintext; // already wrapped

#ifdef _WIN32
    const QByteArray cipher = dpapiProtect(plaintext.toUtf8());
    if (cipher.isEmpty())
        return plaintext; // fallback: store plaintext rather than losing data

    return QString::fromLatin1(kDpapiPrefix) +
           QString::fromLatin1(cipher.toBase64());
#elif defined(__APPLE__)
    const QString account =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!keychainStore(account, plaintext.toUtf8())) {
        qWarning("Keychain store failed; storing plaintext as fallback");
        return plaintext;
    }
    return QString::fromLatin1(kKeychainPrefix) + account;
#else
    // Linux: no OS-level secret store wired up yet; store plaintext.
    return plaintext;
#endif
}

QString decryptSecret(const QString &stored)
{
    if (stored.isEmpty())
        return {};

    if (stored.startsWith(QString::fromLatin1(kDpapiPrefix))) {
#ifdef _WIN32
        const QByteArray b64 = stored.mid(int(qstrlen(kDpapiPrefix))).toLatin1();
        const QByteArray cipher = QByteArray::fromBase64(b64);
        const QByteArray plain  = dpapiUnprotect(cipher);
        if (plain.isEmpty()) {
            qWarning("DPAPI decryption failed for stored secret");
            return {};
        }
        return QString::fromUtf8(plain);
#else
        qWarning("DPAPI-encrypted secret cannot be decrypted on this platform");
        return {};
#endif
    }

    if (stored.startsWith(QString::fromLatin1(kKeychainPrefix))) {
#ifdef __APPLE__
        const QString account = stored.mid(int(qstrlen(kKeychainPrefix)));
        const QByteArray data = keychainLoad(account);
        if (data.isEmpty()) {
            qWarning("Keychain lookup failed for stored secret");
            return {};
        }
        return QString::fromUtf8(data);
#else
        qWarning("Keychain-backed secret cannot be decrypted on this platform");
        return {};
#endif
    }

    return stored; // legacy plaintext
}

} // namespace tscrt
