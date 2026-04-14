/*
 * MainWindow - top-level window for tscrt_win.
 *
 * Hosts the menu bar, the session tabs and the status bar. Each tab owns
 * a TerminalWidget paired with an ISession backend.
 */
#pragma once

#include "tscrt.h"

#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QString>

class QAction;
class QDockWidget;
class QLabel;
class QMenu;
class QStackedWidget;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class TerminalWidget;
class ISession;

namespace tscrt { class SessionTab; class SnapshotManager; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void dragEnterEvent(QDragEnterEvent *ev) override;
    void dropEvent(QDropEvent *ev) override;

public slots:
    void openSessionByIndex(int profileIndex);
    void closeCurrentTab();
    void newSession();
    void showSessionManagerDialog();
    void showSettingsDialog();
    void showAboutDialog();
    void exportDiagnostics();
    void setUiLanguage(const QString &langCode);

public:
    /// Factory used by openSessionByIndex() and the Pane reconnect path.
    ISession *makeSessionFor(const profile_t &p, const session_entry_t &entry);
    const profile_t &profile() const { return m_profile; }
    QTabWidget *tabWidget() const { return m_tabs; }

    /// Detach tab at index from this window (without destroying it).
    /// The caller is responsible for reparenting the returned widget.
    tscrt::SessionTab *detachTab(int index);
    /// Adopt an already-running SessionTab from another window.
    void adoptTab(tscrt::SessionTab *tab, const QString &name);
    /// Detach tab at index and move it into a brand-new MainWindow.
    void detachToNewWindow(int index);

    /// Global list of live MainWindows (for cross-window drag).
    static QList<MainWindow *> &allWindows();

private:
    bool appendSessionToProfile(session_entry_t entry);

private slots:
    void onTabCloseRequested(int index);
    void onTabBarDoubleClicked(int index);
    void onTabBarContextMenu(const QPoint &pos);
    void onCurrentTabChanged(int index);
    void onSessionTreeActivated(QTreeWidgetItem *item, int column);
    void onSessionTreeContextMenu(const QPoint &pos);
    void editButtonSlot(int slotIndex);
    void rebuildSessionsMenu();
    void rebuildSnapshotsMenu();
    void showSnapshotsDialog();
    void showSnapshotRulesDialog();
    void openSnapshotFolder();
    void showLogSettingsDialog();
    void openLogFolder();
    void exportProfile();
    void importProfile();
    void exportSessions();
    void importSessions();
    void exportSnapshots();
    void importSnapshots();
    void rebuildSessionTree();
    void toggleButtonBars(bool visible);
    void toggleCmdLines(bool visible);
    void toggleStatusBar(bool visible);
    void toggleFullScreen();

private:
    void renameSessionByIndex(int profileIndex);
    void editSessionByIndex(int profileIndex);
    void deleteSessionByIndex(int profileIndex);
    void copySessionByIndex(int profileIndex);
    void pasteSessionFromClipboard();

private:
    void createMenus();
    void createSessionDock();
    void createStatusBar();
    void updateStatusForCurrentTab();
    void updateCentralView();
    void renameTab(int index);
    void duplicateTab(int index);
    void restorePinnedTabs();
    void updatePinIcon(int index);
    void loadProfile();
    void saveSettings();
    void loadSettings();

    profile_t        m_profile{};
    QStackedWidget  *m_central    = nullptr;
    QWidget         *m_emptyPage  = nullptr;
    QTabWidget      *m_tabs       = nullptr;
    QDockWidget   *m_sessionDock = nullptr;
    QTreeWidget   *m_sessionTree = nullptr;
    QLabel        *m_lblProto    = nullptr;
    QLabel        *m_lblHost     = nullptr;
    QLabel        *m_lblGrid     = nullptr;
    QMenu         *m_sessionsMenu  = nullptr;
    QMenu         *m_snapshotsMenu = nullptr;
    QMenu         *m_logsMenu      = nullptr;
    QMenu         *m_viewMenu      = nullptr;
    QAction       *m_actSettings = nullptr;
    QAction       *m_actQuit     = nullptr;
    QAction       *m_actAbout    = nullptr;
    QAction       *m_actCloseTab = nullptr;
    QAction       *m_actReload   = nullptr;
    QAction       *m_actViewButtons = nullptr;
    QAction       *m_actViewCmdLine = nullptr;
    QAction       *m_actViewStatus  = nullptr;
    QAction       *m_actFullScreen  = nullptr;
    bool           m_fullScreen     = false;

    // Session clipboard for the tree's right-click "Copy"/"Paste".
    session_entry_t m_sessionClipboard{};
    bool            m_sessionClipboardValid = false;

    // Snapshot coordinator (owns runners + cron ticker).
    tscrt::SnapshotManager *m_snapshotMgr = nullptr;
};
