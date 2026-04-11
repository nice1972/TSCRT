/*
 * Pane - one terminal + backend session inside a SessionTab.
 *
 * A SessionTab may contain several Pane instances arranged in a QSplitter
 * tree (split horizontally / vertically). Each Pane owns its own
 * TerminalWidget, ISession, AutomationEngine and SessionLogger, and runs
 * its own auto-reconnect state machine. The Pane also draws a coloured
 * border to indicate active focus / broadcast membership.
 */
#pragma once

#include "tscrt.h"

#include <QByteArray>
#include <QFrame>
#include <QList>
#include <QMetaObject>
#include <QString>
#include <QTimer>

class ISession;
class MainWindow;
class SessionLogger;
class TerminalWidget;

namespace tscrt {

class AutomationEngine;

class Pane : public QFrame {
    Q_OBJECT
public:
    Pane(MainWindow *mw, const profile_t &profile,
         const session_entry_t &entry, ISession *session,
         QWidget *parent = nullptr);
    ~Pane() override;

    TerminalWidget        *terminal() const { return m_term; }
    ISession              *session()  const { return m_session; }
    AutomationEngine      *engine()   const { return m_engine; }
    const session_entry_t &entry()    const { return m_entry; }

    QString markPattern() const { return m_markPattern; }
    void    setMarkPattern(const QString &p);
    void    clearMark();

    void setActive(bool on);
    bool isActive() const { return m_active; }
    void setBroadcastMember(bool on);
    bool isBroadcastMember() const { return m_bcast; }

    /// Mark the next disconnect as user-initiated so the reconnect
    /// state machine won't try to restore the session.
    void suppressAutoReconnect() { m_userRequestedDisconnect = true; }

signals:
    /// Emitted just before the current ISession is destroyed during a
    /// reconnect, so MainWindow can detach it from SnapshotManager.
    void sessionAboutToChange(ISession *oldSession);
    /// Emitted after a reconnect installs a fresh ISession on this pane.
    void sessionRebound(ISession *newSession);

private slots:
    void onSessionDisconnected(const QString &reason);
    void reconnectNow();

private:
    void bindSession(ISession *session);
    void scheduleReconnect();
    void applyBorder();

    MainWindow       *m_mw      = nullptr;
    session_entry_t   m_entry{};
    TerminalWidget   *m_term    = nullptr;
    ISession         *m_session = nullptr;
    ::SessionLogger  *m_logger  = nullptr;
    AutomationEngine *m_engine  = nullptr;
    QString           m_markPattern;

    QList<QMetaObject::Connection> m_sessionConns;

    QTimer m_rcTimer;
    int    m_rcAttempts = 0;
    bool   m_userRequestedDisconnect = false;

    bool   m_active = false;
    bool   m_bcast  = false;
};

} // namespace tscrt
