#include "MainWindow.h"

#include "ButtonBar.h"
#include "CommandLineWidget.h"
#include "Credentials.h"
#include "DiagnosticExporter.h"
#include "HelpDialog.h"
#include "ISession.h"
#include "SerialSession.h"
#include "SessionEditDialog.h"
#include "SessionHistory.h"
#include "SessionManagerDialog.h"
#include "Pane.h"
#include "SessionTab.h"
#include "LogSettingsDialog.h"
#include "SettingsDialog.h"
#include "SnapshotBrowserDialog.h"
#include "SnapshotManager.h"
#include "SnapshotsDialog.h"
#include "SshSession.h"
#include "TerminalWidget.h"

#include "profile.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QSettings>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QShortcut>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtGlobal>

#include <libssh2.h>
#include <vterm.h>

#include <cstring>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    allWindows().append(this);
    setWindowTitle(QStringLiteral("TSCRT"));
    setAcceptDrops(true);
    resize(1100, 720);

    // -- Empty-state page (logo background) --
    m_emptyPage = new QWidget(this);
    m_emptyPage->setStyleSheet(QStringLiteral("background: white;"));
    auto *emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->setAlignment(Qt::AlignCenter);
    auto *logoLabel = new QLabel(m_emptyPage);
    QPixmap logo(QStringLiteral(":/images/airix_bg.png"));
    logoLabel->setPixmap(logo.scaled(480, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(logoLabel);

    // -- Tab widget --
    m_tabs = new QTabWidget(this);
    // We install our own tiny close button per tab (see openSessionByIndex),
    // so the default closable machinery is disabled.
    m_tabs->setTabsClosable(false);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setStyleSheet(QStringLiteral(
        "QTabBar::tab:selected {"
        "    background: #aed6f1;"
        "    color: #1a1a1a;"
        "}"
        "QTabBar::tab {"
        "    padding: 4px 6px 4px 10px;"
        "}"));
    connect(m_tabs, &QTabWidget::currentChanged,
            this, &MainWindow::onCurrentTabChanged);
    connect(m_tabs->tabBar(), &QTabBar::tabBarDoubleClicked,
            this, &MainWindow::onTabBarDoubleClicked);
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested,
            this, &MainWindow::onTabBarContextMenu);
    m_tabs->tabBar()->installEventFilter(this);
    m_tabs->tabBar()->setAcceptDrops(true);

    // -- Stacked central widget --
    m_central = new QStackedWidget(this);
    m_central->addWidget(m_emptyPage);  // index 0
    m_central->addWidget(m_tabs);       // index 1
    m_central->setCurrentWidget(m_emptyPage);
    setCentralWidget(m_central);

    // Ctrl+Alt+Left / Ctrl+Alt+Right: move between tabs without wrap-around.
    auto *prevTab = new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left), this);
    prevTab->setContext(Qt::ApplicationShortcut);
    connect(prevTab, &QShortcut::activated, this, [this] {
        const int i = m_tabs->currentIndex();
        if (i > 0)
            m_tabs->setCurrentIndex(i - 1);
    });
    auto *nextTab = new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right), this);
    nextTab->setContext(Qt::ApplicationShortcut);
    connect(nextTab, &QShortcut::activated, this, [this] {
        const int i = m_tabs->currentIndex();
        if (i >= 0 && i < m_tabs->count() - 1)
            m_tabs->setCurrentIndex(i + 1);
    });

    auto *fsToggle = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fsToggle->setContext(Qt::ApplicationShortcut);
    connect(fsToggle, &QShortcut::activated,
            this, &MainWindow::toggleFullScreen);

    loadProfile();

    // Snapshot coordinator listens to sessions as they are attached.
    m_snapshotMgr = new tscrt::SnapshotManager(this);
    m_snapshotMgr->setProfile(m_profile);
    connect(m_snapshotMgr, &tscrt::SnapshotManager::message,
            this, [this](const QString &text) {
        statusBar()->showMessage(text, 4000);
    });

    createMenus();
    createSessionDock();
    createStatusBar();
    rebuildSessionsMenu();
    rebuildSnapshotsMenu();
    rebuildSessionTree();
    loadSettings();

    // Apply persisted view-toggle preferences after createStatusBar.
    {
        QSettings prefs;
        const bool showStatus = prefs.value(QStringLiteral("ui/showStatusBar"), true).toBool();
        statusBar()->setVisible(showStatus);
    }

    restorePinnedTabs();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QMainWindow::keyPressEvent(event);
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("MainWindow"));
    s.setValue(QStringLiteral("geometry"),  saveGeometry());
    s.setValue(QStringLiteral("state"),     saveState());
    s.endGroup();

    // Persist pinned tabs (profile index + display name).
    QList<QVariant> pinned;
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget *page = m_tabs->widget(i);
        if (page && page->property("tscrtPinned").toBool()) {
            const QVariant v = page->property("tscrtProfileIndex");
            if (v.isValid()) {
                QVariantMap entry;
                entry[QStringLiteral("index")] = v.toInt();
                entry[QStringLiteral("name")]  = m_tabs->tabText(i);
                pinned.append(entry);
            }
        }
    }
    s.setValue(QStringLiteral("pinnedTabs"), pinned);
}

