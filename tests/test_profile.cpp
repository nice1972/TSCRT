/*
 * Unit tests for profile.c - verifies INI load/save round-trip and
 * default initialization. Uses a temporary HOME via APPDATA env var so
 * the user's real config is never touched.
 */
#include "profile.h"
#include "tscrt.h"

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TestProfile : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void initialize();
    void roundTripSshSession();
    void roundTripSerialSession();
    void buttonsPersist();
    void perSessionAutomation();
    void snapshotRoundTrip();

private:
    QTemporaryDir m_tmp;
    QString       m_savedAppdata;
};

void TestProfile::initTestCase()
{
    QVERIFY(m_tmp.isValid());
#ifdef _WIN32
    m_savedAppdata = qEnvironmentVariable("APPDATA");
    qputenv("APPDATA", m_tmp.path().toLocal8Bit());
#else
    m_savedAppdata = qEnvironmentVariable("HOME");
    qputenv("HOME", m_tmp.path().toLocal8Bit());
#endif
}

void TestProfile::cleanupTestCase()
{
    if (!m_savedAppdata.isEmpty())
#ifdef _WIN32
        qputenv("APPDATA", m_savedAppdata.toLocal8Bit());
#else
        qputenv("HOME", m_savedAppdata.toLocal8Bit());
#endif
}

void TestProfile::initialize()
{
    profile_t p;
    QCOMPARE(profile_init(&p), 0);
    QCOMPARE(p.session_count, 0);
    QVERIFY(p.common.scrollback > 0);
    QCOMPARE(QString::fromLatin1(p.common.terminal_type),
             QStringLiteral("xterm-256color"));
}

void TestProfile::roundTripSshSession()
{
    profile_t p;
    QCOMPARE(profile_init(&p), 0);

    session_entry_t s{};
    s.type = SESSION_TYPE_SSH;
    qstrncpy(s.name,         "router-1", sizeof(s.name));
    qstrncpy(s.ssh.host,     "192.168.1.1", sizeof(s.ssh.host));
    qstrncpy(s.ssh.username, "admin",       sizeof(s.ssh.username));
    qstrncpy(s.ssh.password, "secret",      sizeof(s.ssh.password));
    s.ssh.port = 2222;
    QCOMPARE(profile_add_session(&p, &s), 0);

    profile_t loaded;
    QCOMPARE(profile_init(&loaded), 0);
    QCOMPARE(profile_load(&loaded), 0);
    QCOMPARE(loaded.session_count, 1);
    QCOMPARE(loaded.sessions[0].type, SESSION_TYPE_SSH);
    QCOMPARE(QString::fromLatin1(loaded.sessions[0].name),
             QStringLiteral("router-1"));
    QCOMPARE(loaded.sessions[0].ssh.port, 2222);
}

void TestProfile::roundTripSerialSession()
{
    profile_t p;
    QCOMPARE(profile_init(&p), 0);
    p.session_count = 0;

    session_entry_t s{};
    s.type = SESSION_TYPE_SERIAL;
    qstrncpy(s.name,          "console-A", sizeof(s.name));
    qstrncpy(s.serial.device, "COM3",      sizeof(s.serial.device));
    s.serial.baudrate    = 115200;
    s.serial.databits    = 8;
    s.serial.stopbits    = 1;
    s.serial.parity      = PARITY_NONE;
    s.serial.flowcontrol = FLOW_NONE;
    QCOMPARE(profile_add_session(&p, &s), 0);

    profile_t loaded;
    QCOMPARE(profile_init(&loaded), 0);
    QCOMPARE(profile_load(&loaded), 0);
    bool found = false;
    for (int i = 0; i < loaded.session_count; ++i) {
        if (QString::fromLatin1(loaded.sessions[i].name) == QStringLiteral("console-A")) {
            QCOMPARE(loaded.sessions[i].type, SESSION_TYPE_SERIAL);
            QCOMPARE(loaded.sessions[i].serial.baudrate, 115200);
            found = true;
        }
    }
    QVERIFY(found);
}

