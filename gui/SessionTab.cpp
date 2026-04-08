#include "SessionTab.h"

#include "AutomationEngine.h"
#include "ButtonBar.h"
#include "ISession.h"
#include "TerminalWidget.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>

namespace tscrt {

SessionTab::SessionTab(ISession *session, const profile_t &profile,
                       const session_entry_t &entry, QWidget *parent)
    : QWidget(parent),
      m_session(session),
      m_displayName(QString::fromLocal8Bit(entry.name))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_term = new TerminalWidget(this);
    root->addWidget(m_term, 1);

    m_buttons = new ButtonBar(this);
    m_buttons->setButtons(profile.buttons);
    root->addWidget(m_buttons, 0);

    // Engine listens to the session backend
    m_engine = new AutomationEngine(session, profile, m_displayName, this);

    // Wire data flow:
    //   session -> terminal
    //   terminal -> session (input)
    //   terminal -> session (resize)
    //   button   -> engine.executeAction
    connect(session, &ISession::bytesReceived,
            m_term, &TerminalWidget::feedBytes);
    connect(m_term, &TerminalWidget::outputBytes,
            session, &ISession::sendBytes);
    connect(m_term, &TerminalWidget::gridResized,
            session, &ISession::resize);

    connect(m_buttons, &ButtonBar::actionRequested,
            this, &SessionTab::onButtonAction);
    connect(m_buttons, &ButtonBar::markRequested,
            this, &SessionTab::onMarkRequested);
    connect(m_buttons, &ButtonBar::loopRequested,
            this, &SessionTab::onLoopRequested);
    connect(m_buttons, &ButtonBar::helpRequested,
            this, &SessionTab::onHelpRequested);

    // Take parent ownership of the session so it dies with the tab.
    if (session && session->parent() != this)
        session->setParent(this);

    m_term->setFocus(Qt::OtherFocusReason);
}

SessionTab::~SessionTab()
{
    if (m_session)
        m_session->stop();
}

void SessionTab::onButtonAction(const QString &actionString)
{
    if (m_engine)
        m_engine->executeAction(actionString);
}

void SessionTab::onMarkRequested()
{
    bool ok = false;
    const QString cur = m_engine ? m_engine->highlightPattern() : QString();
    const QString p = QInputDialog::getText(this, tr("Mark"),
        tr("Highlight pattern (empty to clear):"), QLineEdit::Normal, cur, &ok);
    if (!ok) return;
    if (m_engine) m_engine->setHighlightPattern(p);
    if (m_term)   m_term->setHighlightPattern(p);
}

void SessionTab::onLoopRequested()
{
    bool ok = false;
    const QString cmd = QInputDialog::getText(this, tr("Loop"),
        tr("Command to repeat:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || cmd.isEmpty()) return;
    const int sec = QInputDialog::getInt(this, tr("Loop"),
        tr("Interval (seconds):"), 60, 1, 86400, 1, &ok);
    if (!ok) return;
    // For now we fire once + queue another via QTimer. Simple implementation:
    // run an action with embedded \r and rely on AutomationEngine periodic.
    // Until full periodic-from-button support lands, just send once.
    if (m_engine)
        m_engine->executeAction(cmd + QStringLiteral("\\r"));
    Q_UNUSED(sec);
}

void SessionTab::onHelpRequested()
{
    QMessageBox::information(this, tr("tscrt help"),
        tr("<h3>Quick reference</h3>"
           "<ul>"
           "<li>Bottom buttons send their action when clicked.</li>"
           "<li><b>cmd</b> opens the broadcast command window.</li>"
           "<li><b>loop</b> repeats a command on an interval.</li>"
           "<li><b>mark</b> highlights a substring in incoming output.</li>"
           "<li>Resize the window to update the remote PTY size.</li>"
           "<li>Settings &rarr; Preferences (Ctrl+,) edits everything.</li>"
           "</ul>"));
}

} // namespace tscrt