void MainWindow::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("MainWindow"));
    const auto geom = s.value(QStringLiteral("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    const auto state = s.value(QStringLiteral("state")).toByteArray();
    if (!state.isEmpty())
        restoreState(state);
    s.endGroup();
}

MainWindow::~MainWindow()
{
    allWindows().removeOne(this);
    // If this was the last window, quit the application.
    if (allWindows().isEmpty())
        QApplication::quit();
}

void MainWindow::loadProfile()
{
    if (profile_init(&m_profile) != 0) {
        QMessageBox::critical(this, tr("Profile error"),
            tr("Cannot initialize profile directory."));
        return;
    }
    if (profile_load(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Profile error"),
            tr("Failed to load profile; defaults will be used."));
    }
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    m_actCloseTab = new QAction(tr("&Close tab"), this);
    // Ctrl+F4 so we don't collide with Ctrl+Shift+W (Close Pane in the
    // View menu). Closing the last pane already auto-closes the tab, so
    // Ctrl+Shift+W effectively closes tabs with a single pane too.
    m_actCloseTab->setShortcut(QKeySequence(tr("Ctrl+F4")));
    connect(m_actCloseTab, &QAction::triggered, this, &MainWindow::closeCurrentTab);
    fileMenu->addAction(m_actCloseTab);

    fileMenu->addSeparator();

    // Export ▸
    auto *exportMenu = fileMenu->addMenu(tr("&Export"));
    auto *actExportAll = exportMenu->addAction(tr("&Full profile..."));
    connect(actExportAll, &QAction::triggered, this, &MainWindow::exportProfile);
    auto *actExportSessions = exportMenu->addAction(tr("&Sessions only..."));
    connect(actExportSessions, &QAction::triggered,
            this, &MainWindow::exportSessions);
    auto *actExportSnapshots = exportMenu->addAction(tr("S&napshots only..."));
    connect(actExportSnapshots, &QAction::triggered,
            this, &MainWindow::exportSnapshots);

    // Import ▸
    auto *importMenu = fileMenu->addMenu(tr("&Import"));
    auto *actImportAll = importMenu->addAction(tr("&Full profile..."));
    connect(actImportAll, &QAction::triggered, this, &MainWindow::importProfile);
    auto *actImportSessions = importMenu->addAction(tr("&Sessions only..."));
    connect(actImportSessions, &QAction::triggered,
            this, &MainWindow::importSessions);
    auto *actImportSnapshots = importMenu->addAction(tr("S&napshots only..."));
    connect(actImportSnapshots, &QAction::triggered,
            this, &MainWindow::importSnapshots);

    fileMenu->addSeparator();

    // Logs ▸
    m_logsMenu = fileMenu->addMenu(tr("&Logs"));
    {
        auto *openAct = new QAction(tr("&Open log folder"), this);
        connect(openAct, &QAction::triggered,
                this, &MainWindow::openLogFolder);
        m_logsMenu->addAction(openAct);

        m_logsMenu->addSeparator();

        auto *settingsAct = new QAction(tr("&Log settings..."), this);
        connect(settingsAct, &QAction::triggered,
                this, &MainWindow::showLogSettingsDialog);
        m_logsMenu->addAction(settingsAct);
    }

    fileMenu->addSeparator();

    m_actQuit = new QAction(tr("E&xit"), this);
    m_actQuit->setShortcut(QKeySequence::Quit);
    connect(m_actQuit, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(m_actQuit);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    auto *actFind  = new QAction(tr("&Find..."), this);
    actFind->setShortcut(QKeySequence::Find);
    connect(actFind, &QAction::triggered, this, [this]() {
        if (auto *tab = qobject_cast<tscrt::SessionTab *>(
                m_tabs->currentWidget()))
            tab->showFindBar(false);
    });
    editMenu->addAction(actFind);

    m_sessionsMenu = menuBar()->addMenu(tr("&Sessions"));

    m_snapshotsMenu = menuBar()->addMenu(tr("S&napshots"));

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    auto *viewMenu = m_viewMenu;

    m_actViewCmdLine = new QAction(tr("Show &Command Line"), this);
    m_actViewCmdLine->setCheckable(true);
    m_actViewButtons = new QAction(tr("Show Action &Buttons"), this);
    m_actViewButtons->setCheckable(true);
    m_actViewStatus  = new QAction(tr("Show &Status Bar"), this);
    m_actViewStatus->setCheckable(true);
    {
        QSettings prefs;
        m_actViewCmdLine->setChecked(
            prefs.value(QStringLiteral("ui/showCmdLine"),  true).toBool());
        m_actViewButtons->setChecked(
            prefs.value(QStringLiteral("ui/showButtonBar"), true).toBool());
        m_actViewStatus->setChecked(
            prefs.value(QStringLiteral("ui/showStatusBar"), true).toBool());
    }
    connect(m_actViewCmdLine, &QAction::toggled,
            this, &MainWindow::toggleCmdLines);
    connect(m_actViewButtons, &QAction::toggled,
            this, &MainWindow::toggleButtonBars);
    connect(m_actViewStatus, &QAction::toggled,
            this, &MainWindow::toggleStatusBar);
    viewMenu->addAction(m_actViewCmdLine);
    viewMenu->addAction(m_actViewButtons);
    viewMenu->addAction(m_actViewStatus);
    viewMenu->addSeparator();

    // Split / broadcast actions operate on the current tab.
    // Naming convention follows the divider line (tmux/iTerm style):
    //   Horizontal split = horizontal divider line = top/bottom panes
    //   Vertical split   = vertical divider line   = left/right panes
    // That's the opposite of Qt::Orientation, so the arguments look
    // inverted on purpose.
    auto *actSplitH = new QAction(tr("Split Pane &Horizontally"), this);
    actSplitH->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+H")));
    connect(actSplitH, &QAction::triggered, this, [this] {
        if (auto *tab = qobject_cast<tscrt::SessionTab*>(m_tabs->currentWidget()))
            tab->splitActive(Qt::Vertical);
    });
    viewMenu->addAction(actSplitH);

    auto *actSplitV = new QAction(tr("Split Pane &Vertically"), this);
    actSplitV->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    connect(actSplitV, &QAction::triggered, this, [this] {
        if (auto *tab = qobject_cast<tscrt::SessionTab*>(m_tabs->currentWidget()))
            tab->splitActive(Qt::Horizontal);
    });
    viewMenu->addAction(actSplitV);

    auto *actClosePane = new QAction(tr("Close &Pane"), this);
    actClosePane->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+W")));
    connect(actClosePane, &QAction::triggered, this, [this] {
        if (auto *tab = qobject_cast<tscrt::SessionTab*>(m_tabs->currentWidget()))
            tab->closeActivePane();
    });
    viewMenu->addAction(actClosePane);

    auto *actBroadcast = new QAction(tr("&Broadcast Input"), this);
    actBroadcast->setCheckable(true);
    actBroadcast->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    connect(actBroadcast, &QAction::toggled, this, [this](bool on) {
        if (auto *tab = qobject_cast<tscrt::SessionTab*>(m_tabs->currentWidget()))
            tab->setBroadcastEnabled(on);
    });
    viewMenu->addAction(actBroadcast);

    // Register the split/broadcast actions on the MainWindow itself so
    // their shortcuts remain active when the menu bar is hidden in
    // fullscreen mode. Without this, Qt disables shortcuts for actions
    // that belong exclusively to a hidden menu.
    addAction(actSplitH);
    addAction(actSplitV);
    addAction(actClosePane);
    addAction(actBroadcast);

    // Reset the broadcast toggle to reflect the current tab when the
    // user switches tabs.
    connect(m_tabs, &QTabWidget::currentChanged, this, [this, actBroadcast](int) {
        auto *tab = qobject_cast<tscrt::SessionTab*>(m_tabs->currentWidget());
        actBroadcast->blockSignals(true);
        actBroadcast->setChecked(tab && tab->broadcastEnabled());
        actBroadcast->blockSignals(false);
    });

    viewMenu->addSeparator();
    m_actFullScreen = new QAction(tr("&Full Screen\tF11"), this);
    m_actFullScreen->setCheckable(true);
    connect(m_actFullScreen, &QAction::triggered,
            this, &MainWindow::toggleFullScreen);
    viewMenu->addAction(m_actFullScreen);

    auto *settingsMenu = menuBar()->addMenu(tr("Se&ttings"));
    m_actSettings = new QAction(tr("&Preferences..."), this);
    m_actSettings->setShortcut(QKeySequence(tr("Ctrl+,")));
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::showSettingsDialog);
    settingsMenu->addAction(m_actSettings);

    auto *langMenu = settingsMenu->addMenu(tr("&Language"));
    struct LangEntry { const char *code; const char *label; };
    static const LangEntry langs[] = {
        { "en", "English" },   { "ko", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4" }, // 한국어
        { "ja", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" }, // 日本語
        { "zh", "\xE4\xB8\xAD\xE6\x96\x87" },               // 中文
        { "es", "Espa\xC3\xB1ol" },    { "fr", "Fran\xC3\xA7\x61is" },
        { "de", "Deutsch" },            { "pt", "Portugu\xC3\xAAs" },
        { "ru", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9" }, // Русский
        { "it", "Italiano" },
    };
    {
        QSettings prefs;
        const QString cur = prefs.value(QStringLiteral("ui/language"),
                                        QStringLiteral("en")).toString();
        for (const auto &l : langs) {
            auto *act = langMenu->addAction(QString::fromUtf8(l.label));
            act->setCheckable(true);
            act->setChecked(cur == QLatin1String(l.code));
            const QString code = QString::fromLatin1(l.code);
            connect(act, &QAction::triggered, this, [this, code] {
                setUiLanguage(code);
            });
        }
    }

    m_actReload = new QAction(tr("&Reload profile"), this);
    m_actReload->setShortcut(QKeySequence(tr("F5")));
    connect(m_actReload, &QAction::triggered, this, [this] {
        loadProfile();
        if (m_snapshotMgr) m_snapshotMgr->setProfile(m_profile);
        rebuildSessionsMenu();
        rebuildSnapshotsMenu();
        rebuildSessionTree();
        statusBar()->showMessage(tr("Profile reloaded."), 2500);
    });
    settingsMenu->addAction(m_actReload);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto *actUsage = new QAction(tr("&Usage guide..."), this);
    connect(actUsage, &QAction::triggered, this, [this] {
        HelpDialog dlg(this);
        dlg.exec();
    });
    helpMenu->addAction(actUsage);

    auto *actDiag = new QAction(tr("&Export diagnostics..."), this);
    connect(actDiag, &QAction::triggered, this, &MainWindow::exportDiagnostics);
    helpMenu->addAction(actDiag);

    helpMenu->addSeparator();
    m_actAbout = new QAction(tr("&About TSCRT..."), this);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
    helpMenu->addAction(m_actAbout);
}

void MainWindow::createSessionDock()
{
    m_sessionDock = new QDockWidget(tr("Session Manager"), this);
    m_sessionDock->setObjectName(QStringLiteral("SessionDock"));
    m_sessionDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_sessionDock->setFeatures(QDockWidget::DockWidgetMovable
                               | QDockWidget::DockWidgetClosable);

    m_sessionTree = new QTreeWidget(m_sessionDock);
    m_sessionTree->setHeaderHidden(true);
    m_sessionTree->setRootIsDecorated(true);
    m_sessionTree->setUniformRowHeights(true);
    m_sessionTree->setIndentation(14);
    m_sessionTree->setStyleSheet(QStringLiteral(
        "QTreeWidget { background:#1e1e1e; color:#e0e0e0;"
        "              border:0; outline:0;"
        "              selection-background-color:#3a5f8a;"
        "              selection-color:#ffffff; }"
        "QTreeWidget::item { padding:3px 2px; }"
        "QTreeWidget::item:hover { background:#2a2a2a; }"));
    connect(m_sessionTree, &QTreeWidget::itemActivated,
            this, &MainWindow::onSessionTreeActivated);

    m_sessionTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sessionTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::onSessionTreeContextMenu);

    m_sessionDock->setWidget(m_sessionTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_sessionDock);

    if (m_viewMenu) {
        m_viewMenu->addSeparator();
        m_viewMenu->addAction(m_sessionDock->toggleViewAction());
    }
}

void MainWindow::createStatusBar()
{
    auto makeSep = [this] {
        auto *l = new QLabel(QStringLiteral("·"), this);
        l->setStyleSheet(QStringLiteral("color:#666;"));
        return l;
    };

    m_lblProto = new QLabel(this);
    m_lblHost  = new QLabel(this);
    m_lblGrid  = new QLabel(this);
    m_lblProto->setStyleSheet(QStringLiteral("color:#1a4d80; font-weight:bold;"));
    m_lblHost ->setStyleSheet(QStringLiteral("color:#202020;"));
    m_lblGrid ->setStyleSheet(QStringLiteral("color:#555;"));

    statusBar()->addPermanentWidget(m_lblProto);
    statusBar()->addPermanentWidget(makeSep());
    statusBar()->addPermanentWidget(m_lblHost);
    statusBar()->addPermanentWidget(makeSep());
    statusBar()->addPermanentWidget(m_lblGrid);

    statusBar()->showMessage(
        tr("Ready · libssh2 %1 · libvterm %2.%3")
            .arg(QString::fromLatin1(LIBSSH2_VERSION))
            .arg(VTERM_VERSION_MAJOR)
            .arg(VTERM_VERSION_MINOR));

    updateStatusForCurrentTab();
}

void MainWindow::updateStatusForCurrentTab()
{
    if (!m_lblProto) return;
    auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->currentWidget());
    if (!tab) {
        m_lblProto->setText(QString());
        m_lblHost ->setText(QString());
        m_lblGrid ->setText(QString());
        return;
    }
    m_lblProto->setText(tab->property("tscrtProto").toString());
    m_lblHost ->setText(tab->property("tscrtHost").toString());
    if (tab->terminal()) {
        m_lblGrid->setText(QStringLiteral("%1x%2")
                               .arg(tab->terminal()->cols())
                               .arg(tab->terminal()->rows()));
    } else {
        m_lblGrid->setText(QString());
    }
}

void MainWindow::onCurrentTabChanged(int /*index*/)
{
    updateStatusForCurrentTab();
    updateCentralView();
}

void MainWindow::updateCentralView()
{
    m_central->setCurrentWidget(m_tabs->count() > 0 ? m_tabs : m_emptyPage);
}

void MainWindow::onSessionTreeActivated(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    const QVariant v = item->data(0, Qt::UserRole);
    if (!v.isValid()) return;
    bool ok = false;
    const int idx = v.toInt(&ok);
    if (ok && idx >= 0)
        openSessionByIndex(idx);
}

void MainWindow::onSessionTreeContextMenu(const QPoint &pos)
{
    if (!m_sessionTree) return;
    QTreeWidgetItem *item = m_sessionTree->itemAt(pos);
    int profileIndex = -1;
    if (item) {
        const QVariant v = item->data(0, Qt::UserRole);
        bool ok = false;
        if (v.isValid()) {
            const int idx = v.toInt(&ok);
            if (ok && idx >= 0 && idx < m_profile.session_count)
                profileIndex = idx;
        }
    }

    QMenu menu(m_sessionTree);

    QAction *actOpen   = menu.addAction(tr("Open"));
    menu.addSeparator();
    QAction *actRename = menu.addAction(tr("Rename..."));
    QAction *actEdit   = menu.addAction(tr("Edit..."));
    QAction *actDelete = menu.addAction(tr("Delete"));
    menu.addSeparator();
    QAction *actCopy   = menu.addAction(tr("Copy"));
    QAction *actPaste  = menu.addAction(tr("Paste"));

    const bool onSession = (profileIndex >= 0);
    actOpen  ->setEnabled(onSession);
    actRename->setEnabled(onSession);
    actEdit  ->setEnabled(onSession);
    actDelete->setEnabled(onSession);
    actCopy  ->setEnabled(onSession);
    actPaste ->setEnabled(m_sessionClipboardValid
                          && m_profile.session_count < MAX_SESSIONS);

    QAction *chosen = menu.exec(m_sessionTree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actOpen)        openSessionByIndex(profileIndex);
    else if (chosen == actRename) renameSessionByIndex(profileIndex);
    else if (chosen == actEdit)   editSessionByIndex(profileIndex);
    else if (chosen == actDelete) deleteSessionByIndex(profileIndex);
    else if (chosen == actCopy)   copySessionByIndex(profileIndex);
    else if (chosen == actPaste)  pasteSessionFromClipboard();
}

void MainWindow::renameSessionByIndex(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= m_profile.session_count) return;
    session_entry_t &s = m_profile.sessions[profileIndex];

    bool ok = false;
    const QString cur = QString::fromLocal8Bit(s.name);
    const QString next = QInputDialog::getText(this, tr("Rename session"),
        tr("New name:"), QLineEdit::Normal, cur, &ok);
    if (!ok) return;
    const QString trimmed = next.trimmed();
    if (trimmed.isEmpty()) {
        QMessageBox::warning(this, tr("Rename"), tr("Name cannot be empty."));
        return;
    }

    const QByteArray b = trimmed.toLocal8Bit();
    const int n = qMin<int>(int(sizeof(s.name)) - 1, b.size());
    memcpy(s.name, b.constData(), n);
    s.name[n] = '\0';

    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("Could not write profile."));
        return;
    }
    rebuildSessionsMenu();
    rebuildSnapshotsMenu();
    rebuildSessionTree();
}

