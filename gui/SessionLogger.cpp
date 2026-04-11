#include "SessionLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

/* Replace any character that is forbidden in a Windows filename. */
QString sanitizeForFilename(const QString &in)
{
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|\\s]+"));
    QString out = in;
    out.replace(bad, QStringLiteral("_"));
    if (out.isEmpty()) out = QStringLiteral("session");
    return out;
}

} // namespace

SessionLogger::SessionLogger(const QString &logDir,
                             const QString &sessionName,
                             const QString &host,
                             QObject       *parent)
    : QObject(parent)
{
    QDir dir(logDir);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    const QString stamp = QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyyMMdd_HHmmss"));

    const QString name = QStringLiteral("%1_%2_%3.log")
                             .arg(sanitizeForFilename(sessionName),
                                  sanitizeForFilename(host),
                                  stamp);

    m_file.setFileName(dir.filePath(name));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning("SessionLogger: cannot open %s",
                 qUtf8Printable(m_file.fileName()));
    }
}

SessionLogger::~SessionLogger()
{
    if (m_file.isOpen()) {
        if (!m_atLineStart)
            m_file.write("\n");
        m_file.close();
    }
}

QByteArray SessionLogger::stripAnsi(const QByteArray &data)
{
    int state = 0;
    return stripAnsiStateful(data, &state);
}

QByteArray SessionLogger::stripAnsiStateful(const QByteArray &data, int *state)
{
    QByteArray out;
    out.reserve(data.size());
    int ansi = state ? *state : 0;

    for (char raw : data) {
        const unsigned char c = static_cast<unsigned char>(raw);
        switch (ansi) {
        case 1:
            if (c == '[')        { ansi = 2; continue; }
            if (c == ']' || c == 'P' || c == 'X' || c == '^' || c == '_') {
                ansi = 3; continue;
            }
            ansi = 0;
            continue;
        case 2:
            if (c >= 0x40 && c <= 0x7e) ansi = 0;
            continue;
        case 3:
            if (c == 0x07) { ansi = 0; continue; }
            if (c == 0x1b) { ansi = 4; continue; }
            continue;
        case 4:
            ansi = (c == '\\') ? 0 : 3;
            continue;
        default: break;
        }

        if (c == 0x1b) { ansi = 1; continue; }
        if (c == '\r') continue;
        if (c != '\n' && c != '\t' && c < 0x20) continue;
        out.append(static_cast<char>(c));
    }

    if (state) *state = ansi;
    return out;
}

void SessionLogger::writeTimestampPrefix()
{
    const QString prefix = QDateTime::currentDateTime()
                               .toString(QStringLiteral("[MMdd_HHmmss_zzz] "));
    m_file.write(prefix.toUtf8());
    m_atLineStart = false;
}

void SessionLogger::onBytesReceived(const QByteArray &data)
{
    if (!m_file.isOpen()) return;

    for (char raw : data) {
        const unsigned char c = static_cast<unsigned char>(raw);

        /* ANSI escape state machine — drop everything inside a sequence. */
        switch (m_ansi) {
        case 1: /* just saw ESC */
            if (c == '[')        { m_ansi = 2; continue; }
            if (c == ']' || c == 'P' || c == 'X' || c == '^' || c == '_') {
                m_ansi = 3; continue;
            }
            /* Two-byte sequence — drop this byte and resume. */
            m_ansi = 0;
            continue;
        case 2: /* inside CSI, looking for final byte */
            if (c >= 0x40 && c <= 0x7e) m_ansi = 0;
            continue;
        case 3: /* inside OSC / string, looking for BEL or ST */
            if (c == 0x07) { m_ansi = 0; continue; }
            if (c == 0x1b) { m_ansi = 4; continue; }
            continue;
        case 4: /* saw ESC inside OSC, expecting '\\' to terminate */
            m_ansi = (c == '\\') ? 0 : 3;
            continue;
        default: break;
        }

        if (c == 0x1b) { m_ansi = 1; continue; }   /* swallow ESC */

        /* Line-aware writer with timestamp prefix per line. */
        if (c == '\r')
            continue;                              /* normalise CRLF -> LF */

        if (c == '\n') {
            if (m_atLineStart) writeTimestampPrefix();   /* empty line */
            m_file.write("\n");
            m_atLineStart = true;
            continue;
        }

        if (c < 0x20 && c != '\t') continue;       /* drop other ctrl chars */

        if (m_atLineStart) writeTimestampPrefix();
        const char buf[1] = { static_cast<char>(c) };
        m_file.write(buf, 1);
    }

    m_file.flush();
}
