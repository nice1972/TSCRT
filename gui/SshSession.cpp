#include "SshSession.h"

#include "Credentials.h"

#include <QtGlobal>
#include <ws2tcpip.h>

#include <cstring>

namespace {
constexpr int IO_BUF = 65536;
constexpr int POLL_TIMEOUT_MS = 30;
} // namespace

SshSession::SshSession(const ssh_config_t &cfg, const QString &name,
                       QObject *parent)
    : ISession(parent), m_cfg(cfg), m_name(name)
{
    // Decrypt DPAPI-protected password at session-construction time so
    // the worker thread sees the plaintext libssh2 expects.
    if (m_cfg.password[0]) {
        const QString plain = tscrt::decryptSecret(
            QString::fromLocal8Bit(m_cfg.password));
        const QByteArray b = plain.toLocal8Bit();
        const int n = qMin<int>(int(sizeof(m_cfg.password)) - 1, b.size());
        memcpy(m_cfg.password, b.constData(), n);
        m_cfg.password[n] = '\0';
    }

    // Move ourselves to the worker thread; queued slots execute there.
    moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, this, &SshSession::runLoop);
}

SshSession::~SshSession()
{
    stop();
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait(2000);
    }
}

void SshSession::start()
{
    if (m_thread.isRunning())
        return;
    m_stopRequested = false;
    m_thread.start();
}

void SshSession::stop()
{
    {
        QMutexLocker lock(&m_outMutex);
        m_stopRequested = true;
    }
}

void SshSession::sendBytes(const QByteArray &data)
{
    QMutexLocker lock(&m_outMutex);
    m_outBuf.append(data);
}

void SshSession::resize(int cols, int rows)
{
    QMutexLocker lock(&m_outMutex);
    m_pendingCols   = cols;
    m_pendingRows   = rows;
    m_resizePending = true;
}

bool SshSession::connectSocket(QString *err)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        *err = QStringLiteral("WSAStartup failed");
        return false;
    }

    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port[16];
    snprintf(port, sizeof(port), "%d", m_cfg.port > 0 ? m_cfg.port : 22);

    addrinfo *res = nullptr;
    if (getaddrinfo(m_cfg.host, port, &hints, &res) != 0 || !res) {
        *err = QStringLiteral("Failed to resolve %1").arg(QString::fromLocal8Bit(m_cfg.host));
        return false;
    }

    m_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (m_sock == INVALID_SOCKET) {
        freeaddrinfo(res);
        *err = QStringLiteral("socket() failed");
        return false;
    }

    BOOL nodelay = TRUE;
    setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char *>(&nodelay), sizeof(nodelay));

    if (::connect(m_sock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(res);
        *err = QStringLiteral("connect() failed: %1").arg(WSAGetLastError());
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    freeaddrinfo(res);
    return true;
}

bool SshSession::sshHandshake(QString *err)
{
    m_session = libssh2_session_init();
    if (!m_session) {
        *err = QStringLiteral("libssh2_session_init failed");
        return false;
    }
    libssh2_session_flag(m_session, LIBSSH2_FLAG_COMPRESS, 1);

    if (libssh2_session_handshake(m_session, (libssh2_socket_t)m_sock) != 0) {
        char *e = nullptr;
        libssh2_session_last_error(m_session, &e, nullptr, 0);
        *err = QStringLiteral("SSH handshake failed: %1")
                   .arg(QString::fromLocal8Bit(e ? e : "unknown"));
        return false;
    }
    return true;
}

bool SshSession::authenticate(QString *err)
{
    bool ok = false;
    if (m_cfg.keyfile[0]) {
        if (libssh2_userauth_publickey_fromfile(
                m_session, m_cfg.username, nullptr, m_cfg.keyfile,
                m_cfg.password[0] ? m_cfg.password : nullptr) == 0) {
            ok = true;
        }
    }
    if (!ok && m_cfg.password[0]) {
        if (libssh2_userauth_password(m_session, m_cfg.username,
                                      m_cfg.password) == 0)
            ok = true;
    }
    if (!ok) {
        char *e = nullptr;
        libssh2_session_last_error(m_session, &e, nullptr, 0);
        *err = QStringLiteral("Authentication failed: %1")
                   .arg(QString::fromLocal8Bit(e ? e : "no method"));
        return false;
    }
    return true;
}

bool SshSession::openShell(int cols, int rows, QString *err)
{
    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) {
        *err = QStringLiteral("Failed to open SSH channel");
        return false;
    }
    if (libssh2_channel_request_pty_ex(m_channel, "xterm-256color", 14,
                                       nullptr, 0,
                                       cols, rows, 0, 0) != 0) {
        *err = QStringLiteral("Failed to request PTY");
        return false;
    }
    if (libssh2_channel_shell(m_channel) != 0) {
        *err = QStringLiteral("Failed to start shell");
        return false;
    }
    libssh2_session_set_blocking(m_session, 0);
    return true;
}

