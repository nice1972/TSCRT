#include "SmtpClient.h"

#include "Credentials.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSslSocket>

namespace tscrt {

namespace {
constexpr int kLineMax = 998;   // RFC 5321 line length ceiling

// Strip CR, LF and other C0 control characters from a header or command
// value. Without this a malicious profile could inject arbitrary SMTP
// commands or MIME headers (e.g. a hidden Bcc:) via from_name, from_addr
// or recipient fields — RFC 5322 header injection.
QByteArray sanitizeHeaderValue(const QByteArray &in)
{
    QByteArray out;
    out.reserve(in.size());
    for (char c : in) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7f)
            continue;   // drops \r, \n, \t, NUL, DEL, etc.
        out += c;
    }
    return out;
}

// from_name is emitted inside a quoted-string; an embedded '"' would
// terminate the quote and let a crafted value smuggle extra tokens.
QByteArray sanitizeQuotedDisplayName(const QByteArray &in)
{
    QByteArray out = sanitizeHeaderValue(in);
    out.replace('"', QByteArray());
    return out;
}
} // namespace

SmtpClient::SmtpClient(const smtp_config_t &cfg, QObject *parent)
    : QObject(parent), m_cfg(cfg)
{
    m_sock = new QSslSocket(this);
    connect(m_sock, &QSslSocket::connected,
            this, &SmtpClient::onConnected);
    connect(m_sock, &QSslSocket::encrypted,
            this, &SmtpClient::onEncrypted);
    connect(m_sock, &QSslSocket::readyRead,
            this, &SmtpClient::onReadyRead);
    connect(m_sock, &QSslSocket::errorOccurred,
            this, &SmtpClient::onSocketError);

    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout,
            this, &SmtpClient::onTimeout);
}

SmtpClient::~SmtpClient() = default;

QByteArray SmtpClient::b64(const QByteArray &in)
{
    return in.toBase64();
}

void SmtpClient::send(const QStringList &to, const QString &subject,
                      const QString &bodyText, const QString &attachPath)
{
    if (m_state != Idle && m_state != Done && m_state != Failed) {
        emit failed(tr("SMTP client is busy"));
        return;
    }
    if (to.isEmpty()) {
        emit failed(tr("No recipients"));
        return;
    }
    if (m_cfg.host[0] == '\0') {
        emit failed(tr("SMTP host is not configured"));
        return;
    }

    m_to         = to;
    m_subject    = subject;
    m_bodyText   = bodyText;
    m_attachPath = attachPath;
    m_rcptIndex  = 0;
    m_secondEhlo = false;
    m_recvBuf.clear();
    m_state      = Connecting;

    const int timeout = m_cfg.timeout_sec > 0 ? m_cfg.timeout_sec : 30;
    m_timeoutTimer.start(timeout * 1000);

    const QString host = QString::fromLocal8Bit(m_cfg.host);
    const int port = m_cfg.port > 0 ? m_cfg.port
                     : (m_cfg.security == 2 ? 465 : 25);

    if (m_cfg.security == 2) {   // SMTPS (implicit TLS)
        m_sock->connectToHostEncrypted(host, static_cast<quint16>(port));
    } else {
        m_sock->connectToHost(host, static_cast<quint16>(port));
    }
}

void SmtpClient::onConnected()
{
    // For plaintext / STARTTLS we expect the server banner next.
    m_state = Greeting;
}

void SmtpClient::onEncrypted()
{
    // SMTPS handshake finished. Server now sends 220 banner.
    if (m_state == Connecting)
        m_state = Greeting;
    // After STARTTLS we must re-EHLO.
    if (m_state == StartTlsSent) {
        m_secondEhlo = true;
        sendLine("EHLO tscrt");
        m_state = EhloAfterTls;
    }
}

void SmtpClient::onReadyRead()
{
    m_recvBuf.append(m_sock->readAll());
    parseResponse();
}

void SmtpClient::parseResponse()
{
    while (true) {
        int nl = m_recvBuf.indexOf('\n');
        if (nl < 0) return;

        QByteArray line = m_recvBuf.left(nl);
        m_recvBuf.remove(0, nl + 1);
        if (line.endsWith('\r')) line.chop(1);
        if (line.size() < 3) continue;

        // Multi-line responses have '-' after the code, final has ' '.
        // We only act on the final line of each response group.
        const char sep = line.size() > 3 ? line.at(3) : ' ';
        if (sep == '-') continue;

        bool ok = false;
        const int code = line.left(3).toInt(&ok);
        if (!ok) continue;

        processResponse(code, QString::fromUtf8(line.mid(4)));
    }
}

