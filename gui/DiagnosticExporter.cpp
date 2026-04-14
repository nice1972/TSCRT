#include "DiagnosticExporter.h"

#include "Logging.h"
#include "tscrt.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimeZone>

#include <libssh2.h>
#include <vterm.h>
#include <zlib.h>

namespace tscrt {

namespace {

constexpr int kTruncTailBytes = 64 * 1024;  // keep last 64 KB when truncating

QString humanSize(qint64 bytes)
{
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    double kb = bytes / 1024.0;
    if (kb < 1024.0) return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
    double mb = kb / 1024.0;
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 2);
}

void appendSection(QByteArray &buf, const QString &title)
{
    buf.append("\n==== ");
    buf.append(title.toUtf8());
    buf.append(" ====\n");
}

void appendKV(QByteArray &buf, const char *key, const QString &val)
{
    buf.append("  ");
    buf.append(key);
    const int pad = 14 - int(qstrlen(key));
    if (pad > 0) buf.append(QByteArray(pad, ' '));
    buf.append(val.toUtf8());
    buf.append('\n');
}

void writeSystemInfo(QByteArray &buf)
{
    appendSection(buf, QStringLiteral("system info"));
    appendKV(buf, "generated",    QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    appendKV(buf, "timezone",     QTimeZone::systemTimeZone().id());
    appendKV(buf, "version",      QString::fromLatin1(TSCRT_VERSION));
    appendKV(buf, "qt",           QString::fromLatin1(qVersion()));
    appendKV(buf, "libssh2",      QString::fromLatin1(LIBSSH2_VERSION));
    appendKV(buf, "libvterm",
             QStringLiteral("%1.%2").arg(VTERM_VERSION_MAJOR).arg(VTERM_VERSION_MINOR));
    appendKV(buf, "os",           QSysInfo::prettyProductName());
    appendKV(buf, "kernel",       QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion());
    appendKV(buf, "arch",         QSysInfo::currentCpuArchitecture());
    appendKV(buf, "product type", QSysInfo::productType());
    appendKV(buf, "machine",      QSysInfo::machineHostName());
    appendKV(buf, "locale",       QLocale().name());
    appendKV(buf, "ui lang",      QLocale::languageToString(QLocale().language()));
    appendKV(buf, "executable",   QCoreApplication::applicationFilePath());
    appendKV(buf, "app dir",      QCoreApplication::applicationDirPath());
}

void writePaths(QByteArray &buf)
{
    appendSection(buf, QStringLiteral("paths"));
    appendKV(buf, "profile home", QString::fromLocal8Bit(tscrt_get_home()));
    appendKV(buf, "log dir",      currentLogDir());
    appendKV(buf, "log file",     currentLogFile());
    appendKV(buf, "temp",         QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    appendKV(buf, "appdata",      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    appendKV(buf, "cache",        QStandardPaths::writableLocation(QStandardPaths::CacheLocation));

    buf.append("\n  log open attempts:\n");
    const QStringList attempts = logOpenAttempts();
    if (attempts.isEmpty()) {
        buf.append("    (none recorded)\n");
    } else {
        for (const QString &a : attempts) {
            buf.append("    - ");
            buf.append(a.toUtf8());
            buf.append('\n');
        }
    }
}

void writeEnv(QByteArray &buf)
{
    appendSection(buf, QStringLiteral("environment"));
    const auto env = QProcessEnvironment::systemEnvironment();
    const QStringList keys = {
        QStringLiteral("APPDATA"),
        QStringLiteral("LOCALAPPDATA"),
        QStringLiteral("TEMP"),
        QStringLiteral("TMP"),
        QStringLiteral("USERPROFILE"),
        QStringLiteral("HOME"),
        QStringLiteral("LANG"),
        QStringLiteral("LC_ALL"),
        QStringLiteral("QT_QPA_PLATFORM"),
        QStringLiteral("QT_SCALE_FACTOR"),
        QStringLiteral("QT_AUTO_SCREEN_SCALE_FACTOR"),
    };
    for (const QString &k : keys) {
        if (env.contains(k))
            appendKV(buf, k.toLatin1().constData(), env.value(k));
    }
}

void writeDirWritability(QByteArray &buf)
{
    appendSection(buf, QStringLiteral("writability probe"));
    struct Probe { const char *label; QString dir; };
    const Probe probes[] = {
        { "profile home", QString::fromLocal8Bit(tscrt_get_home()) },
        { "log dir",      currentLogDir() },
        { "temp",         QStandardPaths::writableLocation(QStandardPaths::TempLocation) },
    };
    for (const Probe &p : probes) {
        if (p.dir.isEmpty()) {
            appendKV(buf, p.label, QStringLiteral("<empty>"));
            continue;
        }
        const QString probe = QDir(p.dir).filePath(QStringLiteral(".tscrt_diag_probe"));
        QFile f(probe);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("ok");
            f.close();
            QFile::remove(probe);
            appendKV(buf, p.label, QStringLiteral("%1 (writable)").arg(p.dir));
        } else {
            appendKV(buf, p.label, QStringLiteral("%1 (NOT writable: %2)").arg(p.dir, f.errorString()));
        }
    }
}

struct LogFile {
    QString  path;
    QFileInfo info;
};

QList<LogFile> collectLogFiles(const QString &dir, int daysBack,
                               QStringList *warnings)
{
    QList<LogFile> out;
    if (dir.isEmpty())
        return out;
    QDir d(dir);
    if (!d.exists()) {
        if (warnings) warnings->append(QStringLiteral("log dir does not exist: %1").arg(dir));
        return out;
    }
    const QStringList filters = {
        QStringLiteral("tscrt_win.log*"),
        QStringLiteral("tscrt_mac.log*"),
    };
    const QFileInfoList entries =
        d.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Time);
    const QDateTime cutoff = (daysBack > 0)
        ? QDateTime::currentDateTime().addDays(-daysBack)
        : QDateTime();
    for (const QFileInfo &fi : entries) {
        if (cutoff.isValid() && fi.lastModified() < cutoff)
            continue;
        out.append({ fi.absoluteFilePath(), fi });
    }
    return out;
}

void writeCrashDumpList(QByteArray &buf, QStringList *warnings)
{
    appendSection(buf, QStringLiteral("crash dump listing"));
    const QString home = QString::fromLocal8Bit(tscrt_get_home());
    if (home.isEmpty()) {
        buf.append("  (no home)\n");
        return;
    }
    QDir d(home);
    if (!d.cd(QString::fromLatin1(TSCRT_DIR_NAME))) {
        buf.append("  (profile dir not found)\n");
        return;
    }
    // Common crash dump names used by the app / Qt — keep listing
    // permissive since dumps vary by platform.
    const QStringList filters = {
        QStringLiteral("*.dmp"),
        QStringLiteral("crash_*.log"),
        QStringLiteral("*.crash"),
    };
    const QFileInfoList entries =
        d.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Time);
    if (entries.isEmpty()) {
        buf.append("  (no dumps present)\n");
        return;
    }
    for (const QFileInfo &fi : entries) {
        buf.append("  ");
        buf.append(QStringLiteral("%1  %2  %3")
                       .arg(fi.lastModified().toString(Qt::ISODate))
                       .arg(humanSize(fi.size()), 10)
                       .arg(fi.fileName()).toUtf8());
        buf.append('\n');
    }
    if (warnings && !entries.isEmpty())
        warnings->append(QStringLiteral("%1 crash dump(s) present in profile dir — "
                                        "dump contents are NOT included in this bundle")
                             .arg(entries.size()));
}

QByteArray readFileBounded(const QString &path, qint64 maxBytes, bool *truncated)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const qint64 total = f.size();
    if (maxBytes <= 0 || total <= maxBytes) {
        *truncated = false;
        return f.readAll();
    }
    // Keep head + tail with a marker between.
    const qint64 head = maxBytes / 2;
    const qint64 tail = maxBytes - head - 64;
    QByteArray out;
    out.reserve(int(maxBytes));
    out.append(f.read(head));
    out.append("\n\n[...TRUNCATED ");
    out.append(QByteArray::number(total - head - tail));
    out.append(" BYTES...]\n\n");
    f.seek(total - tail);
    out.append(f.read(tail));
    *truncated = true;
    return out;
}

