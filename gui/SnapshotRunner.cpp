#include "SnapshotRunner.h"

#include "ActionParser.h"
#include "ISession.h"
#include "SessionLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace tscrt {

namespace {

// Upper bound on the rolling window that expect_prompt regex is evaluated
// against. Prompts are matched near the tail of the output, so trimming
// the front keeps the match cost bounded and neutralises the ReDoS
// surface that a malicious or careless profile regex could otherwise
// exploit (e.g. "(a+)+b" against an attacker-grown buffer).
constexpr int kMaxStepCaptureBytes = 64 * 1024;

QString sanitizeForFilename(const QString &in)
{
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|\\s]+"));
    QString out = in;
    out.replace(bad, QStringLiteral("_"));
    if (out.isEmpty()) out = QStringLiteral("x");
    return out;
}

} // namespace

SnapshotRunner::SnapshotRunner(ISession               *session,
                               const snapshot_entry_t &snap,
                               const QString          &sessionName,
                               const QString          &logDir,
                               QObject                *parent)
    : QObject(parent),
      m_session(session),
      m_snap(snap),
      m_sessionName(sessionName),
      m_logDir(logDir)
{
    m_delayTimer.setSingleShot(true);
    m_maxWaitTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout,
            this, &SnapshotRunner::onDelayElapsed);
    connect(&m_maxWaitTimer, &QTimer::timeout,
            this, &SnapshotRunner::onMaxWaitElapsed);
}

void SnapshotRunner::start()
{
    if (m_running || !m_session) return;
    m_running  = true;
    m_stepIdx  = 0;
    m_captured.clear();
    m_stepCapture.clear();
    m_ansiState = 0;

    connect(m_session, &ISession::bytesReceived,
            this, &SnapshotRunner::onBytes);

    const QString stamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyyMMdd_HHmmss"));
    m_captured += QStringLiteral("===== TSCRT snapshot: %1 =====\n"
                                 "session: %2\ntimestamp: %3\n\n")
                      .arg(QString::fromLocal8Bit(m_snap.name),
                           m_sessionName, stamp).toUtf8();

    runNextStep();
}

void SnapshotRunner::runNextStep()
{
    if (m_stepIdx >= m_snap.cmd_count) {
        writeAndFinish(true);
        return;
    }

    emit progress(m_stepIdx, m_snap.cmd_count);

    const snapshot_cmd_t &cmd = m_snap.cmds[m_stepIdx];
    const QString stamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("HH:mm:ss"));
    m_captured += QStringLiteral("\n----- [%1] $ %2 -----\n")
                      .arg(stamp, QString::fromLocal8Bit(cmd.command))
                      .toUtf8();

    // Parse command (ActionParser escape sequences) and send bytes.
    const auto chunks = parseAction(QString::fromLocal8Bit(cmd.command));
    for (const auto &c : chunks) {
        if (!c.bytes.isEmpty())
            m_session->sendBytes(c.bytes);
        // Intra-command \p pauses are skipped here: the whole step has
        // its own delay_ms timer and blocking on QThread::msleep would
        // stall the GUI event loop we depend on for onBytes().
    }

    m_stepCapture.clear();
    m_inWait = true;
    m_waitingPrompt = false;
    m_delayTimer.start(cmd.delay_ms > 0 ? cmd.delay_ms : 200);
}

void SnapshotRunner::onBytes(const QByteArray &data)
{
    if (!m_running) return;

    // Strip ANSI so the saved file matches what a human would see.
    const QByteArray stripped =
        SessionLogger::stripAnsiStateful(data, &m_ansiState);
    m_captured += stripped;
    if (m_inWait) {
        m_stepCapture += stripped;
        if (m_stepCapture.size() > kMaxStepCaptureBytes) {
            m_stepCapture.remove(
                0, m_stepCapture.size() - kMaxStepCaptureBytes);
        }
    }

    if (m_waitingPrompt && !m_currentRe.pattern().isEmpty()) {
        QRegularExpressionMatch m =
            m_currentRe.match(QString::fromUtf8(m_stepCapture));
        if (m.hasMatch()) {
            m_maxWaitTimer.stop();
            finishStep();
        }
    }
}

void SnapshotRunner::onDelayElapsed()
{
    const snapshot_cmd_t &cmd = m_snap.cmds[m_stepIdx];

    if (cmd.expect_prompt[0] && cmd.max_wait_ms > 0) {
        m_currentRe.setPattern(QString::fromLocal8Bit(cmd.expect_prompt));
        if (!m_currentRe.isValid()) {
            // Bad regex: skip straight to next step.
            finishStep();
            return;
        }
        // Check if the prompt already arrived during the fixed delay.
        QRegularExpressionMatch pre =
            m_currentRe.match(QString::fromUtf8(m_stepCapture));
        if (pre.hasMatch()) {
            finishStep();
            return;
        }
        m_waitingPrompt = true;
        m_maxWaitTimer.start(cmd.max_wait_ms);
        return;
    }
    finishStep();
}

void SnapshotRunner::onMaxWaitElapsed()
{
    m_captured += "(timeout waiting for prompt)\n";
    finishStep();
}

void SnapshotRunner::finishStep()
{
    m_inWait = false;
    m_waitingPrompt = false;
    m_stepIdx++;
    runNextStep();
}

void SnapshotRunner::writeAndFinish(bool ok)
{
    if (m_session)
        disconnect(m_session, &ISession::bytesReceived,
                   this, &SnapshotRunner::onBytes);
    m_running = false;

    QDir dir(m_logDir);
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));
    if (!dir.exists(QStringLiteral("snapshots")))
        dir.mkdir(QStringLiteral("snapshots"));
    dir.cd(QStringLiteral("snapshots"));

    const QString stamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString name = QStringLiteral("%1_%2_%3.log")
        .arg(sanitizeForFilename(QString::fromLocal8Bit(m_snap.name)),
             sanitizeForFilename(m_sessionName), stamp);
    m_filePath = dir.filePath(name);

    QFile f(m_filePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(m_captured);
        f.close();
    } else {
        ok = false;
    }

    emit progress(m_snap.cmd_count, m_snap.cmd_count);
    emit finished(ok, m_filePath);
}

} // namespace tscrt