void SmtpClient::processResponse(int code, const QString &text)
{
    Q_UNUSED(text);

    auto expect = [this, code](int want, const char *stage) -> bool {
        if (code != want) {
            fail(QStringLiteral("SMTP %1 expected %2, got %3")
                     .arg(QString::fromLatin1(stage))
                     .arg(want).arg(code));
            return false;
        }
        return true;
    };

    switch (m_state) {
    case Greeting:
        if (!expect(220, "greeting")) return;
        sendLine("EHLO tscrt");
        m_state = EhloSent;
        return;

    case EhloSent: {
        if (!expect(250, "EHLO")) return;
        if (m_cfg.security == 1 && !m_secondEhlo) {
            sendLine("STARTTLS");
            m_state = StartTlsSent;
            return;
        }
        // Fall through to auth / mail.
        if (m_cfg.username[0]) {
            sendLine("AUTH LOGIN");
            m_state = AuthUserSent;
        } else {
            sendLine(QByteArray("MAIL FROM:<") +
                     sanitizeHeaderValue(QByteArray(m_cfg.from_addr)) + ">");
            m_state = MailFromSent;
        }
        return;
    }

    case StartTlsSent:
        if (!expect(220, "STARTTLS")) return;
        m_sock->startClientEncryption();
        // onEncrypted() will drive us to EHLO again.
        return;

    case EhloAfterTls:
        if (!expect(250, "EHLO-TLS")) return;
        if (m_cfg.username[0]) {
            sendLine("AUTH LOGIN");
            m_state = AuthUserSent;
        } else {
            sendLine(QByteArray("MAIL FROM:<") +
                     sanitizeHeaderValue(QByteArray(m_cfg.from_addr)) + ">");
            m_state = MailFromSent;
        }
        return;

    case AuthUserSent:
        if (!expect(334, "AUTH LOGIN")) return;
        sendLine(b64(QByteArray(m_cfg.username)));
        m_state = AuthNameSent;
        return;

    case AuthNameSent: {
        if (!expect(334, "AUTH user")) return;
        const QString plain = decryptSecret(QString::fromLocal8Bit(m_cfg.password));
        sendLine(b64(plain.toUtf8()));
        m_state = AuthPassSent;
        return;
    }

    case AuthPassSent:
        if (code != 235) {
            fail(QStringLiteral("SMTP authentication failed (%1)").arg(code));
            return;
        }
        sendLine(QByteArray("MAIL FROM:<") +
                 sanitizeHeaderValue(QByteArray(m_cfg.from_addr)) + ">");
        m_state = MailFromSent;
        return;

    case MailFromSent:
        if (!expect(250, "MAIL FROM")) return;
        sendLine(QByteArray("RCPT TO:<") +
                 sanitizeHeaderValue(m_to.at(m_rcptIndex).toUtf8()) + ">");
        m_state = RcptToSent;
        return;

    case RcptToSent:
        if (code != 250 && code != 251) {
            fail(QStringLiteral("SMTP RCPT TO rejected: %1").arg(code));
            return;
        }
        m_rcptIndex++;
        if (m_rcptIndex < m_to.size()) {
            sendLine(QByteArray("RCPT TO:<") +
                     sanitizeHeaderValue(m_to.at(m_rcptIndex).toUtf8()) + ">");
            return;
        }
        sendLine("DATA");
        m_state = DataSent;
        return;

    case DataSent:
        if (!expect(354, "DATA")) return;
        m_sock->write(buildMime());
        m_sock->write("\r\n.\r\n");
        m_state = BodySent;
        return;

    case BodySent:
        if (!expect(250, "DATA body")) return;
        sendLine("QUIT");
        m_state = QuitSent;
        return;

    case QuitSent:
        // Accept 221 or treat anything else as still-OK since the body
        // has already been accepted by the server.
        finishOk();
        return;

    default:
        return;
    }
}

void SmtpClient::sendLine(const QByteArray &line)
{
    QByteArray l = line;
    if (l.size() > kLineMax) l.truncate(kLineMax);
    m_sock->write(l);
    m_sock->write("\r\n");
}

