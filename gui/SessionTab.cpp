#include "SessionTab.h"

#include "AutomationEngine.h"
#include "ButtonBar.h"
#include "CommandLineWidget.h"
#include "FindBar.h"
#include "ISession.h"
#include "MainWindow.h"
#include "Pane.h"
#include "SessionHistory.h"
#include "SessionLogger.h"
#include "TerminalWidget.h"

#include <QApplication>
#include <QInputDialog>
#include <QKeySequence>
#include <QMainWindow>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

namespace tscrt {

SessionTab::SessionTab(ISession *session, const profile_t &profile,
                       const session_entry_t &entry, QWidget *parent)
    : QWidget(parent),
      m_profile(profile),
      m_entry(entry),
      m_displayName(QString::fromLocal8Bit(entry.name)),
      m_showCmdLineFs(entry.show_cmdline != 0),
      m_showButtonsFs(entry.show_buttons != 0)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Root splitter holds one or more Pane widgets. A single pane at
    // startup — split actions rebuild the tree. A 1px handle keeps the
    // gap between panes tight.
    m_rootSplitter = new QSplitter(Qt::Horizontal, this);
    m_rootSplitter->setChildrenCollapsible(false);
    m_rootSplitter->setHandleWidth(1);
    root->addWidget(m_rootSplitter, 1);

    // Find / Mark bar — hidden until Ctrl+F or the Mark button opens it.
    m_findBar = new FindBar(this);
    m_findBar->hide();
    root->addWidget(m_findBar);
    connect(m_findBar, &FindBar::patternOrOptionsChanged, this, [this]() {
        TerminalWidget *term = terminal();
        if (!term) return;
        const int total = term->findAll(m_findBar->pattern(),
                                        m_findBar->caseSensitive(),
                                        m_findBar->regex());
        m_findBar->setMatchInfo(term->findCurrentIndex(), total);
        if (m_findBar->markActive() && m_activePane)
            m_activePane->setMarkPattern(m_findBar->pattern());
    });
    connect(m_findBar, &FindBar::findNextRequested, this, [this]() {
        if (auto *term = terminal()) term->findNext();
    });
    connect(m_findBar, &FindBar::findPrevRequested, this, [this]() {
        if (auto *term = terminal()) term->findPrev();
    });
    connect(m_findBar, &FindBar::markToggled, this, [this](bool on) {
        if (!m_activePane) return;
        if (on) {
            m_activePane->setMarkPattern(m_findBar->pattern());
            if (m_buttons)
                m_buttons->setMarkActive(!m_activePane->markPattern().isEmpty());
        } else {
            m_activePane->clearMark();
            if (m_buttons) m_buttons->setMarkActive(false);
        }
    });
    connect(m_findBar, &FindBar::closeRequested, this, [this]() {
        if (auto *term = terminal()) {
            term->clearFind();
            term->setFocus();
        }
        m_findBar->hide();
    });

    // Ctrl+F opens the bar.
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

    // Initial pane wraps the session passed in by MainWindow. We do
    // NOT emit paneAdded() for the first pane: the caller (MainWindow)
    // already attaches its status-bar + snapshot hooks to the session
    // directly before constructing SessionTab, and firing a signal from
    // within a constructor cannot reach slots that haven't connected yet.
    Pane *first = createPane(session);
    m_rootSplitter->addWidget(first);
    m_panes.append(first);
    setActivePane(first);

    // Track focus at the application level so the active pane always
    // reflects whichever terminal the user last touched.
    connect(qApp, &QApplication::focusChanged,
            this, &SessionTab::onAppFocusChanged);

    first->terminal()->setFocus(Qt::OtherFocusReason);
}

SessionTab::~SessionTab()
{
    // Suppress auto-reconnect on all panes — the tab is being destroyed.
    for (Pane *p : m_panes) {
        if (p) p->suppressAutoReconnect();
    }
    // Pane destructors take care of joining worker threads.
}

TerminalWidget *SessionTab::terminal() const
{
    return m_activePane ? m_activePane->terminal() : nullptr;
}

ISession *SessionTab::session() const
{
    return m_activePane ? m_activePane->session() : nullptr;
}

Pane *SessionTab::createPane(ISession *session)
{
    auto *mw = qobject_cast<MainWindow*>(window());
    auto *pane = new Pane(mw, m_profile, m_entry, session, this);

    // Relay session lifecycle from Pane up to the tab so MainWindow
    // can keep its snapshot + status-bar hooks in sync.
    connect(pane, &Pane::sessionAboutToChange,
            this, &SessionTab::sessionAboutToChange);
    connect(pane, &Pane::sessionRebound,
            this, &SessionTab::sessionRebound);

    wirePaneBroadcast(pane);
    return pane;
}

