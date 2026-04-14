#include "Logging.h"

#include "tscrt.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>

#include <libssh2.h>
#include <vterm.h>

namespace tscrt {

namespace {

constexpr qint64 kMaxBytes     = 5 * 1024 * 1024; // 5 MB
constexpr int    kMaxRotations = 5;

QMutex            g_mutex;
QFile            *g_file      = nullptr;
QtMessageHandler  g_prev      = nullptr;
QString           g_logDir;
QString           g_logPath;
QStringList       g_attempts;

QString levelTag(QtMsgType t)
{
    switch (t) {
    case QtDebugMsg:    return QStringLiteral("DEBUG");
    case QtInfoMsg:     return QStringLiteral("INFO ");
    case QtWarningMsg:  return QStringLiteral("WARN ");
    case QtCriticalMsg: return QStringLiteral("ERROR");
    case QtFatalMsg:    return QStringLiteral("FATAL");
    }
    return QStringLiteral("?    ");
}

void rotateIfNeeded(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists() || info.size() < kMaxBytes)
        return;

    const QString last = QStringLiteral("%1.%2").arg(path).arg(kMaxRotations);
    if (QFile::exists(last))
        QFile::remove(last);
    for (int i = kMaxRotations - 1; i >= 1; --i) {
        const QString from = QStringLiteral("%1.%2").arg(path).arg(i);
        const QString to   = QStringLiteral("%1.%2").arg(path).arg(i + 1);
        if (QFile::exists(from))
            QFile::rename(from, to);
    }
    QFile::rename(path, QStringLiteral("%1.1").arg(path));
}

void messageHandler(QtMsgType type, const QMessageLogContext &ctx,
                    const QString &msg)
{
    QMutexLocker lock(&g_mutex);

    if (g_file && g_file->isOpen()) {
        QTextStream out(g_file);
        out.setEncoding(QStringConverter::Utf8);
        out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
            << " [" << levelTag(type) << "] "
            << "tid=" << reinterpret_cast<quintptr>(QThread::currentThreadId())
            << "  " << msg;
        if (ctx.file && *ctx.file)
            out << "  (" << ctx.file << ':' << ctx.line << ')';
        out << '\n';
        out.flush();
    }

    if (g_prev)
        g_prev(type, ctx, msg);
}

// Try to open log file at <base>/tscrt/logs/<name>.log. Returns opened
// QFile* on success (caller owns), nullptr on failure; reason written to
// `why`.
// Try to open fileName for append inside `dir`. Creates `dir` recursively
// if missing. On failure, writes reason to `why`. Returns opened QFile*
// (caller owns) or nullptr.
QFile *tryOpenAt(const QString &dir, const QString &fileName, QString *why)
{
    if (dir.isEmpty()) {
        if (why) *why = QStringLiteral("empty dir path");
        return nullptr;
    }
    QDir d(dir);
    if (!d.exists()) {
        if (!d.mkpath(QStringLiteral("."))) {
            if (why) *why = QStringLiteral("mkpath failed");
            return nullptr;
        }
    }

    const QString path = d.filePath(fileName);
    rotateIfNeeded(path);

    auto *f = new QFile(path);
    if (!f->open(QIODevice::Append | QIODevice::Text)) {
        if (why) *why = QStringLiteral("open failed: %1").arg(f->errorString());
        delete f;
        return nullptr;
    }
    return f;
}

void writeBootDiagnostic()
{
    // Written before qInstallMessageHandler, so writes go via qInfo through
    // the default handler AND this routine writes a copy directly to the
    // file so that even if some later component breaks, the environment
    // snapshot survives. Called with g_mutex NOT held.
    QMutexLocker lock(&g_mutex);
    if (!g_file || !g_file->isOpen())
        return;
    QTextStream out(g_file);
    out.setEncoding(QStringConverter::Utf8);
    out << "==== boot diagnostic ====\n"
        << "  time:        " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "\n"
        << "  version:     " << TSCRT_VERSION << "\n"
        << "  qt:          " << qVersion() << "\n"
        << "  os:          " << QSysInfo::prettyProductName() << "\n"
        << "  kernel:      " << QSysInfo::kernelType() << " " << QSysInfo::kernelVersion() << "\n"
        << "  arch:        " << QSysInfo::currentCpuArchitecture() << "\n"
        << "  machine:     " << QSysInfo::machineHostName() << "\n"
        << "  log dir:     " << g_logDir << "\n";
    if (!g_attempts.isEmpty()) {
        out << "  attempts:\n";
        for (const QString &a : g_attempts)
            out << "    - " << a << "\n";
    }
    out << "========================\n";
    out.flush();
}

} // namespace