void TestProfile::buttonsPersist()
{
    profile_t p;
    QCOMPARE(profile_init(&p), 0);
    // Use a label unlikely to collide with profile_init defaults so the
    // round-trip check looks at exactly the slot we wrote.
    qstrncpy(p.buttons[15].label,  "rt_unique",       sizeof(p.buttons[15].label));
    qstrncpy(p.buttons[15].action, "echo unique\\r",  sizeof(p.buttons[15].action));
    QCOMPARE(profile_save(&p), 0);

    profile_t loaded;
    QCOMPARE(profile_init(&loaded), 0);
    QCOMPARE(profile_load(&loaded), 0);
    bool found = false;
    for (int i = 0; i < MAX_BUTTONS; ++i) {
        if (QString::fromLatin1(loaded.buttons[i].label) == QStringLiteral("rt_unique")) {
            QCOMPARE(QString::fromLatin1(loaded.buttons[i].action),
                     QStringLiteral("echo unique\\r"));
            found = true;
        }
    }
    QVERIFY(found);
}

void TestProfile::perSessionAutomation()
{
    profile_t p;
    QCOMPARE(profile_init(&p), 0);
    p.session_count = 0;

    // Add a session and one of each automation kind
    session_entry_t s{};
    s.type = SESSION_TYPE_SSH;
    qstrncpy(s.name, "auto-test", sizeof(s.name));
    qstrncpy(s.ssh.host, "h", sizeof(s.ssh.host));
    qstrncpy(s.ssh.username, "u", sizeof(s.ssh.username));
    s.ssh.port = 22;
    profile_add_session(&p, &s);

    qstrncpy(p.startups[0].session, "auto-test", sizeof(p.startups[0].session));
    qstrncpy(p.startups[0].name,    "init",      sizeof(p.startups[0].name));
    qstrncpy(p.startups[0].action,  "uname -a\\r", sizeof(p.startups[0].action));
    p.startup_count = 1;

    qstrncpy(p.triggers[0].session, "auto-test", sizeof(p.triggers[0].session));
    qstrncpy(p.triggers[0].pattern, "login:",    sizeof(p.triggers[0].pattern));
    qstrncpy(p.triggers[0].action,  "admin\\r",  sizeof(p.triggers[0].action));
    p.trigger_count = 1;

    qstrncpy(p.periodics[0].session, "auto-test", sizeof(p.periodics[0].session));
    qstrncpy(p.periodics[0].name,    "keepalive", sizeof(p.periodics[0].name));
    qstrncpy(p.periodics[0].action,  "\\r",       sizeof(p.periodics[0].action));
    p.periodics[0].interval = 60;
    p.periodic_count = 1;

    QCOMPARE(profile_save(&p), 0);

    profile_t loaded;
    QCOMPARE(profile_init(&loaded), 0);
    QCOMPARE(profile_load(&loaded), 0);
    QCOMPARE(loaded.startup_count,  1);
    QCOMPARE(loaded.trigger_count,  1);
    QCOMPARE(loaded.periodic_count, 1);
    QCOMPARE(loaded.periodics[0].interval, 60);
    QCOMPARE(QString::fromLatin1(loaded.startups[0].name),
             QStringLiteral("init"));
}