void MainWindow::editSessionByIndex(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= m_profile.session_count) return;

    // Decrypt password into a temporary copy for editing.
    session_entry_t edit = m_profile.sessions[profileIndex];
    if (edit.type == SESSION_TYPE_SSH && edit.ssh.password[0]) {
        const QString plain = tscrt::decryptSecret(
            QString::fromLocal8Bit(edit.ssh.password));
        const QByteArray b = plain.toLocal8Bit();
        const int n = qMin<int>(int(sizeof(edit.ssh.password)) - 1, b.size());
        memcpy(edit.ssh.password, b.constData(), n);
        edit.ssh.password[n] = '\0';
    }

    SessionEditDialog dlg(this);
    dlg.setSession(edit);
    if (dlg.exec() != QDialog::Accepted) return;

    session_entry_t s = dlg.session();
    if (s.name[0] == '\0') {
        QMessageBox::warning(this, tr("Edit"), tr("Name cannot be empty."));
        return;
    }
    if (s.type == SESSION_TYPE_SSH && s.ssh.password[0]) {
        const QString enc = tscrt::encryptSecret(
            QString::fromLocal8Bit(s.ssh.password));
        const QByteArray b = enc.toLocal8Bit();
        const int n = qMin<int>(int(sizeof(s.ssh.password)) - 1, b.size());
        memcpy(s.ssh.password, b.constData(), n);
        s.ssh.password[n] = '\0';
    }

    m_profile.sessions[profileIndex] = s;
    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("Could not write profile."));
        return;
    }
    rebuildSessionsMenu();
    rebuildSnapshotsMenu();
    rebuildSessionTree();
}

void MainWindow::deleteSessionByIndex(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= m_profile.session_count) return;
    const QString name = QString::fromLocal8Bit(m_profile.sessions[profileIndex].name);
    const auto reply = QMessageBox::question(this, tr("Delete session"),
        tr("Delete session \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    for (int i = profileIndex; i < m_profile.session_count - 1; ++i)
        m_profile.sessions[i] = m_profile.sessions[i + 1];
    --m_profile.session_count;
    memset(&m_profile.sessions[m_profile.session_count], 0,
           sizeof(m_profile.sessions[0]));

    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("Could not write profile."));
        return;
    }
    tscrt::pruneOrphanHistoryFiles(m_profile);
    rebuildSessionsMenu();
    rebuildSnapshotsMenu();
    rebuildSessionTree();
}

void MainWindow::copySessionByIndex(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= m_profile.session_count) return;
    m_sessionClipboard      = m_profile.sessions[profileIndex];
    m_sessionClipboardValid = true;
    statusBar()->showMessage(
        tr("Copied session \"%1\".")
            .arg(QString::fromLocal8Bit(m_sessionClipboard.name)),
        2500);
}

void MainWindow::pasteSessionFromClipboard()
{
    if (!m_sessionClipboardValid) return;
    if (m_profile.session_count >= MAX_SESSIONS) {
        QMessageBox::warning(this, tr("Paste"),
            tr("Profile already holds the maximum number of sessions (%1).")
                .arg(MAX_SESSIONS));
        return;
    }

    session_entry_t s = m_sessionClipboard;

    // Generate a unique name by appending " (copy)", " (copy 2)", ...
    const QString base = QString::fromLocal8Bit(s.name);
    auto nameInUse = [this](const QString &candidate) {
        for (int i = 0; i < m_profile.session_count; ++i) {
            if (candidate == QString::fromLocal8Bit(m_profile.sessions[i].name))
                return true;
        }
        return false;
    };
    QString candidate = base + QStringLiteral(" (copy)");
    int n = 2;
    while (nameInUse(candidate)) {
        candidate = base + QStringLiteral(" (copy %1)").arg(n++);
    }
    const QByteArray nb = candidate.toLocal8Bit();
    const int nn = qMin<int>(int(sizeof(s.name)) - 1, nb.size());
    memcpy(s.name, nb.constData(), nn);
    s.name[nn] = '\0';

    m_profile.sessions[m_profile.session_count++] = s;
    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("Could not write profile."));
        --m_profile.session_count;
        return;
    }
    rebuildSessionsMenu();
    rebuildSnapshotsMenu();
    rebuildSessionTree();
}

/* Paint a small DB-9 (RS-232) connector icon programmatically so we
 * don't need an extra resource file. Two rows of pin dots inside a
 * trapezoid. */
static QIcon makeSerialIcon()
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Trapezoidal connector body.
    QPolygonF body;
    body << QPointF(2, 4) << QPointF(14, 4)
         << QPointF(13, 12) << QPointF(3, 12);
    p.setPen(QPen(QColor("#202020"), 1.2));
    p.setBrush(QColor("#9e9e9e"));
    p.drawPolygon(body);

    // 5+4 pin dots.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#1a1a1a"));
    const qreal r = 0.9;
    for (int i = 0; i < 5; ++i)
        p.drawEllipse(QPointF(4 + i * 2, 6.5), r, r);
    for (int i = 0; i < 4; ++i)
        p.drawEllipse(QPointF(5 + i * 2, 9.5), r, r);

    return QIcon(pm);
}

void MainWindow::rebuildSessionTree()
{
    if (!m_sessionTree) return;
    m_sessionTree->clear();

    static const QIcon serialIcon = makeSerialIcon();

    QStyle *st = style();
    auto *sshGroup = new QTreeWidgetItem(m_sessionTree);
    sshGroup->setText(0, tr("SSH Sessions"));
    sshGroup->setIcon(0, st->standardIcon(QStyle::SP_DriveNetIcon));
    QFont gf = sshGroup->font(0); gf.setBold(true); sshGroup->setFont(0, gf);

    auto *serGroup = new QTreeWidgetItem(m_sessionTree);
    serGroup->setText(0, tr("Serial Sessions"));
    serGroup->setIcon(0, serialIcon);
    serGroup->setFont(0, gf);

    for (int i = 0; i < m_profile.session_count; ++i) {
        const session_entry_t &s = m_profile.sessions[i];
        auto *item = new QTreeWidgetItem();
        item->setData(0, Qt::UserRole, i);
        if (s.type == SESSION_TYPE_SSH) {
            item->setText(0, QStringLiteral("%1  (%2@%3:%4)")
                                 .arg(QString::fromLocal8Bit(s.name),
                                      QString::fromLocal8Bit(s.ssh.username),
                                      QString::fromLocal8Bit(s.ssh.host))
                                 .arg(s.ssh.port));
            item->setIcon(0, st->standardIcon(QStyle::SP_ComputerIcon));
            sshGroup->addChild(item);
        } else {
            item->setText(0, QStringLiteral("%1  (%2 %3)")
                                 .arg(QString::fromLocal8Bit(s.name),
                                      QString::fromLocal8Bit(s.serial.device))
                                 .arg(s.serial.baudrate));
            item->setIcon(0, serialIcon);
            serGroup->addChild(item);
        }
    }
    sshGroup->setExpanded(true);
    serGroup->setExpanded(true);
}

