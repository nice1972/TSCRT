/*
 * SerialSession - Win32 serial console session.
 *
 * Uses CreateFile("\\\\.\\COMx") + DCB + OVERLAPPED I/O on a worker
 * thread. Connection parameters come from serial_config_t.
 */
#pragma once

#include "ISession.h"

#include <QByteArray>
#include <QMutex>
#include <QString>
#include <QThread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Windows headers define PARITY_ODD/EVEN as bit-flag macros which collide
// with the parity_t enum in tscrt.h. Drop them before pulling tscrt.h in.
#ifdef PARITY_NONE
#undef PARITY_NONE
#endif
#ifdef PARITY_ODD
#undef PARITY_ODD
#endif
#ifdef PARITY_EVEN
#undef PARITY_EVEN
#endif

#include "tscrt.h"

class SerialSession : public ISession {
    Q_OBJECT
public:
    SerialSession(const serial_config_t &cfg, const QString &name,
                  QObject *parent = nullptr);
    ~SerialSession() override;

    QString displayName() const override { return m_name; }

public slots:
    void start() override;
    void stop()  override;
    void sendBytes(const QByteArray &data) override;
    void resize(int /*cols*/, int /*rows*/) override {}

private slots:
    void runLoop();

private:
    bool openPort(QString *err);
    bool configurePort(QString *err);
    void cleanup();

    serial_config_t m_cfg;
    QString         m_name;
    QThread         m_thread;

    HANDLE          m_handle = INVALID_HANDLE_VALUE;
    QMutex          m_outMutex;
    QByteArray      m_outBuf;
    bool            m_stopRequested = false;
};
