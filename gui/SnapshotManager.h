/*
 * SnapshotManager - owns snapshot rules + running jobs across all session
 * tabs. Created by MainWindow; notified as tabs are opened and closed
 * via attach() / detach().
 *
 * Rule kinds:
 *   ON_CONNECT : fires once when the session emits connected()
 *   CRON       : minute-ticker checks all cron rules
 *   PATTERN    : ring-buffer of incoming bytes per session, substring match
 *
 * Per-session concurrency: at most one SnapshotRunner active. Incoming
 * requests for the same session are queued FIFO.
 */
#pragma once

#include "tscrt.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class ISession;

namespace tscrt {

class SnapshotRunner;
class SmtpClient;

struct SessionBinding {
    ISession *session = nullptr;
    QString   name;
    QByteArray triggerBuf;     // tail buffer for pattern match
    QList<int> queue;          // pending snapshot indices for this session
    QPointer<SnapshotRunner> active;
};

class SnapshotManager : public QObject {
    Q_OBJECT
public:
    explicit SnapshotManager(QObject *parent = nullptr);
    ~SnapshotManager() override;

    void setProfile(const profile_t &profile);
    const profile_t &profile() const { return m_profile; }

    /// Hook a newly-created session into the manager.
    void attachSession(ISession *session, const QString &name);

    /// Called by tabs on close.
    void detachSession(ISession *session);

    /// Run the named snapshot on the given session now (queued if another
    /// snapshot is already running on that session).
    void runSnapshotByName(ISession *session, const QString &sessionName,
                           const QString &snapshotName);

signals:
    void snapshotStarted(const QString &sessionName, const QString &snapshot);
    void snapshotFinished(const QString &sessionName, const QString &snapshot,
                          bool ok, const QString &filePath);
    void message(const QString &text);   // status bar hook

private slots:
    void onSessionConnected();
    void onSessionBytes(const QByteArray &data);
    void onCronTick();
    void onRunnerFinished(bool ok, const QString &filePath);

private:
    int  findSnapshotIndex(const QString &name) const;
    void dispatchNext(ISession *session);
    void enqueue(ISession *session, int snapIdx);
    void deliverEmail(const snapshot_entry_t &snap,
                      const QString &sessionName,
                      const QString &filePath);
    bool shouldCooldownSkip(int ruleIdx);

    profile_t  m_profile{};
    QHash<ISession *, SessionBinding> m_sessions;
    QTimer     m_cronTimer;
    int        m_lastCronMinute = -1;
    QMap<int, qint64> m_lastRuleFire;  // rule index -> last fire epoch ms
};

} // namespace tscrt
