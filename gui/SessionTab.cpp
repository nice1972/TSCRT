#include "SessionTab.h"

#include "AutomationEngine.h"
#include "ButtonBar.h"
#include "CommandLineWidget.h"
#include "FindBar.h"
#include "ISession.h"
#include "SessionHistory.h"
#include "SessionLogger.h"
#include "TerminalWidget.h"

#include <QInputDialog>
#include <QKeySequence>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QVBoxLayout>

namespace tscrt {

SessionTab::SessionTab(ISession *session, const profile_t &profile,
                       const session_entry_t &entry, QWidget *parent)
    : QWidget(parent),
      m_session(session),
      m_displayName(QString::fromLocal8Bit(entry.name)),
      m_showCmdLineFs(entry.show_cmdline != 0),
      m_showButtonsFs(entry.show_buttons != 0)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_term = new TerminalWidget(this);
    root->addWidget(m_term, 1);

    // Find / Mark bar — hidden until Ctrl+F or the Mark button opens it.
    m_findBar = new FindBar(this);
    m_findBar->hide();
    root->addWidget(m_findBar);
    connect(m_findBar, &FindBar::patternOrOptionsChanged, this, [this]() {
        if (!m_term) return;
        const int total = m_term->findAll(m_findBar->pattern(),
                                          m_findBar->caseSensitive(),
                                          m_findBar->regex());
        m_findBar->setMatchInfo(m_term->findCurrentIndex(), total);
        if (m_findBar->markActive())
            m_term->setHighlightPattern(m_findBar->pattern());
    });
    connect(m_findBar, &FindBar::findNextRequested, this, [this]() {
        if (m_term) m_term->findNext();
    });
    connect(m_findBar, &FindBar::findPrevRequested, this, [this]() {
        if (m_term) m_term->findPrev();
    });
    connect(m_findBar, &FindBar::markToggled, this, [this](bool on) {
        if (!m_term) return;
        if (on) {
            m_markPattern = m_findBar->pattern();
            m_term->setHighlightPattern(m_markPattern);
            if (m_engine) m_engine->setHighlightPattern(m_markPattern);
            if (m_buttons) m_buttons->setMarkActive(!m_markPattern.isEmpty());
        } else {
            m_markPattern.clear();
            m_term->setHighlightPattern(QString());
            if (m_engine) m_engine->setHighlightPattern(QString());
            if (m_buttons) m_buttons->setMarkActive(false);
        }
    });
    connect(m_findBar, &FindBar::closeRequested, this, [this]() {
        if (m_term) {
            m_term->clearFind();
            m_term->setFocus();
        }
        m_findBar->hide();
    });
    connect(m_term, &TerminalWidget::findIndexChanged,
            m_findBar, &FindBar::setMatchInfo);

