/*
 * AutomationEngine - per-session startup/trigger/periodic dispatcher.
 *
 * One instance lives inside each session tab. It listens to the session's
 * bytesReceived signal for trigger pattern matching, owns a QTimer for
 * periodic execution, and replays startup actions on the connected
 * signal. All outgoing bytes go through executeAction() which parses
 * escape sequences and sends bytes (with pauses) back to the session.
 */
#pragma once

#include "tscrt.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

class ISession;

namespace tscrt {

class AutomationEngine : public QObject {
    Q_OBJECT
public:
    AutomationEngine(ISession *session, const profile_t &profile,
                     const QString &sessionName,
                     QObject *parent = nullptr);

    /// Replace highlight pattern (used by ButtonBar's Mark feature).
    void setHighlightPattern(const QString &pattern);
    QString highlightPattern() const { return m_highlight; }

    /// Execute an arbitrary action string (used by button clicks).
    void executeAction(const QString &actionString);

private slots:
    void onConnected();
    void onBytesReceived(const QByteArray &data);
    void onPeriodicTick();

private:
    ISession    *m_session = nullptr;
    profile_t    m_profile;
    QString      m_sessionName;
    QTimer       m_periodicTimer;
    QByteArray   m_triggerBuf;
    QString      m_highlight;
    QVector<qint64> m_lastFiredAt;         // periodic last-fire epoch ms
    QVector<qint64> m_lastTriggerFiredAt;  // pattern trigger last-fire epoch ms
    qint64          m_burstWindowStart = 0;
    int             m_burstCount       = 0;
};

} // namespace tscrt
