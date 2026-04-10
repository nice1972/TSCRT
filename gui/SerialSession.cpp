#include "SerialSession.h"

#include <QtGlobal>
#include <cstring>

#ifndef _WIN32
  #include <termios.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <cerrno>
#endif

namespace {
constexpr int IO_BUF = 4096;

#ifndef _WIN32
static speed_t mapBaud(int baud)
{
    switch (baud) {
    case 1200:   return B1200;
    case 2400:   return B2400;
    case 4800:   return B4800;
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    default:     return B115200;
    }
}
#endif
} // namespace

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
#ifdef _WIN32
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
#else
    const QByteArray devPath = QString::fromLocal8Bit(m_cfg.device).toLocal8Bit();
    m_fd = ::open(devPath.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        *err = QStringLiteral("Cannot open %1: %2")
                   .arg(QString::fromLocal8Bit(m_cfg.device),
                        QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    // Clear non-blocking after open (used only to avoid controlling terminal)
    fcntl(m_fd, F_SETFL, 0);
#endif
    return true;
}

bool SerialSession::configurePort(QString *err)
{
#ifdef _WIN32
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
#else
    struct termios tio;
    if (tcgetattr(m_fd, &tio) != 0) {
        *err = QStringLiteral("tcgetattr failed: %1")
                   .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    cfmakeraw(&tio);

    speed_t speed = mapBaud(m_cfg.baudrate > 0 ? m_cfg.baudrate : 115200);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    tio.c_cflag &= ~CSIZE;
    switch (m_cfg.databits > 0 ? m_cfg.databits : 8) {
    case 5: tio.c_cflag |= CS5; break;
    case 6: tio.c_cflag |= CS6; break;
    case 7: tio.c_cflag |= CS7; break;
    default: tio.c_cflag |= CS8; break;
    }

    if (m_cfg.stopbits == 2) tio.c_cflag |= CSTOPB;
    else tio.c_cflag &= ~CSTOPB;

    tio.c_cflag &= ~(PARENB | PARODD);
    switch (m_cfg.parity) {
    case PARITY_ODD:  tio.c_cflag |= (PARENB | PARODD); break;
    case PARITY_EVEN: tio.c_cflag |= PARENB; break;
    default: break;
    }

    tio.c_cflag &= ~CRTSCTS;
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    if (m_cfg.flowcontrol == FLOW_HARDWARE)
        tio.c_cflag |= CRTSCTS;
    else if (m_cfg.flowcontrol == FLOW_SOFTWARE)
        tio.c_iflag |= (IXON | IXOFF);

    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 1;  // 100ms read timeout

    if (tcsetattr(m_fd, TCSANOW, &tio) != 0) {
        *err = QStringLiteral("tcsetattr failed: %1")
                   .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    tcflush(m_fd, TCIOFLUSH);
#endif
    return true;
}

void SerialSession::cleanup()
{
#ifdef _WIN32
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
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
#ifdef _WIN32
            DWORD written = 0;
            WriteFile(m_handle, txChunk.constData(),
                      (DWORD)txChunk.size(), &written, nullptr);
#else
            ::write(m_fd, txChunk.constData(), txChunk.size());
#endif
        }

        // Read whatever is available right now.
#ifdef _WIN32
        DWORD got = 0;
        if (ReadFile(m_handle, rxBuf.data(),
                     (DWORD)rxBuf.size(), &got, nullptr) && got > 0) {
            emit bytesReceived(QByteArray(rxBuf.constData(), (int)got));
        } else {
            Sleep(10);
        }
#else
        ssize_t got = ::read(m_fd, rxBuf.data(), rxBuf.size());
        if (got > 0) {
            emit bytesReceived(QByteArray(rxBuf.constData(), (int)got));
        } else {
            usleep(10000);
        }
#endif
    }

    m_running = false;
    cleanup();
    emit disconnected(err.isEmpty() ? QStringLiteral("closed") : err);
    m_thread.quit();
}
