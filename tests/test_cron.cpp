/*
 * Unit tests for CronSchedule - verifies five-field cron expressions
 * fire only when expected.
 */
#include "CronSchedule.h"

#include <QDateTime>
#include <QTest>

class TestCron : public QObject {
    Q_OBJECT

private slots:
    void wildcardMatchesAnyMinute();
    void literalMinute();
    void rangeHour();
    void stepMinute();
    void everyWeekday();
    void invalidExprs();
    void dowSunday();
};

static QDateTime dt(int y, int mo, int d, int h, int mi)
{
    return QDateTime(QDate(y, mo, d), QTime(h, mi, 0));
}

void TestCron::wildcardMatchesAnyMinute()
{
    tscrt::CronSchedule c;
    QVERIFY(c.parse(QStringLiteral("* * * * *")));
    QVERIFY(c.matches(dt(2026, 4, 11, 0, 0)));
    QVERIFY(c.matches(dt(2026, 4, 11, 13, 37)));
}

void TestCron::literalMinute()
{
    tscrt::CronSchedule c;
    QVERIFY(c.parse(QStringLiteral("30 * * * *")));
    QVERIFY(c.matches(dt(2026, 4, 11, 0, 30)));
    QVERIFY(c.matches(dt(2026, 4, 11, 13, 30)));
    QVERIFY(!c.matches(dt(2026, 4, 11, 13, 31)));
}

void TestCron::rangeHour()
{
    tscrt::CronSchedule c;
    QVERIFY(c.parse(QStringLiteral("0 9-17 * * *")));
    QVERIFY(c.matches(dt(2026, 4, 11, 9, 0)));
    QVERIFY(c.matches(dt(2026, 4, 11, 17, 0)));
    QVERIFY(!c.matches(dt(2026, 4, 11, 8, 0)));
    QVERIFY(!c.matches(dt(2026, 4, 11, 18, 0)));
    QVERIFY(!c.matches(dt(2026, 4, 11, 10, 5)));
}

void TestCron::stepMinute()
{
    tscrt::CronSchedule c;
    QVERIFY(c.parse(QStringLiteral("*/5 * * * *")));
    QVERIFY(c.matches(dt(2026, 4, 11, 10, 0)));
    QVERIFY(c.matches(dt(2026, 4, 11, 10, 5)));
    QVERIFY(c.matches(dt(2026, 4, 11, 10, 55)));
    QVERIFY(!c.matches(dt(2026, 4, 11, 10, 1)));
    QVERIFY(!c.matches(dt(2026, 4, 11, 10, 13)));
}

void TestCron::everyWeekday()
{
    // Monday = 1 ... Friday = 5
    tscrt::CronSchedule c;
    QVERIFY(c.parse(QStringLiteral("0 3 * * 1-5")));
    // 2026-04-13 is a Monday
    QVERIFY(c.matches(dt(2026, 4, 13, 3, 0)));
    // 2026-04-11 is a Saturday
    QVERIFY(!c.matches(dt(2026, 4, 11, 3, 0)));
}

void TestCron::invalidExprs()
{
    tscrt::CronSchedule c;
    QVERIFY(!c.parse(QString()));
    QVERIFY(!c.parse(QStringLiteral("* * * *")));         // too few fields
    QVERIFY(!c.parse(QStringLiteral("60 * * * *")));       // minute overflow
    QVERIFY(!c.parse(QStringLiteral("0 24 * * *")));       // hour overflow
    QVERIFY(!c.parse(QStringLiteral("0 0 32 * *")));       // day overflow
    QVERIFY(!c.parse(QStringLiteral("0 0 1 13 *")));       // month overflow
    QVERIFY(!c.parse(QStringLiteral("0 0 1 * 7")));        // dow overflow (0..6)
    QVERIFY(!c.parse(QStringLiteral("abc * * * *")));
}

void TestCron::dowSunday()
{
    tscrt::CronSchedule c;
    QVERIFY(c.parse(QStringLiteral("0 0 * * 0")));
    // 2026-04-12 is a Sunday
    QVERIFY(c.matches(dt(2026, 4, 12, 0, 0)));
    QVERIFY(!c.matches(dt(2026, 4, 13, 0, 0)));
}

QTEST_APPLESS_MAIN(TestCron)
#include "test_cron.moc"