    // Ctrl+F (⌘F on mac via Qt::CTRL portable constant) opens the bar.
    auto *findShortcut = new QShortcut(
        QKeySequence(QKeySequence::Find), this);
    findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        showFindBar(false);
    });

    // Command line entry — keeps a per-session history file.
    m_cmdLine = new ::CommandLineWidget(this);
    m_cmdLine->setHistoryFile(tscrt::historyPathFor(profile, m_displayName));
    root->addWidget(m_cmdLine);
    connect(m_cmdLine, &::CommandLineWidget::commandEntered,
            this, &SessionTab::onCommandEntered);

    // Bottom shortcut buttons.
    m_buttons = new ButtonBar(this);
    m_buttons->setButtons(profile.buttons);
    root->addWidget(m_buttons);
    connect(m_buttons, &ButtonBar::actionRequested,
            this, &SessionTab::onButtonAction);
    connect(m_buttons, &ButtonBar::buttonEditRequested,
            this, &SessionTab::buttonEditRequested);
    connect(m_buttons, &ButtonBar::markClicked,
            this, &SessionTab::onMarkClicked);
    connect(m_buttons, &ButtonBar::markRightClicked,
            this, &SessionTab::onMarkRightClicked);
    connect(m_buttons, &ButtonBar::markDoubleClicked,
            this, &SessionTab::onMarkDoubleClicked);
    connect(m_buttons, &ButtonBar::loopClicked,
            this, &SessionTab::onLoopClicked);
    connect(m_buttons, &ButtonBar::loopRightClicked,
            this, &SessionTab::onLoopRightClicked);
    connect(m_buttons, &ButtonBar::loopDoubleClicked,
            this, &SessionTab::onLoopDoubleClicked);
    connect(m_buttons, &ButtonBar::buttonLoopRequested,
            this, &SessionTab::onButtonLoopRequested);
    connect(m_buttons, &ButtonBar::helpRequested,
            this, &SessionTab::onHelpRequested);

    m_loopTimer.setSingleShot(false);
    connect(&m_loopTimer, &QTimer::timeout,
            this, &SessionTab::onLoopTick);

    // Honour persisted view-toggle preferences (default ON for both).
    {
        QSettings prefs;
        m_cmdLine->setVisible(
            prefs.value(QStringLiteral("ui/showCmdLine"), true).toBool());
        m_buttons->setVisible(
            prefs.value(QStringLiteral("ui/showButtonBar"), true).toBool());
    }

    // Engine listens to the session backend
    m_engine = new AutomationEngine(session, profile, m_displayName, this);

    // Per-session output log (file in profile.common.log_dir).
    if (entry.log_enabled) {
        const QString host = (entry.type == SESSION_TYPE_SSH)
            ? QString::fromLocal8Bit(entry.ssh.host)
            : QString::fromLocal8Bit(entry.serial.device);
        m_logger = new ::SessionLogger(
            QString::fromLocal8Bit(profile.common.log_dir),
            m_displayName, host, this);
        connect(session, &ISession::bytesReceived,
                m_logger, &::SessionLogger::onBytesReceived);
    }

    // Wire data flow:
    //   session -> terminal     (queued: GUI thread runs the event loop)
    //   terminal -> session     (DIRECT: the worker thread is stuck in a
    //                            tight read/write loop and never pumps its
    //                            event loop, so a queued sendBytes/resize
    //                            would never be delivered. Both target
    //                            slots are mutex-protected, so calling them
    //                            directly from the GUI thread is safe.)
    connect(session, &ISession::bytesReceived,
            m_term, &TerminalWidget::feedBytes);
    connect(m_term, &TerminalWidget::outputBytes,
            session, &ISession::sendBytes,
            Qt::DirectConnection);
    connect(m_term, &TerminalWidget::gridResized,
            session, &ISession::resize,
            Qt::DirectConnection);

    // Session lives on its own QThread — setParent across threads is
    // rejected by Qt, so SessionTab owns it manually and deletes it in
    // the destructor after stop() joins the worker thread.

    m_term->setFocus(Qt::OtherFocusReason);
}

SessionTab::~SessionTab()
{
    if (m_session) {
        m_session->stop();
        delete m_session;
        m_session = nullptr;
    }
}

void SessionTab::onButtonAction(const QString &actionString)
{
    if (m_engine)
        m_engine->executeAction(actionString);
}

void SessionTab::onCommandEntered(const QString &cmd)
{
    if (!m_session) return;
    QByteArray bytes = cmd.toUtf8();
    bytes.append('\r');
    m_session->sendBytes(bytes);
    // Keep focus in the command line; the user moves it elsewhere by
    // clicking the terminal or another widget.
}

void SessionTab::showFindBar(bool markPreset)
{
    if (!m_findBar) return;
    m_findBar->show();
    if (markPreset) {
        m_findBar->setPattern(m_markPattern);
        m_findBar->setMarkActive(!m_markPattern.isEmpty());
    } else {
        m_findBar->setMarkActive(!m_markPattern.isEmpty());
    }
    m_findBar->focusInput();
    // Trigger a search with the current text (if any) so the count updates.
    if (m_term && !m_findBar->pattern().isEmpty()) {
        const int total = m_term->findAll(m_findBar->pattern(),
                                          m_findBar->caseSensitive(),
                                          m_findBar->regex());
        m_findBar->setMatchInfo(m_term->findCurrentIndex(), total);
    }
}

void SessionTab::configureMark()
{
    // Legacy entry point — route to the shared Find/Mark bar.
    showFindBar(true);
}