void TestProfile::snapshotRoundTrip()
{
    profile_t p;
    QCOMPARE(profile_init(&p), 0);
    p.session_count = 0;

    // SMTP
    qstrncpy(p.smtp.host,      "smtp.example.com", sizeof(p.smtp.host));
    p.smtp.port     = 587;
    p.smtp.security = 1;
    qstrncpy(p.smtp.username,  "alerts@example.com", sizeof(p.smtp.username));
    qstrncpy(p.smtp.from_addr, "alerts@example.com", sizeof(p.smtp.from_addr));
    qstrncpy(p.smtp.from_name, "TSCRT Alerts",       sizeof(p.smtp.from_name));
    p.smtp.timeout_sec = 45;

    // One snapshot with two commands and two recipients.
    snapshot_entry_t &s = p.snapshots[p.snapshot_count++];
    memset(&s, 0, sizeof(s));
    qstrncpy(s.name,         "ap_health",              sizeof(s.name));
    qstrncpy(s.description,  "collect ap diagnostics", sizeof(s.description));
    qstrncpy(s.subject_tmpl, "TSCRT {snapshot}",       sizeof(s.subject_tmpl));
    s.send_email = 1;
    s.attach_file = 1;
    qstrncpy(s.recipients[0], "ops@example.com",   MAX_EMAIL_LEN);
    qstrncpy(s.recipients[1], "noc@example.com",   MAX_EMAIL_LEN);
    s.recipient_count = 2;

    qstrncpy(s.cmds[0].command,       "show version\\r", sizeof(s.cmds[0].command));
    s.cmds[0].delay_ms     = 500;
    qstrncpy(s.cmds[0].expect_prompt, "#\\s*$",           sizeof(s.cmds[0].expect_prompt));
    s.cmds[0].max_wait_ms  = 5000;

    qstrncpy(s.cmds[1].command,       "show ip interface brief\\r",
             sizeof(s.cmds[1].command));
    s.cmds[1].delay_ms     = 1000;
    s.cmds[1].max_wait_ms  = 0;
    s.cmd_count = 2;

    // One cron rule firing nightly at 03:00.
    snapshot_rule_t &r = p.snapshot_rules[p.snapshot_rule_count++];
    memset(&r, 0, sizeof(r));
    r.kind = 1;
    qstrncpy(r.session,   "",            sizeof(r.session));
    qstrncpy(r.snapshot,  "ap_health",   sizeof(r.snapshot));
    qstrncpy(r.cron_expr, "0 3 * * *",   sizeof(r.cron_expr));
    r.cooldown_sec = 60;

    QCOMPARE(profile_save(&p), 0);

    profile_t loaded;
    QCOMPARE(profile_init(&loaded), 0);
    QCOMPARE(profile_load(&loaded), 0);

    QCOMPARE(loaded.smtp.port, 587);
    QCOMPARE(loaded.smtp.security, 1);
    QCOMPARE(loaded.smtp.timeout_sec, 45);
    QCOMPARE(QString::fromLatin1(loaded.smtp.host),
             QStringLiteral("smtp.example.com"));

    QCOMPARE(loaded.snapshot_count, 1);
    const snapshot_entry_t &ls = loaded.snapshots[0];
    QCOMPARE(QString::fromLatin1(ls.name), QStringLiteral("ap_health"));
    QCOMPARE(ls.cmd_count, 2);
    QCOMPARE(ls.cmds[0].delay_ms, 500);
    QCOMPARE(ls.cmds[0].max_wait_ms, 5000);
    QCOMPARE(QString::fromLatin1(ls.cmds[0].expect_prompt),
             QStringLiteral("#\\s*$"));
    QCOMPARE(ls.recipient_count, 2);
    QCOMPARE(QString::fromLatin1(ls.recipients[1]),
             QStringLiteral("noc@example.com"));
    QCOMPARE(ls.send_email, 1);
    QCOMPARE(ls.attach_file, 1);

    QCOMPARE(loaded.snapshot_rule_count, 1);
    QCOMPARE(loaded.snapshot_rules[0].kind, 1);
    QCOMPARE(QString::fromLatin1(loaded.snapshot_rules[0].cron_expr),
             QStringLiteral("0 3 * * *"));
    QCOMPARE(loaded.snapshot_rules[0].cooldown_sec, 60);
}

QTEST_APPLESS_MAIN(TestProfile)
#include "test_profile.moc"
