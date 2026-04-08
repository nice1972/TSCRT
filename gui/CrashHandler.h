/*
 * CrashHandler - SEH unhandled exception filter + minidump writer.
 *
 * Installs a SetUnhandledExceptionFilter that writes a .dmp file under
 * %APPDATA%/tscrt/dumps/ containing the crashing thread context. The
 * dump filename embeds the timestamp and PID. After writing, control
 * falls through to EXCEPTION_EXECUTE_HANDLER so the OS terminates the
 * process (no zombie state).
 */
#pragma once

#include <QString>

namespace tscrt {

/// Install the unhandled-exception filter. Returns the directory that
/// will receive minidumps. Call once at startup, before MainWindow.
QString installCrashHandler();

/// Returns the most recent .dmp file in the dumps directory, or an
/// empty string if none. Used by main() to surface a "crash report
/// available" dialog on the next launch.
QString findLastCrashDump();

/// Delete a previously reported dump.
void clearCrashDump(const QString &path);

} // namespace tscrt
