#include "MainWindow.h"

#include "ActionParser.h"
#include "BroadcastDialog.h"
#include "ButtonBar.h"
#include "Credentials.h"
#include "ISession.h"
#include "QuickConnectDialog.h"
#include "SerialSession.h"
#include "SessionEditDialog.h"
#include "SessionTab.h"
#include "SettingsDialog.h"
#include "SshSession.h"
#include "TerminalWidget.h"

#include "profile.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QKeySequence>
#include <QSettings>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtGlobal>

#include <libssh2.h>
#include <vterm.h>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("TSCRT"));
    resize(1100, 720);

    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    connect(m_tabs, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);
    connect(m_tabs, &QTabWidget::currentChanged,
            this, &MainWindow::onCurrentTabChanged);
    setCentralWidget(m_tabs);

    loadProfile();
    createMenus();
    createSessionDock();
    createStatusBar();
    rebuildSessionsMenu();
    rebuildSessionTree();
    loadSettings();

    // Apply persisted view-toggle preferences after createStatusBar.
    {
        QSettings prefs;
        const bool showStatus = prefs.value(QStringLiteral("ui/showStatusBar"), true).toBool();
        statusBar()->setVisible(showStatus);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("MainWindow"));
    s.setValue(QStringLiteral("geometry"),  saveGeometry());
    s.setValue(QStringLiteral("state"),     saveState());
    s.endGroup();
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

MainWindow::~MainWindow() = default;

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

    auto *quickAct = new QAction(tr("&Quick Connect..."), this);
    quickAct->setShortcut(QKeySequence(tr("Ctrl+N")));
    connect(quickAct, &QAction::triggered, this, &MainWindow::openQuickConnect);
    fileMenu->addAction(quickAct);

    fileMenu->addSeparator();

    m_actCloseTab = new QAction(tr("&Close tab"), this);
    m_actCloseTab->setShortcut(QKeySequence(tr("Ctrl+W")));
    connect(m_actCloseTab, &QAction::triggered, this, &MainWindow::closeCurrentTab);
    fileMenu->addAction(m_actCloseTab);

    fileMenu->addSeparator();

    m_actQuit = new QAction(tr("E&xit"), this);
    m_actQuit->setShortcut(QKeySequence::Quit);
    connect(m_actQuit, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(m_actQuit);

    m_sessionsMenu = menuBar()->addMenu(tr("&Sessions"));

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    auto *viewMenu = m_viewMenu;
    m_actViewStatus  = new QAction(tr("Show &Status Bar"), this);
    m_actViewStatus->setCheckable(true);
    {
        QSettings prefs;
        m_actViewStatus->setChecked(
            prefs.value(QStringLiteral("ui/showStatusBar"), true).toBool());
    }
    connect(m_actViewStatus, &QAction::toggled,
            this, &MainWindow::toggleStatusBar);
    viewMenu->addAction(m_actViewStatus);

    auto *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    auto *bcastAct = new QAction(tr("&Broadcast..."), this);
    bcastAct->setShortcut(QKeySequence(tr("Ctrl+B")));
    connect(bcastAct, &QAction::triggered, this, &MainWindow::openBroadcastDialog);
    toolsMenu->addAction(bcastAct);

    auto *settingsMenu = menuBar()->addMenu(tr("Se&ttings"));
    m_actSettings = new QAction(tr("&Preferences..."), this);
    m_actSettings->setShortcut(QKeySequence(tr("Ctrl+,")));
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::showSettingsDialog);
    settingsMenu->addAction(m_actSettings);

    auto *langMenu = settingsMenu->addMenu(tr("&Language"));
    auto *enAct = langMenu->addAction(QStringLiteral("English"));
    auto *koAct = langMenu->addAction(QStringLiteral("\355\225\234\352\265\255\354\226\264 (Korean)"));
    enAct->setCheckable(true);
    koAct->setCheckable(true);
    {
        QSettings prefs;
        const QString cur = prefs.value(QStringLiteral("ui/language"),
                                        QStringLiteral("en")).toString();
        enAct->setChecked(cur == QLatin1String("en"));
        koAct->setChecked(cur == QLatin1String("ko"));
    }
    connect(enAct, &QAction::triggered, this, [this] {
        setUiLanguage(QStringLiteral("en"));
    });
    connect(koAct, &QAction::triggered, this, [this] {
        setUiLanguage(QStringLiteral("ko"));
    });

    m_actReload = new QAction(tr("&Reload profile"), this);
    m_actReload->setShortcut(QKeySequence(tr("F5")));
    connect(m_actReload, &QAction::triggered, this, [this] {
        loadProfile();
        rebuildSessionsMenu();
        rebuildSessionTree();
        statusBar()->showMessage(tr("Profile reloaded."), 2500);
    });
    settingsMenu->addAction(m_actReload);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    m_actAbout = new QAction(tr("&About tscrt..."), this);
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
    connect(m_sessionTree, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onSessionTreeActivated);

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
    m_lblProto->setStyleSheet(QStringLiteral("color:#9cdcfe;"));
    m_lblHost ->setStyleSheet(QStringLiteral("color:#e0e0e0;"));
    m_lblGrid ->setStyleSheet(QStringLiteral("color:#888;"));

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

