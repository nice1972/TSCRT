/*
 * Logging - file-backed Qt message handler for tscrt_win.
 *
 * Captures qDebug/qInfo/qWarning/qCritical/qFatal into a platform log
 * directory. Tries the primary location first (profile dir / logs), and
 * falls back to the system TEMP dir if the primary cannot be created or
 * written to. Whichever path actually succeeded is recorded and can be
 * retrieved via currentLogDir()/currentLogFile().
 *
 * Rotates at startup when the active log exceeds the size budget; keeps
 * the previous file as *.log.1 (up to 5 rolled files).
 *
 * Thread-safe: serialized through an internal QMutex.
 */
#pragma once

#include <QString>
#include <QStringList>

namespace tscrt {

/// Initialize file logging. Returns the absolute path that will receive
/// log messages, or an empty string on failure (in which case messages
/// fall through to the default Qt handler only).
QString installLogging();

/// Restore the previous Qt message handler and close the log file.
void shutdownLogging();

/// Convenience wrapper used by main.cpp on early startup.
void logBanner();

/// Directory that is actually receiving log files (may differ from the
/// primary %APPDATA% location if a fallback was used). Empty if logging
/// is not yet installed or failed entirely.
QString currentLogDir();

/// Full path of the active log file, or empty if none.
QString currentLogFile();

/// Paths that were attempted before logging was successfully opened.
/// Each entry is "<path> — <reason>" for failed attempts; the final entry
/// is the path that succeeded. Useful for the diagnostic exporter.
QStringList logOpenAttempts();

} // namespace tscrt
