/*
 * ActionParser - decode tscrt button/action escape sequences.
 *
 * Mirrors the original Linux tscrt button-bar action grammar so that
 * profiles port without translation:
 *
 *   \r  - CR (0x0D)
 *   \n  - LF (0x0A)
 *   \t  - TAB
 *   \b  - BS
 *   \e  - ESC (0x1B)
 *   \p  - 1-second pause (encoded as 0xFF marker bytes for the executor)
 *   \\  - literal backslash
 *
 * The parser produces a list of (chunk, pauseAfterMs) pairs so the
 * executor can interleave writes and timer waits without re-parsing.
 */
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace tscrt {

struct ActionChunk {
    QByteArray bytes;       // raw bytes to send
    int        pauseMs = 0; // pause AFTER sending these bytes (0 = none)
};

QVector<ActionChunk> parseAction(const QString &actionString);

} // namespace tscrt