void MainWindow::rebuildSessionsMenu()
{
    if (!m_sessionsMenu)
        return;
    m_sessionsMenu->clear();

    auto *newAct = new QAction(tr("&New"), this);
    newAct->setShortcut(QKeySequence(tr("Ctrl+Shift+N")));
    connect(newAct, &QAction::triggered, this, &MainWindow::newSession);
    m_sessionsMenu->addAction(newAct);

    auto *mgrAct = new QAction(tr("&Sessions..."), this);
    mgrAct->setShortcut(QKeySequence(tr("Ctrl+Shift+M")));
    connect(mgrAct, &QAction::triggered, this, &MainWindow::showSessionManagerDialog);
    m_sessionsMenu->addAction(mgrAct);

    m_sessionsMenu->addSeparator();

    if (m_profile.session_count == 0) {
        auto *empty = new QAction(tr("(no sessions defined)"), this);
        empty->setEnabled(false);
        m_sessionsMenu->addAction(empty);
        return;
    }

    for (int i = 0; i < m_profile.session_count; ++i) {
        const session_entry_t &s = m_profile.sessions[i];
        QString label;
        if (s.type == SESSION_TYPE_SSH) {
            label = tr("SSH · %1 (%2@%3:%4)")
                        .arg(QString::fromLocal8Bit(s.name),
                             QString::fromLocal8Bit(s.ssh.username),
                             QString::fromLocal8Bit(s.ssh.host))
                        .arg(s.ssh.port);
        } else {
            label = tr("Serial · %1 (%2 %3 baud)")
                        .arg(QString::fromLocal8Bit(s.name),
                             QString::fromLocal8Bit(s.serial.device))
                        .arg(s.serial.baudrate);
        }
        auto *act = new QAction(label, this);
        connect(act, &QAction::triggered, this, [this, i] {
            openSessionByIndex(i);
        });
        m_sessionsMenu->addAction(act);
    }
}

void MainWindow::rebuildSnapshotsMenu()
{
    if (!m_snapshotsMenu) return;
    m_snapshotsMenu->clear();

    // Run submenu targets the currently active session tab.
    QMenu *runMenu = m_snapshotsMenu->addMenu(tr("&Run on current session"));
    if (m_profile.snapshot_count == 0) {
        QAction *empty = runMenu->addAction(tr("(no snapshots defined)"));
        empty->setEnabled(false);
    } else {
        for (int i = 0; i < m_profile.snapshot_count; ++i) {
            const QString sname =
                QString::fromLocal8Bit(m_profile.snapshots[i].name);
            QAction *a = runMenu->addAction(sname);
            if (i == 0)
                a->setShortcut(QKeySequence(tr("Ctrl+Shift+S")));
            connect(a, &QAction::triggered, this, [this, sname] {
                auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->currentWidget());
                if (tab && m_snapshotMgr)
                    m_snapshotMgr->runSnapshotByName(
                        tab->session(), tab->displayName(), sname);
            });
        }
    }

    m_snapshotsMenu->addSeparator();

    auto *manageAct = new QAction(tr("&Manage snapshots..."), this);
    connect(manageAct, &QAction::triggered,
            this, &MainWindow::showSnapshotsDialog);
    m_snapshotsMenu->addAction(manageAct);

    auto *rulesAct = new QAction(tr("&Automation rules..."), this);
    connect(rulesAct, &QAction::triggered,
            this, &MainWindow::showSnapshotRulesDialog);
    m_snapshotsMenu->addAction(rulesAct);

    m_snapshotsMenu->addSeparator();

    auto *browseAct = new QAction(tr("&Browse snapshots..."), this);
    connect(browseAct, &QAction::triggered, this, [this] {
        SnapshotBrowserDialog dlg(m_profile, this);
        dlg.exec();
    });
    m_snapshotsMenu->addAction(browseAct);

    auto *openFolderAct = new QAction(tr("Open snapshot &folder"), this);
    connect(openFolderAct, &QAction::triggered,
            this, &MainWindow::openSnapshotFolder);
    m_snapshotsMenu->addAction(openFolderAct);
}

void MainWindow::showSnapshotsDialog()
{
    SnapshotsDialog dlg(m_profile, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_profile = dlg.profile();
        if (profile_save(&m_profile) != 0) {
            QMessageBox::warning(this, tr("Save failed"),
                tr("Could not write profile to:\n%1")
                    .arg(QString::fromLocal8Bit(m_profile.profile_path)));
            return;
        }
        if (m_snapshotMgr) m_snapshotMgr->setProfile(m_profile);
        rebuildSnapshotsMenu();
        statusBar()->showMessage(tr("Snapshots saved."), 2500);
    }
}

void MainWindow::showSnapshotRulesDialog()
{
    SnapshotsDialog dlg(m_profile, this);
    dlg.showRulesTab();
    if (dlg.exec() == QDialog::Accepted) {
        m_profile = dlg.profile();
        if (profile_save(&m_profile) != 0) {
            QMessageBox::warning(this, tr("Save failed"),
                tr("Could not write profile to:\n%1")
                    .arg(QString::fromLocal8Bit(m_profile.profile_path)));
            return;
        }
        if (m_snapshotMgr) m_snapshotMgr->setProfile(m_profile);
        rebuildSnapshotsMenu();
        statusBar()->showMessage(tr("Snapshots saved."), 2500);
    }
}

void MainWindow::openSnapshotFolder()
{
    QString dir = QString::fromLocal8Bit(m_profile.common.log_dir);
    if (dir.isEmpty()) return;
    QDir d(dir);
    if (!d.exists(QStringLiteral("snapshots")))
        d.mkpath(QStringLiteral("snapshots"));
    d.cd(QStringLiteral("snapshots"));
    QDesktopServices::openUrl(QUrl::fromLocalFile(d.absolutePath()));
}

void MainWindow::openLogFolder()
{
    const QString configured = QString::fromLocal8Bit(m_profile.common.log_dir);

    auto isWritable = [](const QString &dir) -> bool {
        if (dir.isEmpty()) return false;
        QDir d(dir);
        if (!d.exists() && !d.mkpath(QStringLiteral("."))) return false;
        const QString probe = d.filePath(QStringLiteral(".tscrt_probe"));
        QFile f(probe);
        if (!f.open(QIODevice::WriteOnly)) return false;
        f.close();
        QFile::remove(probe);
        return true;
    };

    QString target = configured;
    bool usedFallback = false;

    if (!isWritable(target)) {
        // Configured dir is unreachable / read-only (OneDrive reparse,
        // Controlled Folder Access, ACL, etc.). Fall back to the same
        // visible top-level directory SessionLogger writes to.
#ifdef Q_OS_WIN
        target = QStringLiteral("C:/TSCRT/sessions");
#else
        target = QDir::homePath() + QStringLiteral("/TSCRT/sessions");
#endif
        QDir d(target);
        if (!d.exists()) d.mkpath(QStringLiteral("."));
        usedFallback = true;
    }

    if (target.isEmpty()) {
        QMessageBox::information(this, tr("Logs"),
            tr("Log directory is not configured yet."));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(target).absolutePath()));

    if (usedFallback) {
        statusBar()->showMessage(
            tr("Configured log directory is not writable — opened fallback: %1").arg(target),
            7000);
    }
}

void MainWindow::showLogSettingsDialog()
{
    LogSettingsDialog dlg(m_profile, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_profile = dlg.profile();
        if (profile_save(&m_profile) != 0) {
            QMessageBox::warning(this, tr("Save failed"),
                tr("Could not write profile to:\n%1")
                    .arg(QString::fromLocal8Bit(m_profile.profile_path)));
            return;
        }
        if (m_snapshotMgr) m_snapshotMgr->setProfile(m_profile);
        statusBar()->showMessage(tr("Log settings saved."), 2500);
    }
}

void MainWindow::exportProfile()
{
    // Flush any in-memory changes before we copy the file off disk.
    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Export"),
            tr("Failed to flush the current profile before exporting."));
        return;
    }

    const QString src = QString::fromLocal8Bit(m_profile.profile_path);
    const QString stamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString suggested = QStringLiteral("tscrt-%1.profile").arg(stamp);

    const QString dst = QFileDialog::getSaveFileName(
        this, tr("Export profile"), suggested,
        tr("TSCRT profile (*.profile);;All files (*)"));
    if (dst.isEmpty()) return;

    QFile::remove(dst);   // QFile::copy() refuses to overwrite
    if (!QFile::copy(src, dst)) {
        QMessageBox::warning(this, tr("Export"),
            tr("Could not copy profile to:\n%1").arg(dst));
        return;
    }

    QMessageBox::information(this, tr("Export"),
        tr("Profile exported to:\n%1\n\n"
           "Note: SSH and SMTP passwords are DPAPI-encrypted for the "
           "current Windows user and will not decrypt on another "
           "account or machine — re-enter them after importing.")
            .arg(dst));
    statusBar()->showMessage(
        tr("Exported: %1").arg(QFileInfo(dst).fileName()), 4000);
}

