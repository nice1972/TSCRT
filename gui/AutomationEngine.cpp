#include "AutomationEngine.h"

#include "ActionParser.h"
#include "ISession.h"

#include <QDateTime>
#include <QDebug>
#include <QThread>

#include <cstring>

namespace tscrt {

namespace {
constexpr int kMaxTriggerBuf = 4096;
constexpr int kPeriodicTickMs = 1000;
} // namespace

AutomationEngine::AutomationEngine(ISession *session, const profile_t &profile,
                                   const QString &sessionName, QObject *parent)
    : QObject(parent),
      m_session(session),
      m_profile(profile),
      m_sessionName(sessionName)
{
    m_triggerBuf.reserve(kMaxTriggerBuf);
    m_lastFiredAt.fill(0, MAX_PERIODIC);

    if (m_session) {
        connect(m_session, &ISession::connected,
                this, &AutomationEngine::onConnected);
        connect(m_session, &ISession::bytesReceived,
                this, &AutomationEngine::onBytesReceived);
    }

    m_periodicTimer.setInterval(kPeriodicTickMs);
    connect(&m_periodicTimer, &QTimer::timeout,
            this, &AutomationEngine::onPeriodicTick);
}

void AutomationEngine::setHighlightPattern(const QString &pattern)
{
    m_highlight = pattern;
}

void AutomationEngine::executeAction(const QString &actionString)
{
    if (!m_session)
        return;
    const auto chunks = parseAction(actionString);
    for (const auto &c : chunks) {
        if (!c.bytes.isEmpty())
            QMetaObject::invokeMethod(m_session, "sendBytes",
                                      Qt::QueuedConnection,
                                      Q_ARG(QByteArray, c.bytes));
        if (c.pauseMs > 0)
            QThread::msleep(c.pauseMs);
    }
}

void AutomationEngine::onConnected()
{
    // Replay startup actions for this session
    for (int i = 0; i < m_profile.startup_count; ++i) {
        if (QString::fromLocal8Bit(m_profile.startups[i].session) == m_sessionName) {
            qInfo("automation: startup [%s] -> %s",
                  m_profile.startups[i].name,
                  m_profile.startups[i].action);
            executeAction(QString::fromLocal8Bit(m_profile.startups[i].action));
        }
    }
    // Initialize periodic timestamps and start ticking if any defined
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < m_profile.periodic_count; ++i)
        m_lastFiredAt[i] = now;

    bool any = false;
    for (int i = 0; i < m_profile.periodic_count; ++i) {
        if (QString::fromLocal8Bit(m_profile.periodics[i].session) == m_sessionName) {
            any = true;
            break;
        }
    }
    if (any)
        m_periodicTimer.start();
}

void AutomationEngine::onBytesReceived(const QByteArray &data)
{
    if (m_profile.trigger_count == 0)
        return;

    // Append to ring buffer
    m_triggerBuf.append(data);
    if (m_triggerBuf.size() > kMaxTriggerBuf)
        m_triggerBuf.remove(0, m_triggerBuf.size() - kMaxTriggerBuf);

    for (int i = 0; i < m_profile.trigger_count; ++i) {
        if (QString::fromLocal8Bit(m_profile.triggers[i].session) != m_sessionName)
            continue;
        const QByteArray pat(m_profile.triggers[i].pattern);
        if (pat.isEmpty())
            continue;
        if (m_triggerBuf.contains(pat)) {
            qInfo("automation: trigger [%s] fired", pat.constData());
            executeAction(QString::fromLocal8Bit(m_profile.triggers[i].action));
            m_triggerBuf.clear();
            break;
        }
    }
}

void AutomationEngine::onPeriodicTick()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < m_profile.periodic_count; ++i) {
        if (QString::fromLocal8Bit(m_profile.periodics[i].session) != m_sessionName)
            continue;
        const int interval = m_profile.periodics[i].interval;
        if (interval <= 0)
            continue;
        if (now - m_lastFiredAt[i] >= qint64(interval) * 1000) {
            qInfo("automation: periodic [%s] fire (%ds)",
                  m_profile.periodics[i].name, interval);
            executeAction(QString::fromLocal8Bit(m_profile.periodics[i].action));
            m_lastFiredAt[i] = now;
        }
    }
}

} // namespace tscrt
