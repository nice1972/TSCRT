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
#include <QTextStream>
#include <QThread>

#include <libssh2.h>
#include <vterm.h>

namespace tscrt {

namespace {

constexpr qint64 kMaxBytes     = 5 * 1024 * 1024; // 5 MB
constexpr int    kMaxRotations = 5;

QMutex            g_mutex;
QFile            *g_file    = nullptr;
QtMessageHandler  g_prev    = nullptr;

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

    // Drop oldest, shift the rest
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

} // namespace

QString installLogging()
{
    QMutexLocker lock(&g_mutex);
    if (g_file)
        return g_file->fileName();

    // Resolve %APPDATA%/tscrt/logs (matches profile_init() layout).
    QString base = QString::fromLocal8Bit(tscrt_get_home());
    QDir dir(base);
    if (!dir.cd(QString::fromLatin1(TSCRT_DIR_NAME))) {
        if (!dir.mkpath(QString::fromLatin1(TSCRT_DIR_NAME)))
            return {};
        dir.cd(QString::fromLatin1(TSCRT_DIR_NAME));
    }
    if (!dir.cd(QString::fromLatin1(TSCRT_LOG_DIR_NAME))) {
        if (!dir.mkpath(QString::fromLatin1(TSCRT_LOG_DIR_NAME)))
            return {};
        dir.cd(QString::fromLatin1(TSCRT_LOG_DIR_NAME));
    }

    const QString path = dir.filePath(QStringLiteral("tscrt_win.log"));
    rotateIfNeeded(path);

    auto *f = new QFile(path);
    if (!f->open(QIODevice::Append | QIODevice::Text)) {
        delete f;
        return {};
    }
    g_file = f;
    g_prev = qInstallMessageHandler(&messageHandler);
    return path;
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
}

void logBanner()
{
    qInfo("tscrt_win %s starting (Qt %s, libssh2 %s, libvterm %d.%d)",
          TSCRT_VERSION,
          qVersion(),
          LIBSSH2_VERSION,
          VTERM_VERSION_MAJOR,
          VTERM_VERSION_MINOR);
}

} // namespace tscrt