void MainWindow::importProfile()
{
    const QString src = QFileDialog::getOpenFileName(
        this, tr("Import profile"), QString(),
        tr("TSCRT profile (*.profile);;All files (*)"));
    if (src.isEmpty()) return;

    const auto choice = QMessageBox::warning(this, tr("Import profile"),
        tr("This will replace your current profile with:\n%1\n\n"
           "Open session tabs will keep running on the old profile; "
           "new tabs use the imported one. The previous profile will "
           "be backed up as tscrt.profile.bak. Continue?").arg(src),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    const QString dst    = QString::fromLocal8Bit(m_profile.profile_path);
    const QString backup = dst + QStringLiteral(".bak");

    // Backup existing.
    QFile::remove(backup);
    if (QFile::exists(dst) && !QFile::copy(dst, backup)) {
        QMessageBox::warning(this, tr("Import"),
            tr("Could not create backup at:\n%1").arg(backup));
        return;
    }

    // Replace.
    QFile::remove(dst);
    if (!QFile::copy(src, dst)) {
        QMessageBox::warning(this, tr("Import"),
            tr("Could not copy new profile to:\n%1").arg(dst));
        return;
    }

    // Reload into memory.
    loadProfile();
    if (m_snapshotMgr) m_snapshotMgr->setProfile(m_profile);
    rebuildSessionsMenu();
    rebuildSnapshotsMenu();
    rebuildSessionTree();

    statusBar()->showMessage(
        tr("Profile imported from: %1 (backup at %2)")
            .arg(QFileInfo(src).fileName(),
                 QFileInfo(backup).fileName()),
        6000);
}

void MainWindow::exportSessions()
{
    const QString stamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString suggested =
        QStringLiteral("tscrt-sessions-%1.profile").arg(stamp);
    const QString dst = QFileDialog::getSaveFileName(
        this, tr("Export sessions"), suggested,
        tr("TSCRT profile (*.profile);;All files (*)"));
    if (dst.isEmpty()) return;

    // Build a subset profile that only contains the sessions.
    profile_t subset{};
    memcpy(subset.base_dir,     m_profile.base_dir,     sizeof(subset.base_dir));
    memcpy(subset.profile_path, m_profile.profile_path, sizeof(subset.profile_path));
    subset.common      = m_profile.common;
    subset.session_count = m_profile.session_count;
    memcpy(subset.sessions, m_profile.sessions,
           sizeof(session_entry_t) * m_profile.session_count);

    if (profile_save_to(&subset, dst.toLocal8Bit().constData()) != 0) {
        QMessageBox::warning(this, tr("Export"),
            tr("Could not write subset profile to:\n%1").arg(dst));
        return;
    }

    QMessageBox::information(this, tr("Export"),
        tr("Exported %1 session(s) to:\n%2\n\n"
           "SSH passwords stored as DPAPI ciphertext will not decrypt "
           "on another Windows account — re-enter them after importing.")
            .arg(m_profile.session_count).arg(dst));
    statusBar()->showMessage(
        tr("Exported: %1").arg(QFileInfo(dst).fileName()), 4000);
}

void MainWindow::importSessions()
{
    const QString src = QFileDialog::getOpenFileName(
        this, tr("Import sessions"), QString(),
        tr("TSCRT profile (*.profile);;All files (*)"));
    if (src.isEmpty()) return;

    profile_t imported{};
    if (profile_load_from(&imported, src.toLocal8Bit().constData()) != 0) {
        QMessageBox::warning(this, tr("Import"),
            tr("Could not parse profile file:\n%1").arg(src));
        return;
    }

    int added = 0, skipped = 0;
    for (int i = 0; i < imported.session_count; ++i) {
        if (m_profile.session_count >= MAX_SESSIONS) break;

        bool dup = false;
        for (int j = 0; j < m_profile.session_count; ++j) {
            if (strcmp(m_profile.sessions[j].name,
                       imported.sessions[i].name) == 0) {
                dup = true; break;
            }
        }
        if (dup) { skipped++; continue; }

        m_profile.sessions[m_profile.session_count++] = imported.sessions[i];
        added++;
    }

    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Import"),
            tr("Could not write merged profile."));
        return;
    }
    if (m_snapshotMgr) m_snapshotMgr->setProfile(m_profile);
    rebuildSessionsMenu();
    rebuildSessionTree();

    QMessageBox::information(this, tr("Import"),
        tr("Imported %1 new session(s); skipped %2 duplicate name(s).")
            .arg(added).arg(skipped));
}

void MainWindow::exportSnapshots()
{
    const QString stamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString suggested =
        QStringLiteral("tscrt-snapshots-%1.profile").arg(stamp);
    const QString dst = QFileDialog::getSaveFileName(
        this, tr("Export snapshots"), suggested,
        tr("TSCRT profile (*.profile);;All files (*)"));
    if (dst.isEmpty()) return;

    profile_t subset{};
    memcpy(subset.base_dir,     m_profile.base_dir,     sizeof(subset.base_dir));
    memcpy(subset.profile_path, m_profile.profile_path, sizeof(subset.profile_path));
    subset.common = m_profile.common;

    subset.snapshot_count      = m_profile.snapshot_count;
    memcpy(subset.snapshots, m_profile.snapshots,
           sizeof(snapshot_entry_t) * m_profile.snapshot_count);
    subset.snapshot_rule_count = m_profile.snapshot_rule_count;
    memcpy(subset.snapshot_rules, m_profile.snapshot_rules,
           sizeof(snapshot_rule_t) * m_profile.snapshot_rule_count);
    subset.smtp = m_profile.smtp;

    if (profile_save_to(&subset, dst.toLocal8Bit().constData()) != 0) {
        QMessageBox::warning(this, tr("Export"),
            tr("Could not write subset profile to:\n%1").arg(dst));
        return;
    }

    QMessageBox::information(this, tr("Export"),
        tr("Exported %1 snapshot(s) and %2 rule(s) to:\n%3\n\n"
           "The SMTP password is DPAPI-encrypted and will not decrypt "
           "on another Windows account — re-enter it after importing.")
            .arg(m_profile.snapshot_count)
            .arg(m_profile.snapshot_rule_count)
            .arg(dst));
    statusBar()->showMessage(
        tr("Exported: %1").arg(QFileInfo(dst).fileName()), 4000);
}

void MainWindow::importSnapshots()
{
    const QString src = QFileDialog::getOpenFileName(
        this, tr("Import snapshots"), QString(),
        tr("TSCRT profile (*.profile);;All files (*)"));
    if (src.isEmpty()) return;

    profile_t imported{};
    if (profile_load_from(&imported, src.toLocal8Bit().constData()) != 0) {
        QMessageBox::warning(this, tr("Import"),
            tr("Could not parse profile file:\n%1").arg(src));
        return;
    }

    int addedSnaps = 0, skippedSnaps = 0;
    for (int i = 0; i < imported.snapshot_count; ++i) {
        if (m_profile.snapshot_count >= MAX_SNAPSHOTS) break;

        bool dup = false;
        for (int j = 0; j < m_profile.snapshot_count; ++j) {
            if (strcmp(m_profile.snapshots[j].name,
                       imported.snapshots[i].name) == 0) {
                dup = true; break;
            }
        }
        if (dup) { skippedSnaps++; continue; }

        m_profile.snapshots[m_profile.snapshot_count++] = imported.snapshots[i];
        addedSnaps++;
    }

    int addedRules = 0;
    for (int i = 0; i < imported.snapshot_rule_count; ++i) {
        if (m_profile.snapshot_rule_count >= MAX_SNAPSHOT_RULES) break;
        m_profile.snapshot_rules[m_profile.snapshot_rule_count++] =
            imported.snapshot_rules[i];
        addedRules++;
    }

    // SMTP: only overwrite when the current profile has no host set,
    // so importing a snapshot bundle doesn't silently clobber the
    // user's existing mail credentials.
    bool smtpImported = false;
    if (m_profile.smtp.host[0] == '\0' && imported.smtp.host[0] != '\0') {
        m_profile.smtp = imported.smtp;
        smtpImported = true;
    }

    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Import"),
            tr("Could not write merged profile."));
        return;
    }
    if (m_snapshotMgr) m_snapshotMgr->setProfile(m_profile);
    rebuildSnapshotsMenu();

    QMessageBox::information(this, tr("Import"),
        tr("Imported %1 snapshot(s) (%2 duplicates skipped) and %3 rule(s).%4")
            .arg(addedSnaps).arg(skippedSnaps).arg(addedRules)
            .arg(smtpImported ? tr("\nSMTP settings were also imported.")
                              : QString()));
}

bool MainWindow::appendSessionToProfile(session_entry_t entry)
{
    if (m_profile.session_count >= MAX_SESSIONS) {
        QMessageBox::warning(this, tr("Save session"),
            tr("Cannot save: profile already holds %1 sessions (max).")
                .arg(MAX_SESSIONS));
        return false;
    }

    // Encrypt SSH password (DPAPI) before storing.
    if (entry.type == SESSION_TYPE_SSH && entry.ssh.password[0]) {
        const QString enc = tscrt::encryptSecret(
            QString::fromLocal8Bit(entry.ssh.password));
        const QByteArray b = enc.toLocal8Bit();
        const int n = qMin<int>(int(sizeof(entry.ssh.password)) - 1, b.size());
        memcpy(entry.ssh.password, b.constData(), n);
        entry.ssh.password[n] = '\0';
    }

    m_profile.sessions[m_profile.session_count++] = entry;
    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("Could not write profile to:\n%1")
                .arg(QString::fromLocal8Bit(m_profile.profile_path)));
        // Roll back the in-memory append on save failure.
        --m_profile.session_count;
        return false;
    }
    rebuildSessionsMenu();
    rebuildSnapshotsMenu();
    rebuildSessionTree();
    return true;
}

void MainWindow::newSession()
{
    SessionEditDialog dlg(this);
    session_entry_t blank{};
    blank.type        = SESSION_TYPE_SSH;
    blank.log_enabled = 1;
    snprintf(blank.name, sizeof(blank.name), "%s", "New Session");
    blank.ssh.port = 22;
    dlg.setSession(blank);
    if (dlg.exec() != QDialog::Accepted) return;

    session_entry_t s = dlg.session();
    if (s.name[0] == '\0')
        snprintf(s.name, sizeof(s.name), "%s", "Untitled");

    // Persist unconditionally — even if the host/device is missing or
    // the connection later fails, the entry should remain in the profile
    // so the user can edit it from Session Manager.
    if (!appendSessionToProfile(s))
        return;

    statusBar()->showMessage(
        tr("Session \"%1\" saved.").arg(QString::fromLocal8Bit(s.name)),
        2500);

    // Try to open the session immediately. A connection failure will
    // show its own error dialog but does not affect the saved entry.
    openSessionByIndex(m_profile.session_count - 1);
}

