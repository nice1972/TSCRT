/*
 * MainWindow - top-level window for tscrt_win.
 *
 * Hosts the menu bar, the session tabs and the status bar. Each tab owns
 * a TerminalWidget paired with an ISession backend.
 */
#pragma once

#include "tscrt.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMainWindow>
#include <QPair>
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
    void moveEvent(QMoveEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;

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

    /// Per-process window identifier (>=1, monotonically assigned at
    /// construction). Used by Tab Link to disambiguate same-role
    /// endpoints that live in different MainWindow instances.
    int windowId() const { return m_windowId; }

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

    // Cross-instance tab-link bookkeeping. Pair id -> local tab bound to
    // that link (rebuilt every time tabs change; see refreshLinkState()).
    QHash<QString, QPointer<tscrt::SessionTab>> m_pairBindings;
    // Guard so the initial refreshLinkState() on an empty window doesn't
    // overwrite the saved layout before autoRestoreLayout() has had a
    // chance to open the previously-persisted tabs.
    bool m_layoutReadyToSave = false;

    // Per-process window identifier (>=1). See windowId().
    int  m_windowId = 0;

    // Debounce timer that publishes window geometry to LinkBroker after
    // the user stops moving / resizing the window for ~150 ms.
    class QTimer *m_geomPublishTimer = nullptr;

public:
    /// Recompute pair_id -> SessionTab bindings by matching every
    /// profile tab_link against the current tab layout, and publish this
    /// process's tab list to the LinkBroker so peers can discover it.
    /// Call after any tab add/remove/rename/move.
    void refreshLinkState();

    /// Build a JSON workspace snapshot of the entire cluster: every
    /// connected TSCRT process's role/tabs (via LinkBroker's cached peer
    /// state) plus the current tab_links table. Suitable for persisting
    /// via WorkspaceStore and later restoring with applyWorkspaceForSelf().
    QJsonObject buildWorkspaceSnapshot() const;

    /// Apply the slice of a workspace snapshot that matches *this*
    /// process's role: open the captured tabs, fan extra window_id groups
    /// out into fresh MainWindow instances, and adopt the snapshot's
    /// tab_links (with self-side window_id fields remapped onto the new
    /// windows).
    void applyWorkspaceForSelf(const QJsonObject &workspace);

private:
    void activateLocalTabForPair(const QString &pairId);
    void propagateProfileLinksToOtherWindows();
    void autoRestoreLayout();   // called once, by the first window of role
    void saveRoleLayout() const; // writes current tabs of all windows
    void broadcastCurrentLinks() const;
};