// gzip-compress `in` into `out`. Returns true on success.
bool gzipCompress(const QByteArray &in, QByteArray &out)
{
    z_stream zs{};
    // windowBits = 15 | 16 enables gzip wrapper (standard .gz).
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED,
                     15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    zs.next_in  = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    zs.avail_in = uInt(in.size());

    QByteArray chunk(64 * 1024, Qt::Uninitialized);
    int ret;
    do {
        zs.next_out  = reinterpret_cast<Bytef *>(chunk.data());
        zs.avail_out = uInt(chunk.size());
        ret = deflate(&zs, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            return false;
        }
        out.append(chunk.constData(), int(chunk.size() - zs.avail_out));
    } while (ret != Z_STREAM_END);

    deflateEnd(&zs);
    return true;
}

} // namespace

DiagnosticExporter::Result
DiagnosticExporter::exportBundle(const QString &outputPath, const Options &opts)
{
    Result r;

    QByteArray buf;
    buf.append("TSCRT diagnostic bundle\n");
    buf.append("=======================\n");

    writeSystemInfo(buf);
    writePaths(buf);
    writeEnv(buf);
    writeDirWritability(buf);

    if (opts.includeCrashDumpList)
        writeCrashDumpList(buf, &r.warnings);

    // ---- Log files -------------------------------------------------------
    const QString dir = currentLogDir();
    const QList<LogFile> logs = collectLogFiles(dir, opts.daysBack, &r.warnings);

    appendSection(buf, QStringLiteral("log file list"));
    if (logs.isEmpty()) {
        buf.append("  (no log files found in ");
        buf.append(dir.toUtf8());
        buf.append(")\n");
    } else {
        for (const LogFile &lf : logs) {
            buf.append("  ");
            buf.append(QStringLiteral("%1  %2  %3")
                           .arg(lf.info.lastModified().toString(Qt::ISODate))
                           .arg(humanSize(lf.info.size()), 10)
                           .arg(lf.info.fileName()).toUtf8());
            buf.append('\n');
        }
    }

    // Budget for log content = opts.maxBytes minus what we've already
    // used for metadata sections.
    const qint64 metaBytes = buf.size();
    qint64 remaining = opts.maxBytes - metaBytes;
    if (remaining < 0) remaining = 0;

    for (const LogFile &lf : logs) {
        appendSection(buf, QStringLiteral("log: %1 (%2)")
                              .arg(lf.info.fileName(), humanSize(lf.info.size())));
        if (remaining <= 1024) {
            buf.append("  [cap reached — file omitted]\n");
            r.truncated = true;
            continue;
        }
        bool trunc = false;
        QByteArray contents = readFileBounded(lf.path, remaining, &trunc);
        if (trunc) r.truncated = true;
        remaining -= contents.size();
        buf.append(contents);
        if (!contents.endsWith('\n'))
            buf.append('\n');
    }

    appendSection(buf, QStringLiteral("end"));
    buf.append("Total uncompressed: ");
    buf.append(humanSize(buf.size()).toUtf8());
    buf.append('\n');

    r.originalBytes = buf.size();

    // ---- Compress --------------------------------------------------------
    QByteArray gz;
    if (!gzipCompress(buf, gz)) {
        r.error = QStringLiteral("gzip compression failed");
        return r;
    }
    r.compressedBytes = gz.size();

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        r.error = QStringLiteral("cannot write %1: %2").arg(outputPath, out.errorString());
        return r;
    }
    if (out.write(gz) != gz.size()) {
        r.error = QStringLiteral("short write to %1").arg(outputPath);
        return r;
    }
    out.close();

    r.ok         = true;
    r.outputPath = outputPath;
    return r;
}

} // namespace tscrt
