#include "TabLayout.h"

#include "tscrt.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace tscrt { namespace TabLayout {

QString filePath(char role)
{
    const QString base = QString::fromLocal8Bit(tscrt_get_home())
                       + QStringLiteral("/") + QStringLiteral(TSCRT_DIR_NAME);
    QDir().mkpath(base);
    return base + QStringLiteral("/layout_") + QChar::fromLatin1(role)
                + QStringLiteral(".lst");
}

QVector<Entry> loadEntries(char role)
{
    QVector<Entry> out;
    QFile f(filePath(role));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.trimmed().isEmpty()) continue;
        const int tab = line.indexOf(QLatin1Char('\t'));
        if (tab < 0) {
            out.append({ line.trimmed(), QString() });
        } else {
            out.append({ line.left(tab), line.mid(tab + 1) });
        }
    }
    return out;
}

QStringList load(char role)
{
    QStringList names;
    for (const Entry &e : loadEntries(role)) names.append(e.first);
    return names;
}

void saveEntries(char role, const QVector<Entry> &entries)
{
    QFile f(filePath(role));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (const Entry &e : entries) {
        if (e.second.isEmpty() || e.second == e.first)
            out << e.first << '\n';
        else
            out << e.first << '\t' << e.second << '\n';
    }
}

void save(char role, const QStringList &names)
{
    QVector<Entry> entries;
    entries.reserve(names.size());
    for (const QString &n : names) entries.append({ n, QString() });
    saveEntries(role, entries);
}

void clear(char role)
{
    QFile::remove(filePath(role));
}

} } // namespace tscrt::TabLayout
