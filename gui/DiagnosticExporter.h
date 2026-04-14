/*
 * DiagnosticExporter - gather a gzipped diagnostic bundle for support.
 *
 * The bundle is a single gzipped UTF-8 text file containing:
 *   - a system-info header (OS, Qt/libssh2/libvterm versions, locale,
 *     log paths, attempted paths, writability check)
 *   - concatenated tscrt log files (primary + rotated), newest first
 *   - directory listing of the crash-dump folder (names + sizes only,
 *     dumps themselves are kept out unless the user opts in)
 *
 * A time-window filter drops log files whose mtime is older than
 * `daysBack` days (0 disables the filter). A hard byte cap bounds the
 * pre-compression size; if exceeded, the oldest log content is truncated
 * with an explicit marker so support can see the cutoff was applied.
 */
#pragma once

#include <QString>
#include <QStringList>

namespace tscrt {

class DiagnosticExporter
{
public:
    struct Options {
        int    daysBack = 7;              // 0 = include all
        qint64 maxBytes = 10 * 1024 * 1024; // pre-gzip cap
        bool   includeCrashDumpList = true;
    };

    struct Result {
        bool        ok             = false;
        QString     outputPath;
        qint64      originalBytes  = 0;
        qint64      compressedBytes = 0;
        bool        truncated      = false;
        QString     error;
        QStringList warnings;
    };

    /// Build the bundle and write it to `outputPath` (usually ending
    /// in .txt.gz). Safe to call from the GUI thread — runs synchronously
    /// and is fast (<1 s on typical log sizes).
    static Result exportBundle(const QString &outputPath, const Options &opts);
};

} // namespace tscrt
