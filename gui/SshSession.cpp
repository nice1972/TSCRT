#include "SshSession.h"

#include "Credentials.h"

#include <QFile>
#include <QtGlobal>
#ifdef _WIN32
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <poll.h>
  #include <netinet/tcp.h>
  #include <cerrno>
#endif

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
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        *err = QStringLiteral("WSAStartup failed");
        return false;
    }
#endif

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
#ifdef _WIN32
    if (m_sock == INVALID_SOCKET) {
#else
    if (m_sock < 0) {
#endif
        freeaddrinfo(res);
        *err = QStringLiteral("socket() failed");
        return false;
    }

    int nodelay = 1;
    setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char *>(&nodelay), sizeof(nodelay));

    if (m_tcpKeepalive) {
        int ka = 1;
        setsockopt(m_sock, SOL_SOCKET, SO_KEEPALIVE,
                   reinterpret_cast<const char *>(&ka), sizeof(ka));
    }

    if (::connect(m_sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        freeaddrinfo(res);
#ifdef _WIN32
        *err = QStringLiteral("connect() failed: %1").arg(WSAGetLastError());
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
#else
        *err = QStringLiteral("connect() failed: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        ::close(m_sock);
        m_sock = -1;
#endif
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

// Returns <home>/tscrt/known_hosts (OpenSSH format), or empty if HOME is unset.
static QString knownHostsPath()
{
    const char *home = tscrt_get_home();
    if (!home || !*home)
        return {};
    return QString::fromLocal8Bit(home)
         + QStringLiteral(TSCRT_PATH_SEP TSCRT_DIR_NAME TSCRT_PATH_SEP "known_hosts");
}

static QString sha256FingerprintHex(LIBSSH2_SESSION *session)
{
    const char *raw = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256);
    if (!raw)
        return {};
    QString out;
    for (int i = 0; i < 32; ++i) {
        out += QString::asprintf("%02x", static_cast<unsigned char>(raw[i]));
        if (i < 31) out += QLatin1Char(':');
    }
    return out;
}

// Validates the SSH server public key against ~/tscrt/known_hosts using the
// TOFU ("trust on first use") model:
//   - MATCH     -> accept silently
//   - NOTFOUND  -> pin the new key, persist, and log the fingerprint
//   - MISMATCH  -> hard-refuse with a detailed error (potential MITM)
bool SshSession::verifyHostKey(QString *err)
{
    LIBSSH2_KNOWNHOSTS *nh = libssh2_knownhost_init(m_session);
    if (!nh) {
        *err = QStringLiteral("libssh2_knownhost_init failed");
        return false;
    }

    const QByteArray khPathBytes = knownHostsPath().toLocal8Bit();
    if (!khPathBytes.isEmpty()) {
        // Missing file is OK on first run; a negative return is expected.
        libssh2_knownhost_readfile(
            nh, khPathBytes.constData(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    }

    size_t keyLen = 0;
    int keyType = 0;
    const char *keyData = libssh2_session_hostkey(m_session, &keyLen, &keyType);
    if (!keyData) {
        libssh2_knownhost_free(nh);
        *err = QStringLiteral("libssh2_session_hostkey failed");
        return false;
    }

    int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW;
    switch (keyType) {
    case LIBSSH2_HOSTKEY_TYPE_RSA:
        typemask |= LIBSSH2_KNOWNHOST_KEY_SSHRSA; break;
    case LIBSSH2_HOSTKEY_TYPE_DSS:
        typemask |= LIBSSH2_KNOWNHOST_KEY_SSHDSS; break;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
        typemask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_256; break;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
        typemask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_384; break;
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
        typemask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_521; break;
    case LIBSSH2_HOSTKEY_TYPE_ED25519:
        typemask |= LIBSSH2_KNOWNHOST_KEY_ED25519; break;
    default:
        typemask |= LIBSSH2_KNOWNHOST_KEY_UNKNOWN; break;
    }

    const int port = m_cfg.port > 0 ? m_cfg.port : 22;
    const int check = libssh2_knownhost_checkp(
        nh, m_cfg.host, port, keyData, keyLen, typemask, nullptr);
    const QString fp = sha256FingerprintHex(m_session);

    if (check == LIBSSH2_KNOWNHOST_CHECK_MISMATCH) {
        libssh2_knownhost_free(nh);
        *err = QStringLiteral(
            "SSH host key MISMATCH for %1:%2 — possible MITM attack.\n"
            "Server offered SHA256:%3\n"
            "If the server key legitimately changed, remove the stale entry "
            "from %4 and reconnect.")
            .arg(QString::fromLocal8Bit(m_cfg.host))
            .arg(port)
            .arg(fp)
            .arg(QString::fromLocal8Bit(khPathBytes));
        return false;
    }

    if (check == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND ||
        check == LIBSSH2_KNOWNHOST_CHECK_FAILURE) {
        char hostPort[512];
        if (port == 22)
            snprintf(hostPort, sizeof(hostPort), "%s", m_cfg.host);
        else
            snprintf(hostPort, sizeof(hostPort), "[%s]:%d", m_cfg.host, port);

        libssh2_knownhost_addc(
            nh, hostPort, nullptr, keyData, keyLen,
            "tscrt TOFU", 10, typemask, nullptr);

        // Ensure the parent directory exists before writing.
        const char *home = tscrt_get_home();
        if (home && *home) {
            char baseDir[MAX_PATH_LEN];
            snprintf(baseDir, sizeof(baseDir),
                     "%s" TSCRT_PATH_SEP "%s", home, TSCRT_DIR_NAME);
            tscrt_ensure_dir(baseDir);
        }

        if (!khPathBytes.isEmpty()) {
            if (libssh2_knownhost_writefile(
                    nh, khPathBytes.constData(),
                    LIBSSH2_KNOWNHOST_FILE_OPENSSH) != 0) {
                qWarning("Failed to persist known_hosts to %s",
                         khPathBytes.constData());
            }
        }

        qWarning("[%s:%d] new host key pinned (SHA256:%s)",
                 m_cfg.host, port, fp.toLocal8Bit().constData());
    }

    libssh2_knownhost_free(nh);
    return true;
}

bool SshSession::authenticate(QString *err)
{
    bool ok = false;

    // 1) SSH Agent (Windows OpenSSH agent / Pageant) — best-effort.
    if (!ok) {
        LIBSSH2_AGENT *agent = libssh2_agent_init(m_session);
        if (agent) {
            if (libssh2_agent_connect(agent) == 0) {
                if (libssh2_agent_list_identities(agent) == 0) {
                    struct libssh2_agent_publickey *id = nullptr;
                    struct libssh2_agent_publickey *prev = nullptr;
                    while (libssh2_agent_get_identity(agent, &id, prev) == 0) {
                        if (libssh2_agent_userauth(agent, m_cfg.username,
                                                   id) == 0) {
                            ok = true;
                            break;
                        }
                        prev = id;
                    }
                }
                libssh2_agent_disconnect(agent);
            }
            libssh2_agent_free(agent);
        }
    }

    // 2) Explicit key file
    if (!ok && m_cfg.keyfile[0]) {
        if (libssh2_userauth_publickey_fromfile(
                m_session, m_cfg.username, nullptr, m_cfg.keyfile,
                m_cfg.password[0] ? m_cfg.password : nullptr) == 0) {
            ok = true;
        }
    }

    // 3) Default key files (~/.ssh/id_rsa, id_ed25519, id_ecdsa)
    if (!ok && !m_cfg.keyfile[0]) {
#ifdef _WIN32
        const QString home = QString::fromLocal8Bit(qgetenv("USERPROFILE"));
#else
        const QString home = QString::fromLocal8Bit(qgetenv("HOME"));
#endif
        const char *keyNames[] = {
            "/.ssh/id_ed25519", "/.ssh/id_rsa", "/.ssh/id_ecdsa"
        };
        for (const char *name : keyNames) {
            const QByteArray path = (home + QString::fromLatin1(name)).toLocal8Bit();
            if (QFile::exists(QString::fromLocal8Bit(path))) {
                if (libssh2_userauth_publickey_fromfile(
                        m_session, m_cfg.username, nullptr,
                        path.constData(),
                        m_cfg.password[0] ? m_cfg.password : nullptr) == 0) {
                    ok = true;
                    break;
                }
            }
        }
    }

    // 4) Password
    if (!ok && m_cfg.password[0]) {
        if (libssh2_userauth_password(m_session, m_cfg.username,
                                      m_cfg.password) == 0)
            ok = true;
    }

    // 5) Keyboard-interactive (many servers only accept this)
    if (!ok && m_cfg.password[0]) {
        *libssh2_session_abstract(m_session) = &m_cfg;
        if (libssh2_userauth_keyboard_interactive(
                m_session, m_cfg.username,
                &SshSession::kbdInteractiveCallback) == 0)
            ok = true;
    }

    if (!ok) {
        if (!m_cfg.password[0] && !m_cfg.keyfile[0])
            *err = QStringLiteral("Authentication failed: no password or key configured");
        else {
            char *e = nullptr;
            libssh2_session_last_error(m_session, &e, nullptr, 0);
            *err = QStringLiteral("Authentication failed: %1")
                       .arg(QString::fromLocal8Bit(e ? e : "unknown"));
        }
        return false;
    }
    return true;
}

void SshSession::kbdInteractiveCallback(
    const char * /*name*/, int /*name_len*/,
    const char * /*instruction*/, int /*instruction_len*/,
    int num_prompts,
    const LIBSSH2_USERAUTH_KBDINT_PROMPT * /*prompts*/,
    LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
    void **abstract)
{
    if (num_prompts <= 0 || !abstract || !*abstract) return;
    auto *cfg = static_cast<ssh_config_t *>(*abstract);

    // Fill the first response (password prompt) with the stored password.
    const size_t len = strlen(cfg->password);
    responses[0].text = static_cast<char *>(malloc(len));
    if (responses[0].text) {
        memcpy(responses[0].text, cfg->password, len);
        responses[0].length = static_cast<unsigned int>(len);
    }
}

bool SshSession::openShell(int cols, int rows, QString *err)
{
    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) {
        *err = QStringLiteral("Failed to open SSH channel");
        return false;
    }
    const QByteArray term = m_termType.isEmpty()
        ? QByteArrayLiteral("xterm-256color")
        : m_termType;
    if (libssh2_channel_request_pty_ex(m_channel,
                                       term.constData(),
                                       (unsigned int)term.size(),
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
        libssh2_session_disconnect(m_session, "tscrt shutdown");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
#ifdef _WIN32
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    WSACleanup();
#else
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
#endif
}

void SshSession::runLoop()
{
    emit connecting();

    libssh2_init(0);

    QString err;
    if (!connectSocket(&err) ||
        !sshHandshake(&err)  ||
        !verifyHostKey(&err) ||
        !authenticate(&err)  ||
        !openShell(m_pendingCols, m_pendingRows, &err)) {
        emit errorOccurred(err);
        cleanup();
        libssh2_exit();
        m_thread.quit();
        return;
    }

    if (m_keepaliveSec > 0) {
        libssh2_keepalive_config(m_session, 1 /*want_reply*/, m_keepaliveSec);
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
#ifdef _WIN32
                    Sleep(1);
#else
                    usleep(1000);
#endif
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

        // Keepalive: ping server when its interval says so. libssh2 tells us
        // how many seconds until the next one — we clamp the idle poll to
        // that so we never oversleep a keepalive tick.
        int pollMs = POLL_TIMEOUT_MS;
        if (m_keepaliveSec > 0) {
            int nextSec = 0;
            int rc = libssh2_keepalive_send(m_session, &nextSec);
            if (rc < 0) {
                err = QStringLiteral("keepalive failed");
                goto done;
            }
            if (nextSec > 0) {
                const int cap = nextSec * 1000;
                if (cap < pollMs) pollMs = cap;
            }
        }

        // Wait on socket if idle.
        if (!gotData && txChunk.isEmpty()) {
#ifdef _WIN32
            WSAPOLLFD pf;
            pf.fd      = m_sock;
            pf.events  = POLLIN;
            pf.revents = 0;
            WSAPoll(&pf, 1, pollMs);
#else
            struct pollfd pf;
            pf.fd      = m_sock;
            pf.events  = POLLIN;
            pf.revents = 0;
            poll(&pf, 1, pollMs);
#endif
        }
    }

done:
    m_running = false;
    cleanup();
    libssh2_exit();
    emit disconnected(err.isEmpty() ? QStringLiteral("closed") : err);
    m_thread.quit();
}