void MainWindow::rebuildSessionTree()
{
    if (!m_sessionTree) return;
    m_sessionTree->clear();

    QStyle *st = style();
    auto *sshGroup = new QTreeWidgetItem(m_sessionTree);
    sshGroup->setText(0, tr("SSH Sessions"));
    sshGroup->setIcon(0, st->standardIcon(QStyle::SP_DriveNetIcon));
    QFont gf = sshGroup->font(0); gf.setBold(true); sshGroup->setFont(0, gf);

    auto *serGroup = new QTreeWidgetItem(m_sessionTree);
    serGroup->setText(0, tr("Serial Sessions"));
    serGroup->setIcon(0, st->standardIcon(QStyle::SP_DriveHDIcon));
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
            item->setIcon(0, st->standardIcon(QStyle::SP_DriveFDIcon));
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

    auto *newAct = new QAction(tr("&New session..."), this);
    newAct->setShortcut(QKeySequence(tr("Ctrl+Shift+N")));
    connect(newAct, &QAction::triggered, this, &MainWindow::newSession);
    m_sessionsMenu->addAction(newAct);
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

void MainWindow::openBroadcastDialog()
{
    QVector<tscrt::BroadcastDialog::Target> targets;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(i));
        if (!tab || !tab->session()) continue;
        targets.push_back({ i, tab->displayName() });
    }
    if (targets.isEmpty()) {
        QMessageBox::information(this, tr("Broadcast"),
            tr("No open session tabs to broadcast to."));
        return;
    }

    tscrt::BroadcastDialog dlg(targets, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString action = dlg.actionString();
    if (action.isEmpty()) return;

    const auto chunks = tscrt::parseAction(action);
    int sentTo = 0;
    for (int idx : dlg.selectedTabs()) {
        auto *tab = qobject_cast<tscrt::SessionTab *>(m_tabs->widget(idx));
        if (!tab || !tab->session()) continue;
        for (const auto &c : chunks) {
            if (!c.bytes.isEmpty())
                QMetaObject::invokeMethod(tab->session(), "sendBytes",
                                          Qt::QueuedConnection,
                                          Q_ARG(QByteArray, c.bytes));
        }
        ++sentTo;
    }
    statusBar()->showMessage(
        tr("Broadcast sent to %1 session(s).").arg(sentTo), 3000);
}

void MainWindow::openQuickConnect()
{
    QuickConnectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    session_entry_t entry = dlg.entry();
    if (entry.type == SESSION_TYPE_SSH) {
        if (entry.ssh.host[0] == '\0' || entry.ssh.username[0] == '\0') {
            QMessageBox::warning(this, tr("Quick Connect"),
                tr("Host and username are required."));
            return;
        }
    } else {
        if (entry.serial.device[0] == '\0') {
            QMessageBox::warning(this, tr("Quick Connect"),
                tr("Device (e.g. COM3) is required."));
            return;
        }
    }

    // Persist to profile so the session is reusable from Session Manager,
    // even if the connection later fails.
    appendSessionToProfile(entry);

    openAdHoc(entry);
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
    rebuildSessionTree();
    return true;
}