ISession *MainWindow::makeSessionFor(const profile_t &p, const session_entry_t &entry)
{
    if (entry.type == SESSION_TYPE_SSH) {
        auto *ssh = new SshSession(entry.ssh, QString::fromLocal8Bit(entry.name));
        if (entry.ssh_keepalive_sec > 0)
            ssh->setKeepalive(entry.ssh_keepalive_sec);
        ssh->setTcpKeepalive(entry.ssh_tcp_keepalive != 0);
        // Per-session terminal type wins; fall back to global preference.
        const char *termType = entry.terminal_type[0]
            ? entry.terminal_type : p.common.terminal_type;
        ssh->setTerminalType(QByteArray(termType));
        return ssh;
    }
    return new SerialSession(entry.serial, QString::fromLocal8Bit(entry.name));
}

void MainWindow::openSessionByIndex(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= m_profile.session_count)
        return;

    const session_entry_t &s = m_profile.sessions[profileIndex];

    ISession *session = nullptr;
    if (s.type == SESSION_TYPE_SSH) {
        ssh_config_t sshCfg = s.ssh;

        // If no password and no keyfile stored, prompt for password.
        if (!sshCfg.password[0] && !sshCfg.keyfile[0]) {
            bool ok = false;
            const QString pw = QInputDialog::getText(this,
                tr("SSH Password"),
                tr("Password for %1@%2:")
                    .arg(QString::fromLocal8Bit(sshCfg.username),
                         QString::fromLocal8Bit(sshCfg.host)),
                QLineEdit::Password, QString(), &ok);
            if (!ok) return;
            if (!pw.isEmpty()) {
                const QByteArray b = pw.toUtf8();
                const int n = qMin<int>(int(sizeof(sshCfg.password)) - 1, b.size());
                memcpy(sshCfg.password, b.constData(), n);
                sshCfg.password[n] = '\0';
            }
        }

        session_entry_t resolved = s;
        resolved.ssh = sshCfg;
        session = makeSessionFor(m_profile, resolved);
    } else {
        session = makeSessionFor(m_profile, s);
    }

    auto *tab = new tscrt::SessionTab(session, m_profile, s, m_tabs);
    if (s.type == SESSION_TYPE_SSH) {
        tab->setProperty("tscrtProto", QStringLiteral("SSH"));
        tab->setProperty("tscrtHost",
            QStringLiteral("%1@%2:%3")
                .arg(QString::fromLocal8Bit(s.ssh.username),
                     QString::fromLocal8Bit(s.ssh.host))
                .arg(s.ssh.port));
    } else {
        tab->setProperty("tscrtProto", QStringLiteral("Serial"));
        tab->setProperty("tscrtHost",
            QStringLiteral("%1 %2")
                .arg(QString::fromLocal8Bit(s.serial.device))
                .arg(s.serial.baudrate));
    }

    const QString sessionName = QString::fromLocal8Bit(s.name);
    auto attachSessionHooks = [this, sessionName](ISession *sess) {
        connect(sess, &ISession::connecting, this, [this, sessionName] {
            statusBar()->showMessage(tr("Connecting to %1...").arg(sessionName));
        });
        connect(sess, &ISession::connected, this, [this, sessionName] {
            statusBar()->showMessage(tr("Connected: %1").arg(sessionName), 3000);
        });
        connect(sess, &ISession::errorOccurred, this, [this](const QString &msg) {
            QMessageBox::warning(this, tr("Session error"), msg);
            statusBar()->showMessage(tr("Error: %1").arg(msg), 5000);
        });
        connect(sess, &ISession::disconnected, this,
                [this, sessionName](const QString &reason) {
            statusBar()->showMessage(
                tr("Disconnected: %1 (%2)").arg(sessionName, reason), 5000);
        });
        if (m_snapshotMgr)
            m_snapshotMgr->attachSession(sess, sessionName);
    };
    attachSessionHooks(session);

    connect(tab, &tscrt::SessionTab::buttonEditRequested,
            this, &MainWindow::editButtonSlot);
    connect(tab, &tscrt::SessionTab::sessionAboutToChange, this,
            [this](ISession *oldSession) {
        if (m_snapshotMgr && oldSession)
            m_snapshotMgr->detachSession(oldSession);
    });
    connect(tab, &tscrt::SessionTab::sessionRebound, this,
            [attachSessionHooks](ISession *newSession) {
        attachSessionHooks(newSession);
    });
    connect(tab, &tscrt::SessionTab::paneAdded, this,
            [attachSessionHooks](tscrt::Pane *pane) {
        if (pane && pane->session())
            attachSessionHooks(pane->session());
    });
    connect(tab, &tscrt::SessionTab::paneRemoving, this,
            [this](tscrt::Pane *pane) {
        if (m_snapshotMgr && pane && pane->session())
            m_snapshotMgr->detachSession(pane->session());
    });
    connect(tab, &tscrt::SessionTab::tabEmpty, this,
            [this, tab] {
        const int i = m_tabs->indexOf(tab);
        if (i >= 0) onTabCloseRequested(i);
    });

    tab->setProperty("tscrtProfileIndex", profileIndex);
    const int idx = m_tabs->addTab(tab, QString::fromLocal8Bit(s.name));
    m_tabs->setCurrentIndex(idx);

    // Tiny close button placed right next to the title. Smaller and less
    // intrusive than Qt's default QTabBar::CloseButton which reserves a
    // chunky square hit area.
    {
        auto *closeBtn = new QToolButton(m_tabs->tabBar());
        closeBtn->setText(QStringLiteral("\u00D7"));    // ×
        closeBtn->setAutoRaise(true);
        closeBtn->setCursor(Qt::ArrowCursor);
        closeBtn->setFocusPolicy(Qt::NoFocus);
        closeBtn->setFixedSize(14, 14);
        closeBtn->setToolTip(tr("Close tab"));
        closeBtn->setStyleSheet(QStringLiteral(
            "QToolButton {"
            "  border: none; background: transparent;"
            "  color: #888; padding: 0; margin: 0;"
            "  font-size: 11pt; font-weight: bold;"
            "}"
            "QToolButton:hover {"
            "  background: rgba(0, 0, 0, 0.15);"
            "  color: #111; border-radius: 2px;"
            "}"));
        QPointer<QWidget> pageRef(tab);
        connect(closeBtn, &QToolButton::clicked, this, [this, pageRef] {
            if (!pageRef) return;
            const int i = m_tabs->indexOf(pageRef.data());
            if (i >= 0) onTabCloseRequested(i);
        });
        m_tabs->tabBar()->setTabButton(idx, QTabBar::RightSide, closeBtn);
    }

    // SessionTab's constructor sets focus on the terminal, but at that
    // point the widget is not yet inside the QTabWidget so the call has
    // no effect. Re-focus after the tab is actually visible — otherwise
    // the very first tab opened in a fresh window swallows keystrokes.
    if (auto *t = tab->terminal())
        t->setFocus(Qt::OtherFocusReason);

    // Tell session to start once the terminal has computed its grid.
    session->resize(tab->terminal()->cols(), tab->terminal()->rows());
    session->start();
}

void MainWindow::closeCurrentTab()
{
    onTabCloseRequested(m_tabs->currentIndex());
}

void MainWindow::onTabCloseRequested(int index)
{
    if (index < 0)
        return;
    QWidget *page = m_tabs->widget(index);
    auto *tab = qobject_cast<tscrt::SessionTab *>(page);
    if (tab && m_snapshotMgr) {
        // Detach every pane's session before the tab goes away, not
        // just the active one — SnapshotManager would otherwise hold
        // dangling pointers to the other panes.
        for (tscrt::Pane *p : tab->panes()) {
            if (p && p->session())
                m_snapshotMgr->detachSession(p->session());
        }
    }
    m_tabs->removeTab(index);
    if (page)
        page->deleteLater();
    updateCentralView();
}

void MainWindow::onTabBarDoubleClicked(int index)
{
    if (index < 0)
        return;
    renameTab(index);
}

void MainWindow::onTabBarContextMenu(const QPoint &pos)
{
    const int index = m_tabs->tabBar()->tabAt(pos);
    if (index < 0)
        return;

    QMenu menu(this);
    QAction *actRename    = menu.addAction(tr("Rename"));
    QAction *actDuplicate = menu.addAction(tr("Duplicate"));
    menu.addSeparator();

    // Run Snapshot ▸ submenu, filled from the profile's snapshot list.
    QMenu *snapMenu = menu.addMenu(tr("Run Snapshot"));
    QList<QAction *> snapActs;
    if (m_profile.snapshot_count == 0) {
        QAction *empty = snapMenu->addAction(tr("(no snapshots defined)"));
        empty->setEnabled(false);
    } else {
        for (int i = 0; i < m_profile.snapshot_count; ++i) {
            QAction *a = snapMenu->addAction(
                QString::fromLocal8Bit(m_profile.snapshots[i].name));
            a->setData(i);
            snapActs.append(a);
        }
    }
    menu.addSeparator();

    QWidget *page = m_tabs->widget(index);
    const bool pinned = page && page->property("tscrtPinned").toBool();
    QAction *actPin = menu.addAction(pinned ? tr("Unpin") : tr("Pin"));
    menu.addSeparator();
    QAction *actDetach = menu.addAction(tr("Detach to New Window"));
    actDetach->setEnabled(m_tabs->count() > 1);
    menu.addSeparator();
    QAction *actClose = menu.addAction(tr("Close"));

    QAction *chosen = menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actRename) {
        renameTab(index);
    } else if (chosen == actDuplicate) {
        duplicateTab(index);
    } else if (chosen == actPin) {
        if (page) {
            page->setProperty("tscrtPinned", !pinned);
            updatePinIcon(index);
        }
    } else if (chosen == actDetach) {
        detachToNewWindow(index);
    } else if (chosen == actClose) {
        onTabCloseRequested(index);
    } else if (snapActs.contains(chosen)) {
        const int snapIdx = chosen->data().toInt();
        auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(index));
        if (tab && m_snapshotMgr && snapIdx >= 0
            && snapIdx < m_profile.snapshot_count) {
            m_snapshotMgr->runSnapshotByName(
                tab->session(), tab->displayName(),
                QString::fromLocal8Bit(m_profile.snapshots[snapIdx].name));
        }
    }
}