void SessionTab::wirePaneBroadcast(Pane *pane)
{
    // Broadcast dispatcher: when enabled, mirror this pane's outgoing
    // bytes (keys, IME, paste, mouse reports) to every other pane.
    connect(pane->terminal(), &TerminalWidget::outputBytes, this,
            [this, src = pane](const QByteArray &b) {
        if (!m_broadcastEnabled) return;
        for (Pane *p : m_panes) {
            if (p == src || !p->session()) continue;
            p->session()->sendBytes(b);
        }
    });
}

void SessionTab::addPaneToSplitter(Pane *newPane, Pane *afterPane,
                                   Qt::Orientation orient)
{
    if (!afterPane) {
        m_rootSplitter->addWidget(newPane);
        return;
    }
    auto *parentSplit = qobject_cast<QSplitter*>(afterPane->parentWidget());
    if (!parentSplit) {
        m_rootSplitter->addWidget(newPane);
        return;
    }

    if (parentSplit->orientation() == orient) {
        parentSplit->setHandleWidth(1);
        const int idx = parentSplit->indexOf(afterPane);
        parentSplit->insertWidget(idx + 1, newPane);
        // Equalise sizes across the parent splitter.
        QList<int> sizes;
        const int total = (orient == Qt::Horizontal
                            ? parentSplit->width() : parentSplit->height());
        const int each = (parentSplit->count() > 0)
            ? total / parentSplit->count() : total;
        for (int i = 0; i < parentSplit->count(); ++i) sizes.append(each);
        parentSplit->setSizes(sizes);
    } else {
        // Orientation differs: wrap the active pane in a new splitter so
        // the tree grows recursively.
        const int idx = parentSplit->indexOf(afterPane);
        auto *nested = new QSplitter(orient, parentSplit);
        nested->setChildrenCollapsible(false);
        nested->setHandleWidth(1);
        // Move afterPane into the nested splitter.
        afterPane->setParent(nested);
        nested->addWidget(afterPane);
        nested->addWidget(newPane);
        const int total = (orient == Qt::Horizontal
                            ? nested->width() : nested->height());
        const int half = (total > 0) ? total / 2 : 200;
        nested->setSizes({half, half});
        parentSplit->insertWidget(idx, nested);
    }
}

void SessionTab::splitActive(Qt::Orientation orient)
{
    auto *mw = qobject_cast<MainWindow*>(window());
    if (!mw || !m_activePane) return;

    // Duplicate the active pane's profile for the new session.
    const session_entry_t entry = m_activePane->entry();
    ISession *fresh = mw->makeSessionFor(mw->profile(), entry);
    if (!fresh) return;

    Pane *newPane = createPane(fresh);
    addPaneToSplitter(newPane, m_activePane, orient);
    m_panes.append(newPane);

    emit paneAdded(newPane);

    if (m_broadcastEnabled)
        newPane->setBroadcastMember(true);

    if (auto *term = newPane->terminal()) {
        term->setFocus(Qt::OtherFocusReason);
        fresh->resize(term->cols(), term->rows());
    }
    fresh->start();
    setActivePane(newPane);
}

void SessionTab::closeActivePane()
{
    if (!m_activePane) return;
    if (m_panes.size() == 1) {
        // Last pane — close the whole tab.
        emit tabEmpty();
        return;
    }
    Pane *target = m_activePane;
    removePane(target);
}

void SessionTab::removePane(Pane *pane)
{
    if (!pane) return;

    emit paneRemoving(pane);

    pane->suppressAutoReconnect();
    m_panes.removeAll(pane);

    auto *parentSplit = qobject_cast<QSplitter*>(pane->parentWidget());
    pane->setParent(nullptr);
    pane->deleteLater();

    // Flatten a splitter that now has a single child, unless it's the
    // root splitter (we keep the root even if it holds just one pane).
    while (parentSplit && parentSplit != m_rootSplitter &&
           parentSplit->count() == 1) {
        auto *only = parentSplit->widget(0);
        auto *grand = qobject_cast<QSplitter*>(parentSplit->parentWidget());
        if (!grand) break;
        const int idx = grand->indexOf(parentSplit);
        only->setParent(grand);
        grand->insertWidget(idx, only);
        parentSplit->setParent(nullptr);
        parentSplit->deleteLater();
        parentSplit = grand;
    }

    // Pick a surviving pane as the active one.
    if (!m_panes.isEmpty()) {
        Pane *next = m_panes.first();
        setActivePane(next);
        if (next->terminal())
            next->terminal()->setFocus(Qt::OtherFocusReason);
    } else {
        m_activePane = nullptr;
        emit tabEmpty();
    }
}