void SshSession::cleanup()
{
    if (m_channel) {
        libssh2_channel_send_eof(m_channel);
        libssh2_channel_close(m_channel);
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
    }
    if (m_session) {
        libssh2_session_disconnect(m_session, "tscrt_win shutdown");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    WSACleanup();
}

void SshSession::runLoop()
{
    emit connecting();

    libssh2_init(0);

    QString err;
    if (!connectSocket(&err) ||
        !sshHandshake(&err)  ||
        !authenticate(&err)  ||
        !openShell(m_pendingCols, m_pendingRows, &err)) {
        emit errorOccurred(err);
        cleanup();
        libssh2_exit();
        m_thread.quit();
        return;
    }

    m_running = true;
    emit connected();

    QByteArray rxBuf;
    rxBuf.resize(IO_BUF);

    while (true) {
        // Pending stop?
        bool stopNow;
        QByteArray txChunk;
        bool resizeNow;
        int  newCols, newRows;
        {
            QMutexLocker lock(&m_outMutex);
            stopNow   = m_stopRequested;
            txChunk   = m_outBuf;
            m_outBuf.clear();
            resizeNow = m_resizePending;
            newCols   = m_pendingCols;
            newRows   = m_pendingRows;
            m_resizePending = false;
        }
        if (stopNow)
            break;

        if (resizeNow) {
            libssh2_channel_request_pty_size(m_channel, newCols, newRows);
        }

        // Outgoing user input -> SSH channel
        if (!txChunk.isEmpty()) {
            int sent = 0;
            while (sent < txChunk.size()) {
                ssize_t w = libssh2_channel_write(
                    m_channel, txChunk.constData() + sent,
                    txChunk.size() - sent);
                if (w == LIBSSH2_ERROR_EAGAIN) {
                    Sleep(1);
                    continue;
                }
                if (w < 0) {
                    err = QStringLiteral("write failed");
                    goto done;
                }
                sent += static_cast<int>(w);
            }
        }

        // Incoming
        bool gotData = false;
        for (;;) {
            ssize_t n = libssh2_channel_read(
                m_channel, rxBuf.data(), rxBuf.size());
            if (n == LIBSSH2_ERROR_EAGAIN || n == 0)
                break;
            if (n < 0) {
                err = QStringLiteral("channel read error");
                goto done;
            }
            emit bytesReceived(QByteArray(rxBuf.constData(), (int)n));
            gotData = true;
        }

        if (libssh2_channel_eof(m_channel)) {
            err = QStringLiteral("remote closed connection");
            break;
        }

        // Wait on socket if idle.
        if (!gotData && txChunk.isEmpty()) {
            WSAPOLLFD pf;
            pf.fd      = m_sock;
            pf.events  = POLLIN;
            pf.revents = 0;
            WSAPoll(&pf, 1, POLL_TIMEOUT_MS);
        }
    }

done:
    m_running = false;
    cleanup();
    libssh2_exit();
    emit disconnected(err.isEmpty() ? QStringLiteral("closed") : err);
    m_thread.quit();
}
