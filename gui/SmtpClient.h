/*
 * SmtpClient - minimal QSslSocket-based SMTP sender.
 *
 * Supports plaintext / STARTTLS / SMTPS (implicit TLS), AUTH LOGIN, and
 * multipart/mixed MIME messages with a single attachment. Intended for
 * sending TSCRT snapshot files; not a general-purpose mail library.
 */
#pragma once

#include "tscrt.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSslError>
#include <QString>
#include <QStringList>
#include <QTimer>

class QSslSocket;

namespace tscrt {

class SmtpClient : public QObject {
    Q_OBJECT
public:
    explicit SmtpClient(const smtp_config_t &cfg, QObject *parent = nullptr);
    ~SmtpClient() override;

    /// Enqueue one message. Emits sent() or failed() exactly once.
    void send(const QStringList &to,
              const QString     &subject,
              const QString     &bodyText,
              const QString     &attachPath = QString());

signals:
    void sent();
    void failed(const QString &reason);

private slots:
    void onConnected();
    void onEncrypted();
    void onReadyRead();
    void onSocketError();
    void onSslErrors(const QList<QSslError> &errors);
    void onTimeout();

private:
    enum State {
        Idle,
        Connecting,
        Greeting,         // waiting for 220
        EhloSent,         // waiting for 250 after EHLO
        StartTlsSent,     // waiting for 220 after STARTTLS
        EhloAfterTls,     // waiting for 250 after second EHLO
        AuthUserSent,     // waiting for 334 after AUTH LOGIN
        AuthNameSent,     // waiting for 334 after username
        AuthPassSent,     // waiting for 235 after password
        MailFromSent,     // waiting for 250 after MAIL FROM
        RcptToSent,       // waiting for 250 after RCPT TO (current index)
        DataSent,         // waiting for 354 after DATA
        BodySent,         // waiting for 250 after body.
        QuitSent,         // waiting for 221 after QUIT
        Done,
        Failed
    };

    void setState(State s) { m_state = s; }
    void fail(const QString &msg);
    void finishOk();

    void sendLine(const QByteArray &line);
    void parseResponse();
    void processResponse(int code, const QString &text);

    QByteArray buildMime() const;
    static QByteArray b64(const QByteArray &in);

    smtp_config_t m_cfg;
    QSslSocket   *m_sock = nullptr;
    QTimer        m_timeoutTimer;
    QByteArray    m_recvBuf;
    State         m_state = Idle;

    // Current message
    QStringList   m_to;
    QString       m_subject;
    QString       m_bodyText;
    QString       m_attachPath;
    int           m_rcptIndex = 0;
    bool          m_secondEhlo = false;
};

} // namespace tscrt
