/*
 * SnapshotRunner - execute a snapshot (ordered command list) on a
 * running ISession, capturing output and writing it to a file.
 *
 * Per step:
 *   1. Send the parsed command bytes (reusing ActionParser).
 *   2. Wait `delay_ms` (minimum).
 *   3. If `expect_prompt` is set, start the max_wait_ms timer and watch
 *      incoming bytes for the regex; advance on match or timeout.
 *   4. Otherwise advance immediately after `delay_ms`.
 *
 * Runs entirely on the GUI thread with QTimer events, so it never blocks
 * the Qt event loop. Signals finished(ok, filePath) when done.
 */
#pragma once

#include "tscrt.h"

#include <QByteArray>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QTimer>

class ISession;

namespace tscrt {

class SnapshotRunner : public QObject {
    Q_OBJECT
public:
    SnapshotRunner(ISession              *session,
                   const snapshot_entry_t &snap,
                   const QString          &sessionName,
                   const QString          &logDir,
                   QObject                *parent = nullptr);

    void start();
    bool isRunning() const { return m_running; }
    QString savedFilePath() const { return m_filePath; }
    QString capturedText()  const { return QString::fromUtf8(m_captured); }
    const snapshot_entry_t &snapshot() const { return m_snap; }

signals:
    void progress(int step, int total);
    void finished(bool ok, const QString &filePath);

private slots:
    void onBytes(const QByteArray &data);
    void onDelayElapsed();
    void onMaxWaitElapsed();

private:
    void runNextStep();
    void finishStep();
    void writeAndFinish(bool ok);

    ISession        *m_session;
    snapshot_entry_t m_snap;
    QString          m_sessionName;
    QString          m_logDir;

    bool        m_running  = false;
    int         m_stepIdx  = 0;
    bool        m_inWait   = false;
    bool        m_waitingPrompt = false;
    int         m_ansiState = 0;

    QByteArray  m_stepCapture;   // bytes collected for current step only
    QByteArray  m_captured;      // full ANSI-stripped capture with headers
    QString     m_filePath;

    QTimer      m_delayTimer;
    QTimer      m_maxWaitTimer;
    QRegularExpression m_currentRe;
};

} // namespace tscrt
