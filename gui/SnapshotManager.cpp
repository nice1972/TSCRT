#include "SnapshotManager.h"

#include "CronSchedule.h"
#include "Credentials.h"
#include "ISession.h"
#include "SmtpClient.h"
#include "SnapshotRunner.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>

namespace tscrt {

namespace {
constexpr int kTriggerBufMax = 4096;
constexpr int kCronTickMs    = 15000;   // poll every 15 s; cron is minute-granular
} // namespace

SnapshotManager::SnapshotManager(QObject *parent)
    : QObject(parent)
{
    m_cronTimer.setInterval(kCronTickMs);
    connect(&m_cronTimer, &QTimer::timeout,
            this, &SnapshotManager::onCronTick);
    m_cronTimer.start();
}

SnapshotManager::~SnapshotManager() = default;

void SnapshotManager::setProfile(const profile_t &profile)
{
    m_profile = profile;
}

int SnapshotManager::findSnapshotIndex(const QString &name) const
{
    for (int i = 0; i < m_profile.snapshot_count; ++i) {
        if (QString::fromLocal8Bit(m_profile.snapshots[i].name) == name)
            return i;
    }
    return -1;
}

void SnapshotManager::attachSession(ISession *session, const QString &name)
{
    if (!session) return;
    if (m_sessions.contains(session)) return;

    SessionBinding b;
    b.session = session;
    b.name    = name;
    b.triggerBuf.reserve(kTriggerBufMax);
    m_sessions.insert(session, b);

    connect(session, &ISession::connected,
            this, &SnapshotManager::onSessionConnected);
    connect(session, &ISession::bytesReceived,
            this, &SnapshotManager::onSessionBytes);
}

void SnapshotManager::detachSession(ISession *session)
{
    if (!session) return;
    auto it = m_sessions.find(session);
    if (it == m_sessions.end()) return;
    disconnect(session, nullptr, this, nullptr);
    if (it->active)
        it->active->deleteLater();
    m_sessions.erase(it);
}

void SnapshotManager::onSessionConnected()
{
    auto *session = qobject_cast<ISession *>(sender());
    if (!session) return;
    auto it = m_sessions.find(session);
    if (it == m_sessions.end()) return;

    for (int i = 0; i < m_profile.snapshot_rule_count; ++i) {
        const snapshot_rule_t &r = m_profile.snapshot_rules[i];
        if (r.kind != SNAPSHOT_TRIGGER_ON_CONNECT) continue;
        const QString rs = QString::fromLocal8Bit(r.session);
        if (!rs.isEmpty() && rs != it->name) continue;
        const int snapIdx = findSnapshotIndex(QString::fromLocal8Bit(r.snapshot));
        if (snapIdx >= 0) {
            m_lastRuleFire[i] = QDateTime::currentMSecsSinceEpoch();
            enqueue(session, snapIdx);
        }
    }
}

void SnapshotManager::onSessionBytes(const QByteArray &data)
{
    auto *session = qobject_cast<ISession *>(sender());
    if (!session) return;
    auto it = m_sessions.find(session);
    if (it == m_sessions.end()) return;

    bool anyPatternRule = false;
    for (int i = 0; i < m_profile.snapshot_rule_count; ++i)
        if (m_profile.snapshot_rules[i].kind == SNAPSHOT_TRIGGER_PATTERN) {
            anyPatternRule = true; break;
        }
    if (!anyPatternRule) return;

    it->triggerBuf.append(data);
    if (it->triggerBuf.size() > kTriggerBufMax)
        it->triggerBuf.remove(0, it->triggerBuf.size() - kTriggerBufMax);

    for (int i = 0; i < m_profile.snapshot_rule_count; ++i) {
        const snapshot_rule_t &r = m_profile.snapshot_rules[i];
        if (r.kind != SNAPSHOT_TRIGGER_PATTERN) continue;
        const QString rs = QString::fromLocal8Bit(r.session);
        if (!rs.isEmpty() && rs != it->name) continue;
        if (r.pattern[0] == '\0') continue;
        if (!it->triggerBuf.contains(r.pattern)) continue;
        if (shouldCooldownSkip(i)) continue;

        const int snapIdx = findSnapshotIndex(QString::fromLocal8Bit(r.snapshot));
        if (snapIdx >= 0) {
            m_lastRuleFire[i] = QDateTime::currentMSecsSinceEpoch();
            enqueue(session, snapIdx);
        }
        it->triggerBuf.clear();
        break;
    }
}

bool SnapshotManager::shouldCooldownSkip(int ruleIdx)
{
    const snapshot_rule_t &r = m_profile.snapshot_rules[ruleIdx];
    if (r.cooldown_sec <= 0) return false;
    auto it = m_lastRuleFire.find(ruleIdx);
    if (it == m_lastRuleFire.end()) return false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - it.value()) < qint64(r.cooldown_sec) * 1000;
}

