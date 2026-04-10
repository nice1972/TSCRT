/*
 * SshSession - libssh2-backed terminal session for Windows.
 *
 * Runs its I/O loop on a dedicated QThread. Connection, authentication,
 * PTY allocation and read/write are handled here. Outgoing user input
 * is delivered via Qt::QueuedConnection (sendBytes()) and pushed into
 * the channel from the worker thread.
 */
#pragma once

#include "ISession.h"
#include "tscrt.h"

#include <QByteArray>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>

#include <winsock2.h>
#include <libssh2.h>

class SshSession : public ISession {
    Q_OBJECT
public:
    SshSession(const ssh_config_t &cfg, const QString &name,
               QObject *parent = nullptr);
    ~SshSession() override;

    QString displayName() const override { return m_name; }

public slots:
    void start() override;
    void stop()  override;
    void sendBytes(const QByteArray &data) override;
    void resize(int cols, int rows) override;

private slots:
    void runLoop();   // executes on worker thread

private:
    bool connectSocket(QString *err);
    bool sshHandshake(QString *err);
    bool authenticate(QString *err);
    bool openShell(int cols, int rows, QString *err);
    void cleanup();

    static void kbdInteractiveCallback(
        const char *name, int name_len,
        const char *instruction, int instruction_len,
        int num_prompts,
        const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
        LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
        void **abstract);

    ssh_config_t      m_cfg;
    QString           m_name;
    QThread           m_thread;

    // Worker-thread state
    SOCKET            m_sock      = INVALID_SOCKET;
    LIBSSH2_SESSION  *m_session   = nullptr;
    LIBSSH2_CHANNEL  *m_channel   = nullptr;

    // Outgoing queue (GUI -> worker)
    QMutex            m_outMutex;
    QByteArray        m_outBuf;
    int               m_pendingCols = 80;
    int               m_pendingRows = 24;
    bool              m_resizePending = false;
    bool              m_stopRequested = false;
};
