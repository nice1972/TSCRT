#include "ActionParser.h"

namespace tscrt {

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

    for (int i = 0; i < src.size(); ++i) {
        const char ch = src.at(i);
        if (ch != '\\') {
            current.append(ch);
            continue;
        }
        if (i + 1 >= src.size()) {
            current.append('\\');
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
            current.append('\\');
            current.append(esc);
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
