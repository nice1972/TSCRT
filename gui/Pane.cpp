#include "Pane.h"

#include "AutomationEngine.h"
#include "ISession.h"
#include "MainWindow.h"
#include "SessionLogger.h"
#include "TerminalWidget.h"

#include <QMainWindow>
#include <QRandomGenerator>
#include <QStatusBar>
#include <QVBoxLayout>

namespace tscrt {

Pane::Pane(MainWindow *mw, const profile_t &profile,
           const session_entry_t &entry, ISession *session, QWidget *parent)
    : QFrame(parent),
      m_mw(mw),
      m_entry(entry)
{
    setFrameShape(QFrame::NoFrame);
    // stylesheet-driven borders (applied in applyBorder()).

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_term = new TerminalWidget(this);
    lay->addWidget(m_term, 1);
    setFocusProxy(m_term);

    // Create the per-session log file once — reconnects reuse it.
    if (entry.log_enabled) {
        const QString host = (entry.type == SESSION_TYPE_SSH)
            ? QString::fromLocal8Bit(entry.ssh.host)
            : QString::fromLocal8Bit(entry.serial.device);
        m_logger = new ::SessionLogger(
            QString::fromLocal8Bit(profile.common.log_dir),
            QString::fromLocal8Bit(entry.name), host, this);
    }

    m_rcTimer.setSingleShot(true);
    connect(&m_rcTimer, &QTimer::timeout, this, &Pane::reconnectNow);

    bindSession(session);
    applyBorder();
}

Pane::~Pane()
{
    m_userRequestedDisconnect = true;
    m_rcTimer.stop();
    for (const auto &c : m_sessionConns)
        QObject::disconnect(c);
    m_sessionConns.clear();
    if (m_session) {
        m_session->stop();
        delete m_session;
        m_session = nullptr;
    }
}

void Pane::bindSession(ISession *session)
{
    for (const auto &c : m_sessionConns)
        QObject::disconnect(c);
    m_sessionConns.clear();

    if (m_engine) {
        delete m_engine;
        m_engine = nullptr;
    }

    m_session = session;
    if (!session)
        return;

    if (m_mw) {
        m_engine = new AutomationEngine(session, m_mw->profile(),
                                        QString::fromLocal8Bit(m_entry.name),
                                        this);
        if (!m_markPattern.isEmpty())
            m_engine->setHighlightPattern(m_markPattern);
    }

    m_sessionConns.append(connect(session, &ISession::bytesReceived,
                                  m_term, &TerminalWidget::feedBytes));
    m_sessionConns.append(connect(m_term, &TerminalWidget::outputBytes,
                                  session, &ISession::sendBytes,
                                  Qt::DirectConnection));
    m_sessionConns.append(connect(m_term, &TerminalWidget::gridResized,
                                  session, &ISession::resize,
                                  Qt::DirectConnection));
    if (m_logger) {
        m_sessionConns.append(connect(session, &ISession::bytesReceived,
                                      m_logger,
                                      &::SessionLogger::onBytesReceived));
    }
    m_sessionConns.append(connect(session, &ISession::disconnected,
                                  this, &Pane::onSessionDisconnected));
    m_sessionConns.append(connect(session, &ISession::connected,
                                  this, [this]{ m_rcAttempts = 0; }));
}

void Pane::onSessionDisconnected(const QString &reason)
{
    if (m_userRequestedDisconnect)
        return;
    if (m_entry.auto_reconnect == 0)
        return;
    if (m_entry.reconnect_max > 0 &&
        m_rcAttempts >= m_entry.reconnect_max) {
        if (m_mw) {
            m_mw->statusBar()->showMessage(
                tr("Reconnect failed for %1: %2")
                    .arg(QString::fromLocal8Bit(m_entry.name), reason), 7000);
        }
        return;
    }
    scheduleReconnect();
}

void Pane::scheduleReconnect()
{
    const int base = m_entry.reconnect_base_ms > 0
        ? m_entry.reconnect_base_ms : 500;
    int shift = m_rcAttempts;
    if (shift > 10) shift = 10;
    int delay = base * (1 << shift);
    if (delay > 30000) delay = 30000;
    delay += int(QRandomGenerator::global()->bounded(250u));
    m_rcAttempts++;

    if (m_mw) {
        const QString tail = (m_entry.reconnect_max > 0)
            ? QStringLiteral("/%1").arg(m_entry.reconnect_max)
            : QString();
        m_mw->statusBar()->showMessage(
            tr("Reconnecting %1 in %2s (attempt %3%4)...")
                .arg(QString::fromLocal8Bit(m_entry.name))
                .arg(delay / 1000)
                .arg(m_rcAttempts)
                .arg(tail), 5000);
    }
    m_rcTimer.start(delay);
}

void Pane::reconnectNow()
{
    if (!m_mw)
        return;

    if (m_session) {
        emit sessionAboutToChange(m_session);
        m_session->stop();
        delete m_session;
        m_session = nullptr;
    }

    ISession *fresh = m_mw->makeSessionFor(m_mw->profile(), m_entry);
    if (!fresh)
        return;

    bindSession(fresh);
    emit sessionRebound(fresh);

    if (m_term)
        fresh->resize(m_term->cols(), m_term->rows());
    fresh->start();
}

void Pane::setMarkPattern(const QString &p)
{
    m_markPattern = p;
    if (m_term)   m_term->setHighlightPattern(p);
    if (m_engine) m_engine->setHighlightPattern(p);
}

void Pane::clearMark()
{
    setMarkPattern(QString());
}

void Pane::setActive(bool on)
{
    if (m_active == on) return;
    m_active = on;
    applyBorder();
}

void Pane::setBroadcastMember(bool on)
{
    if (m_bcast == on) return;
    m_bcast = on;
    applyBorder();
}

void Pane::applyBorder()
{
    // Orange border wins over blue so broadcast membership is obvious.
    // 1px keeps panes visually tight; the colour still reads clearly.
    const char *color = "transparent";
    if (m_bcast)       color = "#e05a00";
    else if (m_active) color = "#3a80ff";
    setStyleSheet(QStringLiteral("tscrt--Pane { border: 1px solid %1; }")
                      .arg(QString::fromLatin1(color)));
}

} // namespace tscrt