QString installLogging()
{
    QMutexLocker lock(&g_mutex);
    if (g_file)
        return g_file->fileName();

#if defined(Q_OS_MACOS)
    const QString fileName = QStringLiteral("tscrt_mac.log");
#elif defined(Q_OS_LINUX)
    const QString fileName = QStringLiteral("tscrt_linux.log");
#else
    const QString fileName = QStringLiteral("tscrt_win.log");
#endif

    struct Candidate { QString label; QString dir; };
    QList<Candidate> candidates;

    // Primary: <profile home>/tscrt/logs. tscrt_get_home() resolves to
    // %APPDATA% on Windows, ~/Library/Application Support on macOS,
    // ~/.config on Linux.
    const QString home = QString::fromLocal8Bit(tscrt_get_home());
    if (!home.isEmpty()) {
        candidates.append({ QStringLiteral("profile"),
                            QDir(home).filePath(QStringLiteral("%1/%2")
                                .arg(QString::fromLatin1(TSCRT_DIR_NAME),
                                     QString::fromLatin1(TSCRT_LOG_DIR_NAME))) });
    }

    // Fallback: a visible top-level directory the user can find easily
    // when the profile home is blocked by OneDrive/EDR/ACLs. Deliberately
    // NOT under AppData — users need to see it.
#ifdef Q_OS_WIN
    candidates.append({ QStringLiteral("toplevel"),
                        QStringLiteral("C:/TSCRT/logs") });
#else
    candidates.append({ QStringLiteral("toplevel"),
                        QDir::homePath() + QStringLiteral("/TSCRT/logs") });
#endif

    // Last resort: OS TEMP. Almost always writable; disposable.
    candidates.append({ QStringLiteral("temp"),
                        QStandardPaths::writableLocation(QStandardPaths::TempLocation) });

    QFile *opened = nullptr;
    for (const Candidate &c : candidates) {
        if (c.dir.isEmpty()) continue;
        QString why;
        QFile *f = tryOpenAt(c.dir, fileName, &why);
        if (f) {
            opened = f;
            g_attempts.append(QStringLiteral("%1 [%2] — opened").arg(c.dir, c.label));
            break;
        }
        g_attempts.append(QStringLiteral("%1 [%2] — %3").arg(c.dir, c.label, why));
    }

    if (!opened) {
        // Nothing worked; leave g_file null. Attempts list preserved for
        // later diagnostic export.
        return {};
    }

    g_file    = opened;
    g_logPath = opened->fileName();
    g_logDir  = QFileInfo(g_logPath).absolutePath();
    g_prev    = qInstallMessageHandler(&messageHandler);
    lock.unlock();

    writeBootDiagnostic();
    return g_logPath;
}

void shutdownLogging()
{
    QMutexLocker lock(&g_mutex);
    if (g_prev) {
        qInstallMessageHandler(g_prev);
        g_prev = nullptr;
    }
    if (g_file) {
        g_file->close();
        delete g_file;
        g_file = nullptr;
    }
    g_logPath.clear();
    g_logDir.clear();
}

void logBanner()
{
#if defined(Q_OS_MACOS)
    qInfo("tscrt_mac %s starting (Qt %s, libssh2 %s, libvterm %d.%d)",
#elif defined(Q_OS_LINUX)
    qInfo("tscrt_linux %s starting (Qt %s, libssh2 %s, libvterm %d.%d)",
#else
    qInfo("tscrt_win %s starting (Qt %s, libssh2 %s, libvterm %d.%d)",
#endif
          TSCRT_VERSION,
          qVersion(),
          LIBSSH2_VERSION,
          VTERM_VERSION_MAJOR,
          VTERM_VERSION_MINOR);
}

QString currentLogDir()
{
    QMutexLocker lock(&g_mutex);
    return g_logDir;
}

QString currentLogFile()
{
    QMutexLocker lock(&g_mutex);
    return g_logPath;
}

QStringList logOpenAttempts()
{
    QMutexLocker lock(&g_mutex);
    return g_attempts;
}

} // namespace tscrt
