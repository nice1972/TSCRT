#include "ActionParser.h"

#include <QDebug>

namespace tscrt {

// Grammar (strict whitelist):
//   \r \n \t \b  — C escapes (CR/LF/TAB/BS)
//   \e           — ESC (0x1b), for arrow keys and ANSI sequences
//   \\           — literal backslash
//   \p           — 1-second pause (splits the chunk list)
//
// Any other backslash sequence is treated as a parser error: the two
// bytes are dropped from the output and a warning is logged. This
// narrows the attack surface that a tampered profile can reach — it
// cannot smuggle unknown escape syntaxes through the parser, and
// operators see the malformed profile in the logs.
static constexpr int kMaxUnknownEscapeWarnings = 4;

QVector<ActionChunk> parseAction(const QString &actionString)
{
    QVector<ActionChunk> result;
    if (actionString.isEmpty())
        return result;

    const QByteArray src = actionString.toUtf8();
    QByteArray current;
    current.reserve(src.size());

    auto flushPause = [&](int ms) {
        ActionChunk c;
        c.bytes  = current;
        c.pauseMs = ms;
        current.clear();
        result.push_back(std::move(c));
    };

    int unknownWarnings = 0;
    for (int i = 0; i < src.size(); ++i) {
        const char ch = src.at(i);
        if (ch != '\\') {
            current.append(ch);
            continue;
        }
        if (i + 1 >= src.size()) {
            // Trailing backslash — drop it rather than emitting it raw,
            // so malformed profiles don't leak a stray '\' to the shell.
            if (unknownWarnings < kMaxUnknownEscapeWarnings) {
                qWarning("ActionParser: trailing backslash dropped");
                ++unknownWarnings;
            }
            break;
        }
        const char esc = src.at(++i);
        switch (esc) {
        case 'r':  current.append('\r'); break;
        case 'n':  current.append('\n'); break;
        case 't':  current.append('\t'); break;
        case 'b':  current.append('\b'); break;
        case 'e':  current.append('\x1b'); break;
        case '\\': current.append('\\'); break;
        case 'p':  flushPause(1000);     break;  // 1 second pause
        default:
            if (unknownWarnings < kMaxUnknownEscapeWarnings) {
                qWarning("ActionParser: unknown escape \\%c dropped", esc);
                ++unknownWarnings;
            }
            // Drop both characters; unknown sequences do not reach the
            // wire. Legitimate actions never rely on unknown escapes.
            break;
        }
    }

    if (!current.isEmpty()) {
        ActionChunk c;
        c.bytes = current;
        c.pauseMs = 0;
        result.push_back(std::move(c));
    }
    return result;
}

} // namespace tscrt
