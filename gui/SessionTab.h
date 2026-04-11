/*
 * SessionTab - composite widget hosting one or more terminal panes.
 *
 * Each tab holds a QSplitter tree of Pane objects. A Pane wraps one
 * TerminalWidget + ISession backend. The FindBar, CommandLineWidget
 * and ButtonBar are shared by the tab and always target the active
 * pane. Split-pane and input-broadcast live here; auto-reconnect
 * lives inside each Pane.
 */
#pragma once

#include "tscrt.h"

#include <QList>
#include <QString>
#include <QTimer>
#include <QWidget>

class CommandLineWidget;
class ISession;
class QSplitter;
class SessionLogger;
class TerminalWidget;

namespace tscrt {

class AutomationEngine;
class ButtonBar;
class FindBar;
class Pane;

class SessionTab : public QWidget {
    Q_OBJECT
public:
    SessionTab(ISession *session, const profile_t &profile,
               const session_entry_t &entry, QWidget *parent = nullptr);
    ~SessionTab() override;

    QString displayName() const { return m_displayName; }

    // Backward-compat adapters — always target the active pane so
    // existing MainWindow call sites (tab->terminal(), tab->session())
    // keep working unchanged.
    TerminalWidget *terminal() const;
    ISession       *session()  const;

    ButtonBar           *buttonBar() const { return m_buttons; }
    ::CommandLineWidget *commandLine() const { return m_cmdLine; }
    bool showCmdLineInFullscreen() const { return m_showCmdLineFs; }
    bool showButtonsInFullscreen() const { return m_showButtonsFs; }

    /// Show the Find/Mark bar and focus the input. If markPreset is true
    /// the Mark toggle starts on with the current mark pattern pre-filled.
    void showFindBar(bool markPreset = false);

    // Split / broadcast API
    Pane *activePane() const { return m_activePane; }
    const QList<Pane*> &panes() const { return m_panes; }
    void splitActive(Qt::Orientation orient);
    void closeActivePane();

    bool broadcastEnabled() const { return m_broadcastEnabled; }
    void setBroadcastEnabled(bool on);

signals:
    void buttonEditRequested(int slotIndex);
    /// A new pane has joined the tab (initial, split, reconnect).
    /// MainWindow uses this to attach its status-bar + snapshot hooks.
    void paneAdded(Pane *pane);
    /// A pane is about to be destroyed.
    void paneRemoving(Pane *pane);
    /// Relayed from Pane: its ISession is about to be destroyed.
    void sessionAboutToChange(ISession *oldSession);
    /// Relayed from Pane: reconnect produced a fresh ISession.
    void sessionRebound(ISession *newSession);
    /// Tab has no panes left — MainWindow should close the tab.
    void tabEmpty();

private slots:
    void onButtonAction(const QString &actionString);
    void onMarkClicked();          // left click on mark button
    void onMarkRightClicked();     // right click on mark button
    void onMarkDoubleClicked();    // double click on mark button
    void onLoopClicked();          // left click on loop button
    void onLoopRightClicked();     // right click on loop button
    void onLoopDoubleClicked();    // double click on loop button
    void onLoopTick();
    void onButtonLoopRequested(const QString &action);
    void onHelpRequested();
    void onCommandEntered(const QString &cmd);
    void onAppFocusChanged(QWidget *old, QWidget *now);

private:
    void configureLoop();
    void startLoop();
    void stopLoop();
    void configureMark();
    void clearMark();

    Pane *createPane(ISession *session);
    void  addPaneToSplitter(Pane *pane, Pane *after, Qt::Orientation orient);
    void  setActivePane(Pane *pane);
    void  wirePaneBroadcast(Pane *pane);
    void  removePane(Pane *pane);
    void  refreshBroadcastBadges();

private:
    profile_t                 m_profile{};
    session_entry_t           m_entry{};
    QSplitter                *m_rootSplitter = nullptr;
    QList<Pane*>              m_panes;
    Pane                     *m_activePane = nullptr;

    FindBar                  *m_findBar = nullptr;
    ::CommandLineWidget      *m_cmdLine = nullptr;
    ButtonBar                *m_buttons = nullptr;
    QString                   m_displayName;

    // Loop state (tab-wide, dispatches to the active pane).
    QTimer               m_loopTimer;
    QString              m_loopAction;
    int                  m_loopIntervalSec = 0;

    // Per-session fullscreen visibility (derived from the initial entry).
    bool                 m_showCmdLineFs = false;
    bool                 m_showButtonsFs = false;

    bool                 m_broadcastEnabled = false;
};

} // namespace tscrt
