/*
 * Logging - file-backed Qt message handler for tscrt_win.
 *
 * Captures qDebug/qInfo/qWarning/qCritical/qFatal into:
 *   %APPDATA%/tscrt/logs/tscrt_win.log
 *
 * Rotates at startup when the active log exceeds kMaxBytes; keeps the
 * previous file as tscrt_win.log.1 (up to kMaxRotations rolled files).
 *
 * Thread-safe: serialized through an internal QMutex.
 */
#pragma once

#include <QString>

namespace tscrt {

/// Initialize file logging. Returns the absolute path that will receive
/// log messages, or an empty string on failure (in which case messages
/// fall through to the default Qt handler only).
QString installLogging();

/// Restore the previous Qt message handler and close the log file.
void shutdownLogging();

/// Convenience wrapper used by main.cpp on early startup.
void logBanner();

} // namespace tscrt
