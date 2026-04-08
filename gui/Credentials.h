/*
 * Credentials - DPAPI-backed secret encryption.
 *
 * Wraps CryptProtectData/CryptUnprotectData so that passwords stored in
 * tscrt.profile are bound to the current Windows user account. The
 * profile only contains base64-encoded ciphertext prefixed with the
 * sentinel "dpapi:" so plaintext legacy values still load correctly.
 *
 * The wire format used inside profile fields is:
 *     dpapi:<base64(ciphertext)>
 *
 * If decryption fails (e.g. profile copied to another machine/user),
 * the call returns an empty string and a warning is logged - the user
 * can re-enter the password through the Settings dialog.
 */
#pragma once

#include <QByteArray>
#include <QString>

namespace tscrt {

/// Encode a plaintext secret as a portable string suitable for storing
/// in profile fields. Always returns a string starting with "dpapi:".
QString encryptSecret(const QString &plaintext);

/// Decode a value previously written by encryptSecret(). If the value
/// does not start with "dpapi:" it is treated as legacy plaintext and
/// returned unchanged. Returns empty on decryption failure.
QString decryptSecret(const QString &stored);

/// Convenience: detect whether a stored value is already encrypted.
bool isEncrypted(const QString &stored);

} // namespace tscrt