void MainWindow::newSession()
{
    SessionEditDialog dlg(this);
    session_entry_t blank{};
    blank.type = SESSION_TYPE_SSH;
    snprintf(blank.name, sizeof(blank.name), "%s", "New Session");
    blank.ssh.port = 22;
    dlg.setSession(blank);
    if (dlg.exec() != QDialog::Accepted) return;

    session_entry_t s = dlg.session();
    if (s.type == SESSION_TYPE_SSH) {
        if (s.ssh.host[0] == '\0' || s.ssh.username[0] == '\0') {
            QMessageBox::warning(this, tr("New session"),
                tr("Host and username are required."));
            return;
        }
    } else {
        if (s.serial.device[0] == '\0') {
            QMessageBox::warning(this, tr("New session"),
                tr("Device (e.g. COM3) is required."));
            return;
        }
    }
    if (s.name[0] == '\0')
        snprintf(s.name, sizeof(s.name), "%s", "Untitled");

    if (appendSessionToProfile(s))
        statusBar()->showMessage(
            tr("Session \"%1\" saved.").arg(QString::fromLocal8Bit(s.name)),
            2500);
}

void MainWindow::openAdHoc(const session_entry_t &entry)
{
    const QString name = QString::fromLocal8Bit(entry.name);

    ISession *session = nullptr;
    if (entry.type == SESSION_TYPE_SSH) {
        session = new SshSession(entry.ssh, name);
    } else {
        session = new SerialSession(entry.serial, name);
    }

    auto *tab = new tscrt::SessionTab(session, m_profile, entry, m_tabs);
    if (entry.type == SESSION_TYPE_SSH) {
        tab->setProperty("tscrtProto", QStringLiteral("SSH"));
        tab->setProperty("tscrtHost",
            QStringLiteral("%1@%2:%3")
                .arg(QString::fromLocal8Bit(entry.ssh.username),
                     QString::fromLocal8Bit(entry.ssh.host))
                .arg(entry.ssh.port));
    } else {
        tab->setProperty("tscrtProto", QStringLiteral("Serial"));
        tab->setProperty("tscrtHost",
            QStringLiteral("%1 %2")
                .arg(QString::fromLocal8Bit(entry.serial.device))
                .arg(entry.serial.baudrate));
    }

    connect(session, &ISession::connecting, this, [this, name] {
        statusBar()->showMessage(tr("Connecting to %1...").arg(name));
    });
    connect(session, &ISession::connected, this, [this, name] {
        statusBar()->showMessage(tr("Connected: %1").arg(name), 3000);
    });
    connect(session, &ISession::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::warning(this, tr("Session error"), msg);
    });
    connect(session, &ISession::disconnected, this, [this, name](const QString &reason) {
        statusBar()->showMessage(tr("Disconnected: %1 (%2)").arg(name, reason), 5000);
    });

    const int idx = m_tabs->addTab(tab, name);
    m_tabs->setCurrentIndex(idx);
    session->resize(tab->terminal()->cols(), tab->terminal()->rows());
    session->start();
}

void MainWindow::openSessionByIndex(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= m_profile.session_count)
        return;

    const session_entry_t &s = m_profile.sessions[profileIndex];

    ISession *session = nullptr;
    if (s.type == SESSION_TYPE_SSH) {
        session = new SshSession(s.ssh, QString::fromLocal8Bit(s.name));
    } else {
        session = new SerialSession(s.serial, QString::fromLocal8Bit(s.name));
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

    connect(session, &ISession::connecting, this, [this, name = QString::fromLocal8Bit(s.name)] {
        statusBar()->showMessage(tr("Connecting to %1...").arg(name));
    });
    connect(session, &ISession::connected, this, [this, name = QString::fromLocal8Bit(s.name)] {
        statusBar()->showMessage(tr("Connected: %1").arg(name), 3000);
    });
    connect(session, &ISession::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::warning(this, tr("Session error"), msg);
        statusBar()->showMessage(tr("Error: %1").arg(msg), 5000);
    });
    connect(session, &ISession::disconnected, this, [this, name = QString::fromLocal8Bit(s.name)](const QString &reason) {
        statusBar()->showMessage(tr("Disconnected: %1 (%2)").arg(name, reason), 5000);
    });

    const int idx = m_tabs->addTab(tab, QString::fromLocal8Bit(s.name));
    m_tabs->setCurrentIndex(idx);

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
    m_tabs->removeTab(index);
    if (page)
        page->deleteLater();
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
        rebuildSessionsMenu();
        rebuildSessionTree();
        statusBar()->showMessage(tr("Preferences saved."), 2500);
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

void MainWindow::toggleStatusBar(bool visible)
{
    QSettings().setValue(QStringLiteral("ui/showStatusBar"), visible);
    statusBar()->setVisible(visible);
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
        "<p>A SecureCRT-style terminal emulator for SSH2 and serial "
        "consoles, ported from the Linux <code>tscrt</code> project.</p>"
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
