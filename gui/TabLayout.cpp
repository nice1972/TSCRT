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

QStringList load(char role)
{
    QStringList out;
    QFile f(filePath(role));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty())
            out.append(line);
    }
    return out;
}

void save(char role, const QStringList &names)
{
    QFile f(filePath(role));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (const QString &n : names)
        out << n << '\n';
}

void clear(char role)
{
    QFile::remove(filePath(role));
}

} } // namespace tscrt::TabLayout
