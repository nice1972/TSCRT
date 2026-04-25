#include "WorkspaceStore.h"

#include "tscrt.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>

namespace tscrt { namespace WorkspaceStore {

QString dir()
{
    const QString base = QString::fromLocal8Bit(tscrt_get_home())
                       + QStringLiteral("/") + QStringLiteral(TSCRT_DIR_NAME)
                       + QStringLiteral("/workspaces");
    QDir().mkpath(base);
    return base;
}

static QString jsonPathFor(const QString &name)
{
    return dir() + QStringLiteral("/") + name + QStringLiteral(".json");
}

static QString lastMarkerPath()
{
    return dir() + QStringLiteral("/_last");
}

QStringList list()
{
    QDir d(dir());
    const QFileInfoList entries = d.entryInfoList(
        QStringList{ QStringLiteral("*.json") }, QDir::Files, QDir::NoSort);
    // Sort by saved_at descending — read each file to extract it. Cheap
    // enough since the dialog only opens on demand and snapshot count
    // stays small in practice.
    struct Row { QString name; qint64 ts; };
    QVector<Row> rows;
    rows.reserve(entries.size());
    for (const QFileInfo &fi : entries) {
        QFile f(fi.absoluteFilePath());
        qint64 ts = fi.lastModified().toMSecsSinceEpoch();
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                const QString iso = doc.object()
                    .value(QStringLiteral("saved_at")).toString();
                const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
                if (dt.isValid()) ts = dt.toMSecsSinceEpoch();
            }
        }
        rows.append({ fi.completeBaseName(), ts });
    }
    std::sort(rows.begin(), rows.end(),
              [](const Row &a, const Row &b) { return a.ts > b.ts; });
    QStringList names;
    names.reserve(rows.size());
    for (const Row &r : rows) names.append(r.name);
    return names;
}

bool exists(const QString &name)
{
    return QFile::exists(jsonPathFor(name));
}

QJsonObject load(const QString &name)
{
    QFile f(jsonPathFor(name));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

bool save(const QString &name, QJsonObject snapshot)
{
    if (name.isEmpty()) return false;
    if (!snapshot.contains(QStringLiteral("name")))
        snapshot[QStringLiteral("name")] = name;
    if (!snapshot.contains(QStringLiteral("saved_at")))
        snapshot[QStringLiteral("saved_at")] =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QFile f(jsonPathFor(name));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    f.write(QJsonDocument(snapshot).toJson(QJsonDocument::Indented));
    return true;
}

bool remove(const QString &name)
{
    const bool ok = QFile::remove(jsonPathFor(name));
    if (lastName() == name) clearLastName();
    return ok;
}

QString lastName()
{
    QFile f(lastMarkerPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    return in.readLine().trimmed();
}

void setLastName(const QString &name)
{
    QFile f(lastMarkerPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << name << '\n';
}

void clearLastName()
{
    QFile::remove(lastMarkerPath());
}

} } // namespace tscrt::WorkspaceStore
