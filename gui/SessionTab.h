/*
 * SessionTab - composite widget hosting one terminal session.
 *
 * Lays out a TerminalWidget on top of a ButtonBar, owns the ISession
 * backend and the AutomationEngine for that session, and wires the
 * data signals between them.
 */
#pragma once

#include "tscrt.h"

#include <QString>
#include <QTimer>
#include <QWidget>

class CommandLineWidget;
class ISession;
class SessionLogger;
class TerminalWidget;

namespace tscrt {

class AutomationEngine;
class ButtonBar;

class SessionTab : public QWidget {
    Q_OBJECT
public:
    SessionTab(ISession *session, const profile_t &profile,
               const session_entry_t &entry, QWidget *parent = nullptr);
    ~SessionTab() override;

    QString displayName() const { return m_displayName; }
    TerminalWidget    *terminal() const { return m_term; }
    ISession          *session()  const { return m_session; }
    ButtonBar         *buttonBar() const { return m_buttons; }
    ::CommandLineWidget *commandLine() const { return m_cmdLine; }

signals:
    void buttonEditRequested(int slotIndex);

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

private:
    void configureLoop();          // pop the cmd+interval dialog
    void startLoop();               // start ticking with current cfg
    void stopLoop();                // stop ticking
    void configureMark();           // pop the highlight pattern dialog
    void clearMark();               // clear the highlight pattern

private:
    TerminalWidget      *m_term    = nullptr;
    ::CommandLineWidget *m_cmdLine = nullptr;
    ButtonBar           *m_buttons = nullptr;
    AutomationEngine    *m_engine  = nullptr;
    ISession            *m_session = nullptr;
    ::SessionLogger     *m_logger  = nullptr;
    QString              m_displayName;

    // Loop state (per session, in-memory).
    QTimer               m_loopTimer;
    QString              m_loopAction;
    int                  m_loopIntervalSec = 0;

    // Mark state.
    QString              m_markPattern;
};

} // namespace tscrt