void SnapshotManager::onCronTick()
{
    const QDateTime now = QDateTime::currentDateTime();
    // Fire at most once per minute across ticks.
    const int currentMinute = now.time().hour() * 60 + now.time().minute();
    if (currentMinute == m_lastCronMinute) return;
    m_lastCronMinute = currentMinute;

    for (int i = 0; i < m_profile.snapshot_rule_count; ++i) {
        const snapshot_rule_t &r = m_profile.snapshot_rules[i];
        if (r.kind != SNAPSHOT_TRIGGER_CRON) continue;
        if (r.cron_expr[0] == '\0')         continue;

        CronSchedule sched;
        if (!sched.parse(QString::fromLocal8Bit(r.cron_expr))) continue;
        if (!sched.matches(now)) continue;
        if (shouldCooldownSkip(i)) continue;

        const int snapIdx = findSnapshotIndex(QString::fromLocal8Bit(r.snapshot));
        if (snapIdx < 0) continue;

        const QString targetSession = QString::fromLocal8Bit(r.session);
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (!targetSession.isEmpty() && targetSession != it->name) continue;
            m_lastRuleFire[i] = QDateTime::currentMSecsSinceEpoch();
            enqueue(it->session, snapIdx);
        }
    }
}

void SnapshotManager::runSnapshotByName(ISession *session,
                                        const QString &sessionName,
                                        const QString &snapshotName)
{
    if (!session) return;
    if (!m_sessions.contains(session))
        attachSession(session, sessionName);
    const int snapIdx = findSnapshotIndex(snapshotName);
    if (snapIdx < 0) {
        emit message(tr("Snapshot \"%1\" not found").arg(snapshotName));
        return;
    }
    enqueue(session, snapIdx);
}

void SnapshotManager::enqueue(ISession *session, int snapIdx)
{
    auto it = m_sessions.find(session);
    if (it == m_sessions.end()) return;
    it->queue.append(snapIdx);
    if (!it->active)
        dispatchNext(session);
}

void SnapshotManager::dispatchNext(ISession *session)
{
    auto it = m_sessions.find(session);
    if (it == m_sessions.end()) return;
    if (it->active) return;
    if (it->queue.isEmpty()) return;

    const int snapIdx = it->queue.takeFirst();
    if (snapIdx < 0 || snapIdx >= m_profile.snapshot_count)
        return;
    const snapshot_entry_t &snap = m_profile.snapshots[snapIdx];

    auto *runner = new SnapshotRunner(
        session, snap, it->name,
        QString::fromLocal8Bit(m_profile.common.log_dir), this);
    it->active = runner;

    connect(runner, &SnapshotRunner::finished,
            this, &SnapshotManager::onRunnerFinished);

    emit snapshotStarted(it->name, QString::fromLocal8Bit(snap.name));
    emit message(tr("Snapshot \"%1\" started on %2")
                     .arg(QString::fromLocal8Bit(snap.name), it->name));
    runner->start();
}

void SnapshotManager::onRunnerFinished(bool ok, const QString &filePath)
{
    auto *runner = qobject_cast<SnapshotRunner *>(sender());
    if (!runner) return;

    ISession *session = nullptr;
    QString sessionName;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it->active == runner) {
            session = it->session;
            sessionName = it->name;
            it->active = nullptr;
            break;
        }
    }

    const snapshot_entry_t &snap = runner->snapshot();
    emit snapshotFinished(sessionName, QString::fromLocal8Bit(snap.name),
                          ok, filePath);
    if (ok) {
        emit message(tr("Snapshot saved: %1").arg(QFileInfo(filePath).fileName()));
        if (snap.send_email && snap.recipient_count > 0)
            deliverEmail(snap, sessionName, filePath);
    } else {
        emit message(tr("Snapshot failed: %1")
                         .arg(QString::fromLocal8Bit(snap.name)));
    }

    runner->deleteLater();
    if (session)
        dispatchNext(session);
}

void SnapshotManager::deliverEmail(const snapshot_entry_t &snap,
                                   const QString &sessionName,
                                   const QString &filePath)
{
    if (m_profile.smtp.host[0] == '\0') {
        emit message(tr("SMTP host not configured — email skipped"));
        return;
    }

    QStringList recipients;
    for (int i = 0; i < snap.recipient_count; ++i)
        recipients << QString::fromLocal8Bit(snap.recipients[i]);
    if (recipients.isEmpty()) return;

    const QString stamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    QString subject = QString::fromLocal8Bit(snap.subject_tmpl);
    if (subject.isEmpty())
        subject = QStringLiteral("TSCRT {session} {snapshot} {timestamp}");
    subject.replace(QStringLiteral("{session}"),  sessionName);
    subject.replace(QStringLiteral("{snapshot}"),
                    QString::fromLocal8Bit(snap.name));
    subject.replace(QStringLiteral("{timestamp}"), stamp);

    QFile f(filePath);
    QString body;
    if (f.open(QIODevice::ReadOnly)) {
        body = QString::fromUtf8(f.readAll());
        f.close();
    }

    auto *smtp = new SmtpClient(m_profile.smtp, this);
    connect(smtp, &SmtpClient::sent, this, [this, subject, smtp] {
        emit message(tr("Email sent: %1").arg(subject));
        smtp->deleteLater();
    });
    connect(smtp, &SmtpClient::failed, this,
            [this, smtp](const QString &reason) {
        emit message(tr("Email failed: %1").arg(reason));
        smtp->deleteLater();
    });

    const QString attach = snap.attach_file ? filePath : QString();
    // When attaching, put a short summary in the body. When not attaching,
    // send the full capture inline.
    const QString textBody = snap.attach_file
        ? QStringLiteral("TSCRT snapshot %1 from session %2 at %3.\n\n"
                         "See the attached file for full output.\n")
              .arg(QString::fromLocal8Bit(snap.name), sessionName, stamp)
        : body;
    smtp->send(recipients, subject, textBody, attach);
}

} // namespace tscrt