void SessionTab::clearMark()
{
    m_markPattern.clear();
    if (m_engine) m_engine->setHighlightPattern(QString());
    if (m_term)   m_term->setHighlightPattern(QString());
    if (m_buttons) m_buttons->setMarkActive(false);
}

void SessionTab::onMarkClicked()
{
    // If mark is active, skip so double-click can fire clearMark().
    if (!m_markPattern.isEmpty()) return;
    configureMark();
}

void SessionTab::onMarkRightClicked()
{
    if (m_markPattern.isEmpty())
        configureMark();
    else
        clearMark();
}

void SessionTab::onMarkDoubleClicked()
{
    if (!m_markPattern.isEmpty())
        clearMark();
    else
        configureMark();
}

void SessionTab::configureLoop()
{
    bool ok = false;
    const QString cmd = QInputDialog::getText(this, tr("Loop"),
        tr("Command to repeat:"), QLineEdit::Normal, m_loopAction, &ok);
    if (!ok || cmd.isEmpty()) return;
    const int sec = QInputDialog::getInt(this, tr("Loop"),
        tr("Interval (seconds):"),
        m_loopIntervalSec > 0 ? m_loopIntervalSec : 60,
        1, 86400, 1, &ok);
    if (!ok) return;
    m_loopAction      = cmd;
    m_loopIntervalSec = sec;
}

void SessionTab::startLoop()
{
    if (m_loopAction.isEmpty() || m_loopIntervalSec <= 0) return;
    if (m_loopTimer.isActive()) return;
    m_loopTimer.start(m_loopIntervalSec * 1000);
    if (m_buttons) {
        m_buttons->setLoopRunning(true);
        m_buttons->setLoopingAction(m_loopAction);
    }
    onLoopTick();   // fire immediately so the user sees feedback
}

void SessionTab::stopLoop()
{
    m_loopTimer.stop();
    if (m_buttons) {
        m_buttons->setLoopRunning(false);
        m_buttons->setLoopingAction(QString());
    }
}

void SessionTab::onLoopClicked()
{
    // Left click: start the loop. If nothing is configured yet, prompt
    // first and then start.
    if (m_loopTimer.isActive()) return;       // already running
    if (m_loopAction.isEmpty() || m_loopIntervalSec <= 0)
        configureLoop();
    startLoop();
}

void SessionTab::onLoopRightClicked()
{
    // Right click while running → cancel.
    // Right click while idle    → open the configuration dialog.
    if (m_loopTimer.isActive())
        stopLoop();
    else
        configureLoop();
}

void SessionTab::onLoopDoubleClicked()
{
    // Double click: stop if running, otherwise configure+start.
    if (m_loopTimer.isActive()) {
        stopLoop();
    } else {
        configureLoop();
        startLoop();
    }
}

void SessionTab::onLoopTick()
{
    if (m_engine && !m_loopAction.isEmpty())
        m_engine->executeAction(m_loopAction + QStringLiteral("\\r"));
    if (m_buttons)
        m_buttons->flashLoopingButton();
}

void SessionTab::onButtonLoopRequested(const QString &action)
{
    // If the same action is already looping, stop it.
    if (m_loopTimer.isActive() && m_loopAction == action) {
        stopLoop();
        return;
    }

    // If a different loop is running, stop it first.
    if (m_loopTimer.isActive())
        stopLoop();

    bool ok = false;
    const int sec = QInputDialog::getInt(this, tr("Loop"),
        tr("Interval (seconds):"),
        m_loopIntervalSec > 0 ? m_loopIntervalSec : 60,
        1, 86400, 1, &ok);
    if (!ok) return;

    m_loopAction      = action;
    m_loopIntervalSec = sec;
    startLoop();
}

void SessionTab::onHelpRequested()
{
    QMessageBox::information(this, tr("TSCRT help"),
        tr("<h3>Quick reference</h3>"
           "<ul>"
           "<li>Bottom buttons send their action when clicked.</li>"
           "<li><b>loop</b> repeats a command on an interval.</li>"
           "<li><b>mark</b> highlights a substring in incoming output.</li>"
           "<li>Resize the window to update the remote PTY size.</li>"
           "<li>Settings &rarr; Preferences (Ctrl+,) edits everything.</li>"
           "</ul>"));
}

} // namespace tscrt
