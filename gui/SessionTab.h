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
#include <QWidget>

class ISession;
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
    TerminalWidget *terminal() const { return m_term; }
    ISession       *session()  const { return m_session; }
    ButtonBar      *buttonBar() const { return m_buttons; }

private slots:
    void onButtonAction(const QString &actionString);
    void onMarkRequested();
    void onLoopRequested();
    void onHelpRequested();

private:
    TerminalWidget   *m_term    = nullptr;
    ButtonBar        *m_buttons = nullptr;
    AutomationEngine *m_engine  = nullptr;
    ISession         *m_session = nullptr;
    QString           m_displayName;
};

} // namespace tscrt
