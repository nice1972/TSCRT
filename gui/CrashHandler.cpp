#include "CrashHandler.h"

#include "tscrt.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>

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

LONG WINAPI exceptionFilter(EXCEPTION_POINTERS *ep)
{
    if (g_dumpDir.isEmpty())
        return EXCEPTION_EXECUTE_HANDLER;

    const QString stamp = QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString path  = QStringLiteral("%1/tscrt_win-%2-%3.dmp")
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
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

QString installCrashHandler()
{
    g_dumpDir = resolveDumpDir();
    SetUnhandledExceptionFilter(&exceptionFilter);
    // Disable Windows Error Reporting popup so our dump runs cleanly.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    return g_dumpDir;
}

QString findLastCrashDump()
{
    const QString dir = resolveDumpDir();
    if (dir.isEmpty())
        return {};
    QDir d(dir);
    const auto entries = d.entryInfoList(
        QStringList{ QStringLiteral("*.dmp") },
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