QByteArray SmtpClient::buildMime() const
{
    const QString boundary = QStringLiteral("tscrt-boundary-%1")
        .arg(QDateTime::currentMSecsSinceEpoch());

    QByteArray mime;

    // Headers. All externally-controlled values are stripped of CR/LF and
    // other control characters before being written to prevent RFC 5322
    // header injection (e.g. a smuggled "Bcc:" via from_name).
    const QByteArray fromAddrSan =
        sanitizeHeaderValue(QByteArray(m_cfg.from_addr));
    if (m_cfg.from_name[0]) {
        mime += "From: \"";
        mime += sanitizeQuotedDisplayName(QByteArray(m_cfg.from_name));
        mime += "\" <";
        mime += fromAddrSan;
        mime += ">\r\n";
    } else {
        mime += "From: ";
        mime += fromAddrSan;
        mime += "\r\n";
    }
    QByteArray toHeader;
    for (int i = 0; i < m_to.size(); ++i) {
        if (i > 0) toHeader += ", ";
        toHeader += sanitizeHeaderValue(m_to.at(i).toUtf8());
    }
    mime += "To: " + toHeader + "\r\n";
    mime += "Subject: =?UTF-8?B?" + m_subject.toUtf8().toBase64() + "?=\r\n";
    mime += "Date: " + QDateTime::currentDateTime()
                         .toString(Qt::RFC2822Date).toUtf8() + "\r\n";
    mime += "MIME-Version: 1.0\r\n";

    const bool hasAttach = !m_attachPath.isEmpty()
                           && QFile::exists(m_attachPath);

    if (!hasAttach) {
        mime += "Content-Type: text/plain; charset=UTF-8\r\n";
        mime += "Content-Transfer-Encoding: base64\r\n\r\n";
        const QByteArray bb = m_bodyText.toUtf8().toBase64();
        // Wrap at 76 chars per RFC 2045.
        for (int i = 0; i < bb.size(); i += 76) {
            mime += bb.mid(i, 76);
            mime += "\r\n";
        }
        return mime;
    }

    mime += "Content-Type: multipart/mixed; boundary=\"" + boundary.toUtf8() + "\"\r\n\r\n";
    mime += "--" + boundary.toUtf8() + "\r\n";
    mime += "Content-Type: text/plain; charset=UTF-8\r\n";
    mime += "Content-Transfer-Encoding: base64\r\n\r\n";
    const QByteArray bodyB64 = m_bodyText.toUtf8().toBase64();
    for (int i = 0; i < bodyB64.size(); i += 76) {
        mime += bodyB64.mid(i, 76);
        mime += "\r\n";
    }

    // Attachment
    QFile f(m_attachPath);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray content = f.readAll();
        const QString fname = QFileInfo(m_attachPath).fileName();
        mime += "--" + boundary.toUtf8() + "\r\n";
        mime += "Content-Type: application/octet-stream; name=\""
                + fname.toUtf8() + "\"\r\n";
        mime += "Content-Transfer-Encoding: base64\r\n";
        mime += "Content-Disposition: attachment; filename=\""
                + fname.toUtf8() + "\"\r\n\r\n";
        const QByteArray c64 = content.toBase64();
        for (int i = 0; i < c64.size(); i += 76) {
            mime += c64.mid(i, 76);
            mime += "\r\n";
        }
    }

    mime += "--" + boundary.toUtf8() + "--\r\n";
    return mime;
}

void SmtpClient::onSocketError()
{
    if (m_state == Done || m_state == Failed) return;
    fail(QStringLiteral("SMTP socket error: %1").arg(m_sock->errorString()));
}

void SmtpClient::onTimeout()
{
    if (m_state == Done || m_state == Failed) return;
    fail(tr("SMTP timeout"));
}

void SmtpClient::fail(const QString &msg)
{
    m_state = Failed;
    m_timeoutTimer.stop();
    if (m_sock->state() != QAbstractSocket::UnconnectedState)
        m_sock->abort();
    emit failed(msg);
}

void SmtpClient::finishOk()
{
    m_state = Done;
    m_timeoutTimer.stop();
    m_sock->disconnectFromHost();
    emit sent();
}

} // namespace tscrt
