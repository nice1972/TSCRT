#include "CronSchedule.h"

namespace tscrt {

bool CronSchedule::parseField(const QString &field, int lo, int hi,
                              QVector<bool> &out)
{
    const int size = hi - lo + 1;
    out.fill(false, size);

    const QStringList items = field.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (items.isEmpty()) return false;

    for (const QString &rawItem : items) {
        QString item = rawItem.trimmed();
        int step = 1;

        // "*/N" or "a-b/N"
        const int slash = item.indexOf(QLatin1Char('/'));
        if (slash >= 0) {
            bool ok = false;
            step = item.mid(slash + 1).toInt(&ok);
            if (!ok || step <= 0) return false;
            item = item.left(slash);
        }

        int a, b;
        if (item == QLatin1String("*")) {
            a = lo; b = hi;
        } else if (item.contains(QLatin1Char('-'))) {
            const int dash = item.indexOf(QLatin1Char('-'));
            bool ok1 = false, ok2 = false;
            a = item.left(dash).toInt(&ok1);
            b = item.mid(dash + 1).toInt(&ok2);
            if (!ok1 || !ok2) return false;
        } else {
            bool ok = false;
            a = item.toInt(&ok);
            if (!ok) return false;
            b = a;
        }

        if (a < lo || b > hi || a > b) return false;

        for (int v = a; v <= b; v += step) {
            out[v - lo] = true;
        }
    }
    return true;
}

bool CronSchedule::parse(const QString &expr)
{
    m_valid = false;
    const QStringList f = expr.simplified().split(QLatin1Char(' '),
                                                  Qt::SkipEmptyParts);
    if (f.size() != 5) return false;

    if (!parseField(f[0], 0, 59, m_minute)) return false;
    if (!parseField(f[1], 0, 23, m_hour))   return false;
    if (!parseField(f[2], 1, 31, m_dom))    return false;
    if (!parseField(f[3], 1, 12, m_mon))    return false;
    if (!parseField(f[4], 0, 6,  m_dow))    return false;

    m_valid = true;
    return true;
}

bool CronSchedule::matches(const QDateTime &dt) const
{
    if (!m_valid) return false;
    const QDate d = dt.date();
    const QTime t = dt.time();
    // Qt weekday: 1 = Monday ... 7 = Sunday.  Cron: 0 = Sunday ... 6 = Saturday.
    const int qtDow = d.dayOfWeek();
    const int cronDow = (qtDow == 7) ? 0 : qtDow;
    return m_minute.at(t.minute())
        && m_hour.at(t.hour())
        && m_dom.at(d.day() - 1)
        && m_mon.at(d.month() - 1)
        && m_dow.at(cronDow);
}

} // namespace tscrt
