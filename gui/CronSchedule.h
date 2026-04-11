/*
 * CronSchedule - minimal 5-field cron expression matcher.
 *
 * Fields (in order): minute hour day-of-month month day-of-week
 *   minute       0-59
 *   hour         0-23
 *   day-of-month 1-31
 *   month        1-12
 *   day-of-week  0-6  (0 = Sunday)
 *
 * Each field supports:
 *   "*"          wildcard
 *   "N"          literal
 *   "N-M"        range
 *   "N,M,..."    list of literals or ranges
 *   "*\/N"       every N (step)
 *
 * Intended to be called once per minute tick.
 */
#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

namespace tscrt {

class CronSchedule {
public:
    CronSchedule() = default;

    /// Parse a five-field expression. Returns false on syntax error.
    bool parse(const QString &expr);

    /// True if the expression fires at the given minute (seconds ignored).
    bool matches(const QDateTime &dt) const;

    bool isValid() const { return m_valid; }

private:
    // For each field, a boolean mask the size of the field's range.
    QVector<bool> m_minute;   // 60
    QVector<bool> m_hour;     // 24
    QVector<bool> m_dom;      // 31 (index 0 = day 1)
    QVector<bool> m_mon;      // 12 (index 0 = month 1)
    QVector<bool> m_dow;      // 7  (index 0 = Sunday)
    bool          m_valid = false;

    static bool parseField(const QString &field, int lo, int hi,
                           QVector<bool> &out);
};

} // namespace tscrt
