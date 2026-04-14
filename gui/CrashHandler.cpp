#include "CrashHandler.h"

#include "tscrt.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <dbghelp.h>
#else
  #include <signal.h>
  #include <execinfo.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <cstdlib>
#endif

namespace tscrt {

namespace {

QString g_dumpDir;

QString resolveDumpDir()
{
    const QString home = QString::fromLocal8Bit(tscrt_get_home());
    QDir dir(home);
    if (!dir.cd(QString::fromLatin1(TSCRT_DIR_NAME))) {
        if (!dir.mkpath(QString::fromLatin1(TSCRT_DIR_NAME)))
            return {};
        dir.cd(QString::fromLatin1(TSCRT_DIR_NAME));
    }
    if (!dir.exists(QStringLiteral("dumps")))
        dir.mkdir(QStringLiteral("dumps"));
    dir.cd(QStringLiteral("dumps"));
    return dir.absolutePath();
}

#ifdef _WIN32

LONG WINAPI exceptionFilter(EXCEPTION_POINTERS *ep)
{
    if (g_dumpDir.isEmpty())
        return EXCEPTION_EXECUTE_HANDLER;

    const QString stamp = QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString path  = QStringLiteral("%1/tscrt-%2-%3.dmp")
                              .arg(g_dumpDir, stamp)
                              .arg(GetCurrentProcessId());

    HANDLE hFile = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),
                               GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return EXCEPTION_EXECUTE_HANDLER;

    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId          = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers    = FALSE;

    const MINIDUMP_TYPE type = (MINIDUMP_TYPE)(
        MiniDumpNormal |
        MiniDumpWithThreadInfo |
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData);

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                      hFile, type, ep ? &mei : nullptr, nullptr, nullptr);
    CloseHandle(hFile);
    // Restrict the minidump to the current user only — it may contain
    // secrets (SSH/SMTP passwords, scrollback) still sitting in memory.
    const QByteArray pathUtf8 = path.toLocal8Bit();
    tscrt_set_private_perms(pathUtf8.constData());
    return EXCEPTION_EXECUTE_HANDLER;
}

#else // POSIX

static char g_dumpPath[512];

static void posixSignalHandler(int sig, siginfo_t * /*info*/, void * /*ctx*/)
{
    // Async-signal-safe: write backtrace to file using raw fd I/O.
    // Mode 0600 — backtraces can leak symbol addresses and terminal
    // scrollback that should not be world-readable.
    int fd = ::open(g_dumpPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        // glibc marks write() warn_unused_result under _FORTIFY_SOURCE.
        // We're already in a best-effort signal handler — swallow the
        // result via a branch the compiler can't elide.
        const char *msg = "TSCRT crash — signal ";
        ssize_t wn;
        wn = ::write(fd, msg, strlen(msg));
        (void)wn;
        char sigbuf[16];
        int n = snprintf(sigbuf, sizeof(sigbuf), "%d\n", sig);
        wn = ::write(fd, sigbuf, n);
        (void)wn;

        void *bt[64];
        int depth = backtrace(bt, 64);
        backtrace_symbols_fd(bt, depth, fd);
        ::close(fd);
    }
    _exit(128 + sig);
}

#endif

} // namespace

static void purgeOldDumps(const QString &dir, int maxAgeDays)
{
    if (dir.isEmpty() || maxAgeDays <= 0)
        return;
    QDir d(dir);
    const auto entries = d.entryInfoList(
        QStringList{ QStringLiteral("*.dmp"), QStringLiteral("*.crash") },
        QDir::Files);
    const QDateTime cutoff =
        QDateTime::currentDateTime().addDays(-maxAgeDays);
    for (const QFileInfo &e : entries) {
        if (e.lastModified() < cutoff)
            QFile::remove(e.absoluteFilePath());
    }
}

QString installCrashHandler()
{
    g_dumpDir = resolveDumpDir();
    // Dumps can hold sensitive memory (decrypted passwords, scrollback).
    // Don't let them accumulate forever on disk — 7 days is enough for
    // the user to review a crash before we reclaim the space.
    purgeOldDumps(g_dumpDir, 7);

#ifdef _WIN32
    SetUnhandledExceptionFilter(&exceptionFilter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#else
    // Build dump path once so the signal handler can use it safely.
    QByteArray p = QStringLiteral("%1/tscrt-%2.crash")
                       .arg(g_dumpDir)
                       .arg(getpid())
                       .toLocal8Bit();
    strncpy(g_dumpPath, p.constData(), sizeof(g_dumpPath) - 1);

    struct sigaction sa{};
    sa.sa_sigaction = &posixSignalHandler;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
#endif

    return g_dumpDir;
}

QString findLastCrashDump()
{
    const QString dir = resolveDumpDir();
    if (dir.isEmpty())
        return {};
    QDir d(dir);
    const auto entries = d.entryInfoList(
#ifdef _WIN32
        QStringList{ QStringLiteral("*.dmp") },
#else
        QStringList{ QStringLiteral("*.crash") },
#endif
        QDir::Files, QDir::Time);
    if (entries.isEmpty())
        return {};
    return entries.first().absoluteFilePath();
}

void clearCrashDump(const QString &path)
{
    if (!path.isEmpty())
        QFile::remove(path);
}

} // namespace tscrt
