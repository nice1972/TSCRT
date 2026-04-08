#include "SerialSession.h"

#include <QtGlobal>

namespace {
constexpr int IO_BUF = 4096;
}

SerialSession::SerialSession(const serial_config_t &cfg, const QString &name,
                             QObject *parent)
    : ISession(parent), m_cfg(cfg), m_name(name)
{
    moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, this, &SerialSession::runLoop);
}

SerialSession::~SerialSession()
{
    stop();
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait(2000);
    }
}

void SerialSession::start()
{
    if (m_thread.isRunning())
        return;
    m_stopRequested = false;
    m_thread.start();
}

void SerialSession::stop()
{
    QMutexLocker lock(&m_outMutex);
    m_stopRequested = true;
}

void SerialSession::sendBytes(const QByteArray &data)
{
    QMutexLocker lock(&m_outMutex);
    m_outBuf.append(data);
}

bool SerialSession::openPort(QString *err)
{
    // Use \\.\COMx form so high COM numbers (>= 10) work.
    QString path = QStringLiteral("\\\\.\\%1")
                       .arg(QString::fromLocal8Bit(m_cfg.device));

    m_handle = CreateFileW((LPCWSTR)path.utf16(),
                           GENERIC_READ | GENERIC_WRITE,
                           0,                      // exclusive
                           nullptr,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,  // synchronous
                           nullptr);
    if (m_handle == INVALID_HANDLE_VALUE) {
        *err = QStringLiteral("Cannot open %1 (err=%2)")
                   .arg(path).arg(GetLastError());
        return false;
    }
    return true;
}

bool SerialSession::configurePort(QString *err)
{
    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(m_handle, &dcb)) {
        *err = QStringLiteral("GetCommState failed");
        return false;
    }

    dcb.BaudRate = m_cfg.baudrate > 0 ? m_cfg.baudrate : CBR_115200;
    dcb.ByteSize = (BYTE)(m_cfg.databits > 0 ? m_cfg.databits : 8);
    dcb.StopBits = (m_cfg.stopbits == 2) ? TWOSTOPBITS : ONESTOPBIT;

    switch (m_cfg.parity) {
    case PARITY_ODD:  dcb.Parity = ODDPARITY;  dcb.fParity = TRUE;  break;
    case PARITY_EVEN: dcb.Parity = EVENPARITY; dcb.fParity = TRUE;  break;
    default:          dcb.Parity = NOPARITY;   dcb.fParity = FALSE; break;
    }

    dcb.fOutxCtsFlow = (m_cfg.flowcontrol == FLOW_HARDWARE) ? TRUE : FALSE;
    dcb.fRtsControl  = (m_cfg.flowcontrol == FLOW_HARDWARE)
                          ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
    dcb.fOutX        = (m_cfg.flowcontrol == FLOW_SOFTWARE) ? TRUE : FALSE;
    dcb.fInX         = (m_cfg.flowcontrol == FLOW_SOFTWARE) ? TRUE : FALSE;
    dcb.fBinary      = TRUE;
    dcb.fNull        = FALSE;
    dcb.fErrorChar   = FALSE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(m_handle, &dcb)) {
        *err = QStringLiteral("SetCommState failed");
        return false;
    }

    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout         = MAXDWORD;
    to.ReadTotalTimeoutConstant    = 0;
    to.ReadTotalTimeoutMultiplier  = 0;
    to.WriteTotalTimeoutConstant   = 50;
    to.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(m_handle, &to);

    PurgeComm(m_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return true;
}

void SerialSession::cleanup()
{
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}

void SerialSession::runLoop()
{
    emit connecting();

    QString err;
    if (!openPort(&err) || !configurePort(&err)) {
        emit errorOccurred(err);
        cleanup();
        m_thread.quit();
        return;
    }

    m_running = true;
    emit connected();

    QByteArray rxBuf;
    rxBuf.resize(IO_BUF);

    while (true) {
        bool stopNow;
        QByteArray txChunk;
        {
            QMutexLocker lock(&m_outMutex);
            stopNow = m_stopRequested;
            txChunk = m_outBuf;
            m_outBuf.clear();
        }
        if (stopNow)
            break;

        // Write
        if (!txChunk.isEmpty()) {
            DWORD written = 0;
            WriteFile(m_handle, txChunk.constData(),
                      (DWORD)txChunk.size(), &written, nullptr);
        }

        // Read whatever is available right now (timeouts make this nonblocking).
        DWORD got = 0;
        if (ReadFile(m_handle, rxBuf.data(),
                     (DWORD)rxBuf.size(), &got, nullptr) && got > 0) {
            emit bytesReceived(QByteArray(rxBuf.constData(), (int)got));
        } else {
            // Idle; sleep briefly to avoid busy loop.
            Sleep(10);
        }
    }

    m_running = false;
    cleanup();
    emit disconnected(err.isEmpty() ? QStringLiteral("closed") : err);
    m_thread.quit();
}