void MainWindow::renameTab(int index)
{
    const QString cur = m_tabs->tabText(index);
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Tab"), tr("Tab name:"),
        QLineEdit::Normal, cur, &ok);
    if (ok && !name.isEmpty())
        m_tabs->setTabText(index, name);
}

void MainWindow::duplicateTab(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return;
    const QVariant v = page->property("tscrtProfileIndex");
    if (!v.isValid())
        return;
    bool ok = false;
    const int profileIndex = v.toInt(&ok);
    if (ok && profileIndex >= 0)
        openSessionByIndex(profileIndex);
}

void MainWindow::restorePinnedTabs()
{
    QSettings s;
    const QList<QVariant> pinned = s.value(QStringLiteral("pinnedTabs")).toList();
    for (const QVariant &v : pinned) {
        // Support new format (QVariantMap) and legacy format (int).
        int idx = -1;
        QString displayName;
        if (v.typeId() == QMetaType::QVariantMap) {
            const QVariantMap m = v.toMap();
            idx = m.value(QStringLiteral("index"), -1).toInt();
            displayName = m.value(QStringLiteral("name")).toString();
        } else {
            bool ok = false;
            idx = v.toInt(&ok);
            if (!ok) continue;
        }
        if (idx < 0 || idx >= m_profile.session_count)
            continue;
        openSessionByIndex(idx);
        const int tabIdx = m_tabs->count() - 1;
        QWidget *page = m_tabs->widget(tabIdx);
        if (page) {
            page->setProperty("tscrtPinned", true);
            updatePinIcon(tabIdx);
            if (!displayName.isEmpty())
                m_tabs->setTabText(tabIdx, displayName);
        }
    }
}

void MainWindow::updatePinIcon(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return;
    const bool pinned = page->property("tscrtPinned").toBool();
    const QString base = page->property("tscrtProfileIndex").isValid()
        ? QString::fromLocal8Bit(m_profile.sessions[page->property("tscrtProfileIndex").toInt()].name)
        : m_tabs->tabText(index);
    if (pinned)
        m_tabs->tabBar()->setTabIcon(index, style()->standardIcon(QStyle::SP_DialogApplyButton));
    else
        m_tabs->tabBar()->setTabIcon(index, QIcon());
}

void MainWindow::showSettingsDialog()
{
    SettingsDialog dlg(m_profile, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_profile = dlg.profile();
        if (profile_save(&m_profile) != 0) {
            QMessageBox::warning(this, tr("Save failed"),
                tr("Could not write profile to:\n%1")
                    .arg(QString::fromLocal8Bit(m_profile.profile_path)));
            return;
        }
        if (m_snapshotMgr) m_snapshotMgr->setProfile(m_profile);
        rebuildSessionsMenu();
        rebuildSessionTree();
        statusBar()->showMessage(tr("Preferences saved."), 2500);
    }
}

void MainWindow::showSessionManagerDialog()
{
    SessionManagerDialog dlg(m_profile, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_profile = dlg.profile();
        if (profile_save(&m_profile) != 0) {
            QMessageBox::warning(this, tr("Save failed"),
                tr("Could not write profile to:\n%1")
                    .arg(QString::fromLocal8Bit(m_profile.profile_path)));
            return;
        }
        tscrt::pruneOrphanHistoryFiles(m_profile);
        rebuildSessionsMenu();
        rebuildSessionTree();
        statusBar()->showMessage(tr("Sessions saved."), 2500);
    }
}

void MainWindow::toggleButtonBars(bool visible)
{
    QSettings().setValue(QStringLiteral("ui/showButtonBar"), visible);
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(i));
        if (tab && tab->buttonBar())
            tab->buttonBar()->setVisible(visible);
    }
}

void MainWindow::editButtonSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= MAX_BUTTONS) return;
    button_t &b = m_profile.buttons[slotIndex];

    bool ok = false;
    const QString label = QInputDialog::getText(this, tr("Edit button"),
        tr("Label:"), QLineEdit::Normal,
        QString::fromLocal8Bit(b.label), &ok);
    if (!ok) return;
    const QString action = QInputDialog::getText(this, tr("Edit button"),
        tr("Action (escape: \\r \\n \\t \\b \\e \\p \\\\):"),
        QLineEdit::Normal,
        QString::fromLocal8Bit(b.action), &ok);
    if (!ok) return;

    {
        const QByteArray lb = label.toLocal8Bit();
        const int n = qMin<int>(int(sizeof(b.label)) - 1, lb.size());
        memcpy(b.label, lb.constData(), n);
        b.label[n] = '\0';
    }
    {
        const QByteArray ab = action.toLocal8Bit();
        const int n = qMin<int>(int(sizeof(b.action)) - 1, ab.size());
        memcpy(b.action, ab.constData(), n);
        b.action[n] = '\0';
    }

    if (profile_save(&m_profile) != 0) {
        QMessageBox::warning(this, tr("Save failed"),
            tr("Could not write profile."));
        return;
    }

    // Push the new button layout to every open tab.
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(i));
        if (tab && tab->buttonBar())
            tab->buttonBar()->setButtons(m_profile.buttons);
    }
}

void MainWindow::toggleCmdLines(bool visible)
{
    QSettings().setValue(QStringLiteral("ui/showCmdLine"), visible);
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(i));
        if (tab && tab->commandLine())
            tab->commandLine()->setVisible(visible);
    }
}

void MainWindow::toggleStatusBar(bool visible)
{
    QSettings().setValue(QStringLiteral("ui/showStatusBar"), visible);
    statusBar()->setVisible(visible);
}

void MainWindow::toggleFullScreen()
{
    m_fullScreen = !m_fullScreen;
    if (m_actFullScreen)
        m_actFullScreen->setChecked(m_fullScreen);

    if (m_fullScreen) {
        // Hide everything except tabs.
        menuBar()->setVisible(false);
        if (m_sessionDock) m_sessionDock->setVisible(false);
        statusBar()->setVisible(false);
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(i));
            if (!tab) continue;
            if (tab->commandLine())
                tab->commandLine()->setVisible(tab->showCmdLineInFullscreen());
            if (tab->buttonBar())
                tab->buttonBar()->setVisible(tab->showButtonsInFullscreen());
        }
        showFullScreen();
    } else {
        // Restore from saved preferences.
        showNormal();
        menuBar()->setVisible(true);
        QSettings prefs;
        if (m_sessionDock)
            m_sessionDock->setVisible(
                prefs.value(QStringLiteral("ui/showSessionDock"), true).toBool());
        statusBar()->setVisible(
            m_actViewStatus ? m_actViewStatus->isChecked() : true);
        const bool showCmd = m_actViewCmdLine ? m_actViewCmdLine->isChecked() : true;
        const bool showBtn = m_actViewButtons ? m_actViewButtons->isChecked() : true;
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(i));
            if (!tab) continue;
            if (tab->commandLine()) tab->commandLine()->setVisible(showCmd);
            if (tab->buttonBar())   tab->buttonBar()->setVisible(showBtn);
        }
    }
}

void MainWindow::setUiLanguage(const QString &langCode)
{
    QSettings prefs;
    prefs.setValue(QStringLiteral("ui/language"), langCode);
    QMessageBox::information(this, QStringLiteral("Language"),
        QStringLiteral("Language preference saved. Restart TSCRT for the change to take effect."));
}

