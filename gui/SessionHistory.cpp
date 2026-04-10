#include "SessionHistory.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>

namespace tscrt {

namespace {

QString sanitize(const QString &in)
{
    static const QRegularExpression bad(
        QStringLiteral("[\\\\/:*?\"<>|\\s]+"));
    QString out = in;
    out.replace(bad, QStringLiteral("_"));
    if (out.isEmpty()) out = QStringLiteral("session");
    return out;
}

} // namespace

QString historyDirFor(const profile_t &profile)
{
    return QString::fromLocal8Bit(profile.base_dir) +
           QChar(QLatin1Char('/')) + QStringLiteral("history");
}

QString historyPathFor(const profile_t &profile, const QString &sessionName)
{
    return historyDirFor(profile) + QChar(QLatin1Char('/')) +
           sanitize(sessionName) + QStringLiteral(".history");
}

void pruneOrphanHistoryFiles(const profile_t &profile)
{
    const QString dirPath = historyDirFor(profile);
    QDir dir(dirPath);
    if (!dir.exists()) return;

    QSet<QString> alive;
    for (int i = 0; i < profile.session_count; ++i) {
        alive.insert(sanitize(QString::fromLocal8Bit(profile.sessions[i].name)));
    }

    const auto entries = dir.entryList({ QStringLiteral("*.history") },
                                       QDir::Files);
    for (const QString &name : entries) {
        const QString stem = name.left(name.size() - int(qstrlen(".history")));
        if (!alive.contains(stem))
            QFile::remove(dir.filePath(name));
    }
}

} // namespace tscrt