void SessionTab::setActivePane(Pane *pane)
{
    if (m_activePane == pane) return;
    if (m_activePane) m_activePane->setActive(false);
    m_activePane = pane;
    if (m_activePane) {
        m_activePane->setActive(true);
        // Reset FindBar counts — different stream, different matches.
        if (m_findBar) {
            if (auto *t = m_activePane->terminal()) t->clearFind();
        }
        if (m_buttons) {
            m_buttons->setMarkActive(!m_activePane->markPattern().isEmpty());
        }
    }
}

void SessionTab::onAppFocusChanged(QWidget *, QWidget *now)
{
    if (!now) return;
    for (Pane *p : m_panes) {
        if (!p) continue;
        if (p == now || p->isAncestorOf(now)) {
            setActivePane(p);
            return;
        }
    }
}

void SessionTab::setBroadcastEnabled(bool on)
{
    if (m_broadcastEnabled == on) return;
    m_broadcastEnabled = on;
    refreshBroadcastBadges();
}

void SessionTab::refreshBroadcastBadges()
{
    for (Pane *p : m_panes) {
        if (p) p->setBroadcastMember(m_broadcastEnabled);
    }
}

// -------- Button / FindBar / Loop / Mark handlers -------------------------

void SessionTab::onButtonAction(const QString &actionString)
{
    if (m_activePane && m_activePane->engine())
        m_activePane->engine()->executeAction(actionString);
}

void SessionTab::onCommandEntered(const QString &cmd)
{
    if (!m_activePane || !m_activePane->session()) return;
    QByteArray bytes = cmd.toUtf8();
    bytes.append('\r');
    m_activePane->session()->sendBytes(bytes);
}

void SessionTab::showFindBar(bool markPreset)
{
    if (!m_findBar) return;
    m_findBar->show();
    const QString markPat = m_activePane ? m_activePane->markPattern() : QString();
    if (markPreset) {
        m_findBar->setPattern(markPat);
        m_findBar->setMarkActive(!markPat.isEmpty());
    } else {
        m_findBar->setMarkActive(!markPat.isEmpty());
    }
    m_findBar->focusInput();
    if (auto *term = terminal()) {
        if (!m_findBar->pattern().isEmpty()) {
            const int total = term->findAll(m_findBar->pattern(),
                                            m_findBar->caseSensitive(),
                                            m_findBar->regex());
            m_findBar->setMatchInfo(term->findCurrentIndex(), total);
        }
        // Forward find-index updates to the bar for whichever pane is active.
        connect(term, &TerminalWidget::findIndexChanged,
                m_findBar, &FindBar::setMatchInfo,
                Qt::UniqueConnection);
    }
}

void SessionTab::configureMark()
{
    showFindBar(true);
}

void SessionTab::clearMark()
{
    if (m_activePane) m_activePane->clearMark();
    if (m_buttons) m_buttons->setMarkActive(false);
}

void SessionTab::onMarkClicked()
{
    if (m_activePane && !m_activePane->markPattern().isEmpty()) return;
    configureMark();
}

void SessionTab::onMarkRightClicked()
{
    if (!m_activePane || m_activePane->markPattern().isEmpty())
        configureMark();
    else
        clearMark();
}

void SessionTab::onMarkDoubleClicked()
{
    if (m_activePane && !m_activePane->markPattern().isEmpty())
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
    if (m_loopTimer.isActive()) return;
    if (m_loopAction.isEmpty() || m_loopIntervalSec <= 0)
        configureLoop();
    startLoop();
}

void SessionTab::onLoopRightClicked()
{
    if (m_loopTimer.isActive())
        stopLoop();
    else
        configureLoop();
}

void SessionTab::onLoopDoubleClicked()
{
    if (m_loopTimer.isActive()) {
        stopLoop();
    } else {
        configureLoop();
        startLoop();
    }
}

void SessionTab::onLoopTick()
{
    if (m_activePane && m_activePane->engine() && !m_loopAction.isEmpty())
        m_activePane->engine()->executeAction(
            m_loopAction + QStringLiteral("\\r"));
    if (m_buttons)
        m_buttons->flashLoopingButton();
}

void SessionTab::onButtonLoopRequested(const QString &action)
{
    if (m_loopTimer.isActive() && m_loopAction == action) {
        stopLoop();
        return;
    }
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
           "<li>Ctrl+Shift+H / V splits the active pane.</li>"
           "<li>Ctrl+Shift+W closes the active pane.</li>"
           "<li>Ctrl+Shift+B toggles input broadcast across panes.</li>"
           "<li>Resize the window to update the remote PTY size.</li>"
           "<li>Settings &rarr; Preferences (Ctrl+,) edits everything.</li>"
           "</ul>"));
}

} // namespace tscrt