void MainWindow::showAboutDialog()
{
    const QString buildStamp = QStringLiteral(__DATE__ " " __TIME__);

    QMessageBox box(this);
    box.setWindowTitle(tr("About TSCRT"));
    box.setIconPixmap(QIcon(QStringLiteral(":/icons/app.png"))
                          .pixmap(96, 96));
    box.setTextFormat(Qt::RichText);
    box.setText(tr(
        "<h2>TSCRT</h2>"
        "<p><b>Version %1</b><br/>"
        "Built %2</p>"
        "<p>A terminal emulator for SSH2 and serial "
        "consoles.</p>"
        "<p style=\"color:#888;\">"
        "Qt %3 &middot; libssh2 %4 &middot; libvterm %5.%6"
        "</p>")
        .arg(QString::fromLatin1(TSCRT_VERSION),
             buildStamp,
             QString::fromLatin1(qVersion()),
             QString::fromLatin1(LIBSSH2_VERSION))
        .arg(VTERM_VERSION_MAJOR)
        .arg(VTERM_VERSION_MINOR));
    box.setInformativeText(tr(
        "Copyright &copy; 2026 TePSEG Co., Ltd. (Republic of Korea)<br/>"
        "Developer: <a href=\"mailto:ygjeon@tepseg.com\">ygjeon@tepseg.com</a><br/>"
        "Released under the GNU General Public License (GPL)."));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MainWindow::exportDiagnostics()
{
    // Disclosure dialog: be explicit about what is and isn't included, so
    // users can decide to share the bundle with support.
    QMessageBox consent(this);
    consent.setWindowTitle(tr("Export diagnostics"));
    consent.setIcon(QMessageBox::Information);
    consent.setTextFormat(Qt::RichText);
    consent.setText(tr(
        "<b>The diagnostic bundle will include:</b>"
        "<ul>"
        "<li>App log (<code>tscrt_win.log</code>): connection events, error messages</li>"
        "<li>System info: OS, Qt / libssh2 versions, locale</li>"
        "<li>Paths and selected environment variables (may contain your Windows username)</li>"
        "<li>Crash dump file names (contents are NOT included)</li>"
        "</ul>"
        "<b>The bundle will NOT include:</b>"
        "<ul>"
        "<li>Passwords or SSH private keys</li>"
        "<li>Your session profile (hosts, snapshots, automation rules)</li>"
        "<li>Snapshot files or per-session terminal capture logs</li>"
        "<li>Commands you typed or terminal output</li>"
        "</ul>"
        "<p>Review the generated <code>.txt.gz</code> before sharing.</p>"));
    consent.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    consent.button(QMessageBox::Ok)->setText(tr("Continue"));
    consent.setDefaultButton(QMessageBox::Ok);
    if (consent.exec() != QMessageBox::Ok)
        return;

    const QString defaultName = QStringLiteral("tscrt-diag-%1.txt.gz")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    const QString defaultDir = QStandardPaths::writableLocation(
        QStandardPaths::DesktopLocation);
    const QString suggested = QDir(defaultDir).filePath(defaultName);

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Export diagnostics"),
        suggested,
        tr("Diagnostic bundle (*.txt.gz);;All files (*)"));
    if (path.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    tscrt::DiagnosticExporter::Options opts;
    opts.daysBack = 7;
    opts.maxBytes = 10 * 1024 * 1024;
    const auto r = tscrt::DiagnosticExporter::exportBundle(path, opts);
    QApplication::restoreOverrideCursor();

    if (!r.ok) {
        QMessageBox::warning(this, tr("Export failed"),
                             tr("Could not write diagnostic bundle:\n%1").arg(r.error));
        return;
    }

    const auto human = [](qint64 b) {
        if (b < 1024) return QStringLiteral("%1 B").arg(b);
        double kb = b / 1024.0;
        if (kb < 1024.0) return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
        return QStringLiteral("%1 MB").arg(kb / 1024.0, 0, 'f', 2);
    };

    QString detail = tr("File: %1\nUncompressed: %2\nCompressed: %3")
                         .arg(r.outputPath,
                              human(r.originalBytes),
                              human(r.compressedBytes));
    if (r.truncated)
        detail += QLatin1Char('\n') + tr("Note: log content was truncated to fit the size cap.");
    if (!r.warnings.isEmpty())
        detail += QLatin1String("\n\n") + r.warnings.join(QLatin1Char('\n'));

    QMessageBox::information(this, tr("Diagnostics exported"), detail);
}

// ---- Multi-window: detach / adopt / drag -----------------------------------

QList<MainWindow *> &MainWindow::allWindows()
{
    static QList<MainWindow *> list;
    return list;
}

tscrt::SessionTab *MainWindow::detachTab(int index)
{
    if (index < 0 || index >= m_tabs->count())
        return nullptr;

    auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(index));
    if (!tab)
        return nullptr;

    // Detach sessions from this window's SnapshotManager.
    if (m_snapshotMgr) {
        for (auto *p : tab->panes()) {
            if (p && p->session())
                m_snapshotMgr->detachSession(p->session());
        }
    }

    // Disconnect all signals from this MainWindow to the tab.
    disconnect(tab, nullptr, this, nullptr);

    // Remove from tab widget without deleting.
    m_tabs->removeTab(index);
    tab->setParent(nullptr);
    updateCentralView();
    return tab;
}

void MainWindow::adoptTab(tscrt::SessionTab *tab, const QString &name)
{
    if (!tab)
        return;

    // Update every Pane's MainWindow pointer so reconnect, statusBar
    // calls, etc. target the new window.
    for (auto *p : tab->panes())
        p->setMainWindow(this);

    // Re-wire signals identical to openSessionByIndex().
    const QString sessionName = tab->displayName();
    auto attachSessionHooks = [this, sessionName](ISession *s) {
        if (!s || !m_snapshotMgr) return;
        m_snapshotMgr->attachSession(s, sessionName);
    };
    for (auto *p : tab->panes()) {
        if (p && p->session())
            attachSessionHooks(p->session());
    }
    connect(tab, &tscrt::SessionTab::buttonEditRequested,
            this, &MainWindow::editButtonSlot);
    connect(tab, &tscrt::SessionTab::sessionAboutToChange, this,
            [this](ISession *old) {
        if (m_snapshotMgr && old)
            m_snapshotMgr->detachSession(old);
    });
    connect(tab, &tscrt::SessionTab::sessionRebound, this,
            [attachSessionHooks](ISession *s) {
        attachSessionHooks(s);
    });
    connect(tab, &tscrt::SessionTab::paneAdded, this,
            [attachSessionHooks](tscrt::Pane *pane) {
        if (pane && pane->session())
            attachSessionHooks(pane->session());
    });
    connect(tab, &tscrt::SessionTab::paneRemoving, this,
            [this](tscrt::Pane *pane) {
        if (m_snapshotMgr && pane && pane->session())
            m_snapshotMgr->detachSession(pane->session());
    });
    connect(tab, &tscrt::SessionTab::tabEmpty, this,
            [this, tab] {
        const int i = m_tabs->indexOf(tab);
        if (i >= 0) onTabCloseRequested(i);
    });

    const int idx = m_tabs->addTab(tab, name);
    m_tabs->setCurrentIndex(idx);

    // Close button for the new tab.
    {
        auto *closeBtn = new QToolButton(m_tabs->tabBar());
        closeBtn->setText(QStringLiteral("\u00D7"));
        closeBtn->setAutoRaise(true);
        closeBtn->setCursor(Qt::ArrowCursor);
        closeBtn->setFocusPolicy(Qt::NoFocus);
        closeBtn->setFixedSize(14, 14);
        closeBtn->setToolTip(tr("Close tab"));
        closeBtn->setStyleSheet(QStringLiteral(
            "QToolButton {"
            "  border: none; background: transparent;"
            "  color: #888; padding: 0; margin: 0;"
            "  font-size: 11pt; font-weight: bold;"
            "}"
            "QToolButton:hover {"
            "  background: rgba(0, 0, 0, 0.15);"
            "  color: #111; border-radius: 2px;"
            "}"));
        QPointer<QWidget> pageRef(tab);
        connect(closeBtn, &QToolButton::clicked, this, [this, pageRef] {
            if (!pageRef) return;
            const int i = m_tabs->indexOf(pageRef.data());
            if (i >= 0) onTabCloseRequested(i);
        });
        m_tabs->tabBar()->setTabButton(idx, QTabBar::RightSide, closeBtn);
    }

    updateCentralView();
    if (auto *t = tab->terminal())
        t->setFocus(Qt::OtherFocusReason);
}

void MainWindow::detachToNewWindow(int index)
{
    const QString name = m_tabs->tabText(index);
    auto *tab = detachTab(index);
    if (!tab)
        return;

    auto *win = new MainWindow;
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->adoptTab(tab, name);
    win->show();
}

// ---- Drag-and-drop between windows ----------------------------------------
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>

static const QString kMimeTabDrag = QStringLiteral("application/x-tscrt-tab");

// Install an event filter on the tab bar to start QDrag when the
// user drags a tab far enough from the tab bar. This allows both
// intra-window reordering (handled natively by QTabBar::setMovable)
// and inter-window transfer (handled by our drop event).
bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_tabs->tabBar()) {
        static QPoint dragStart;
        static int    dragTabIndex = -1;

        if (ev->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::LeftButton) {
                dragStart    = me->pos();
                dragTabIndex = m_tabs->tabBar()->tabAt(me->pos());
            }
        } else if (ev->type() == QEvent::MouseMove && dragTabIndex >= 0) {
            auto *me = static_cast<QMouseEvent *>(ev);
            if ((me->pos() - dragStart).manhattanLength() >= 40) {
                // Start cross-window drag.
                auto *mimeData = new QMimeData;
                mimeData->setData(kMimeTabDrag,
                    QByteArray::number(quintptr(this)) + '|' +
                    QByteArray::number(dragTabIndex));

                auto *drag = new QDrag(this);
                drag->setMimeData(mimeData);

                // Small pixmap preview (tab text rendered as label).
                QPixmap px(200, 28);
                px.fill(Qt::transparent);
                QPainter painter(&px);
                painter.setPen(Qt::white);
                painter.setBrush(QColor(50, 50, 50, 200));
                painter.drawRoundedRect(px.rect().adjusted(0,0,-1,-1), 4, 4);
                painter.drawText(px.rect(), Qt::AlignCenter,
                                 m_tabs->tabText(dragTabIndex));
                drag->setPixmap(px);

                dragTabIndex = -1;
                Qt::DropAction result = drag->exec(Qt::MoveAction);
                if (result == Qt::IgnoreAction) {
                    // Dropped outside any TSCRT window → new window.
                    const int idx = m_tabs->tabBar()->tabAt(dragStart);
                    if (idx >= 0 && m_tabs->count() > 1)
                        detachToNewWindow(idx);
                }
                return true;   // consume the event
            }
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            dragTabIndex = -1;
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *ev)
{
    if (ev->mimeData()->hasFormat(kMimeTabDrag))
        ev->acceptProposedAction();
    else
        QMainWindow::dragEnterEvent(ev);
}

void MainWindow::dropEvent(QDropEvent *ev)
{
    if (!ev->mimeData()->hasFormat(kMimeTabDrag)) {
        QMainWindow::dropEvent(ev);
        return;
    }
    const QByteArray payload = ev->mimeData()->data(kMimeTabDrag);
    const QList<QByteArray> parts = payload.split('|');
    if (parts.size() != 2) return;

    auto *srcWin = reinterpret_cast<MainWindow *>(parts[0].toULongLong());
    const int srcIndex = parts[1].toInt();

    // Verify the source window is still alive.
    if (!allWindows().contains(srcWin))
        return;
    // Don't drop onto self (reorder is handled by QTabBar::setMovable).
    if (srcWin == this)
        return;

    const QString name = srcWin->tabWidget()->tabText(srcIndex);
    auto *tab = srcWin->detachTab(srcIndex);
    if (tab) {
        adoptTab(tab, name);
        ev->acceptProposedAction();
    }
}
