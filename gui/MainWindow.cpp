#include "MainWindow.h"

#include "ISession.h"
#include "SerialSession.h"
#include "SessionTab.h"
#include "SettingsDialog.h"
#include "SshSession.h"
#include "TerminalWidget.h"

#include "profile.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QKeySequence>
#include <QSettings>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QtGlobal>

#include <libssh2.h>
#include <vterm.h>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("tscrt for Windows"));
    resize(1100, 720);

    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    connect(m_tabs, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);
    setCentralWidget(m_tabs);

    loadProfile();
    createMenus();
    createStatusBar();
    rebuildSessionsMenu();
    loadSettings();
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

    auto *settingsMenu = menuBar()->addMenu(tr("Se&ttings"));
    m_actSettings = new QAction(tr("&Preferences..."), this);
    m_actSettings->setShortcut(QKeySequence(tr("Ctrl+,")));
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::showSettingsDialog);
    settingsMenu->addAction(m_actSettings);

    m_actReload = new QAction(tr("&Reload profile"), this);
    m_actReload->setShortcut(QKeySequence(tr("F5")));
    connect(m_actReload, &QAction::triggered, this, [this] {
        loadProfile();
        rebuildSessionsMenu();
        statusBar()->showMessage(tr("Profile reloaded."), 2500);
    });
    settingsMenu->addAction(m_actReload);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    m_actAbout = new QAction(tr("&About tscrt..."), this);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
    helpMenu->addAction(m_actAbout);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(
        tr("Ready · libssh2 %1 · libvterm %2.%3")
            .arg(QString::fromLatin1(LIBSSH2_VERSION))
            .arg(VTERM_VERSION_MAJOR)
            .arg(VTERM_VERSION_MINOR));
}

void MainWindow::rebuildSessionsMenu()
{
    if (!m_sessionsMenu)
        return;
    m_sessionsMenu->clear();

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
        statusBar()->showMessage(tr("Preferences saved."), 2500);
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, tr("About tscrt for Windows"),
        tr("<h3>tscrt for Windows</h3>"
           "<p>Version %1</p>"
           "<p>SSH and serial console terminal emulator.</p>"
           "<p>Built with Qt %2, libssh2 %3, libvterm %4.%5.</p>")
            .arg(QString::fromLatin1(TSCRT_VERSION),
                 QString::fromLatin1(qVersion()),
                 QString::fromLatin1(LIBSSH2_VERSION))
            .arg(VTERM_VERSION_MAJOR)
            .arg(VTERM_VERSION_MINOR));
}
