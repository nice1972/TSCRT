/*
 * MainWindow - top-level window for tscrt_win.
 *
 * Hosts the menu bar, the session tabs and the status bar. Each tab owns
 * a TerminalWidget paired with an ISession backend.
 */
#pragma once

#include "tscrt.h"

#include <QMainWindow>
#include <QPointer>
#include <QString>

class QAction;
class QDockWidget;
class QLabel;
class QMenu;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class TerminalWidget;
class ISession;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

public slots:
    void openSessionByIndex(int profileIndex);
    void closeCurrentTab();
    void newSession();
    void showSessionManagerDialog();
    void showSettingsDialog();
    void showAboutDialog();
    void setUiLanguage(const QString &langCode);

private:
    bool appendSessionToProfile(session_entry_t entry);

private slots:
    void onTabCloseRequested(int index);
    void onCurrentTabChanged(int index);
    void onSessionTreeActivated(QTreeWidgetItem *item, int column);
    void onSessionTreeContextMenu(const QPoint &pos);
    void rebuildSessionsMenu();
    void rebuildSessionTree();
    void toggleButtonBars(bool visible);
    void toggleStatusBar(bool visible);

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
    void loadProfile();
    void saveSettings();
    void loadSettings();

    profile_t      m_profile{};
    QTabWidget    *m_tabs        = nullptr;
    QDockWidget   *m_sessionDock = nullptr;
    QTreeWidget   *m_sessionTree = nullptr;
    QLabel        *m_lblProto    = nullptr;
    QLabel        *m_lblHost     = nullptr;
    QLabel        *m_lblGrid     = nullptr;
    QMenu         *m_sessionsMenu = nullptr;
    QMenu         *m_viewMenu     = nullptr;
    QAction       *m_actSettings = nullptr;
    QAction       *m_actQuit     = nullptr;
    QAction       *m_actAbout    = nullptr;
    QAction       *m_actCloseTab = nullptr;
    QAction       *m_actReload   = nullptr;
    QAction       *m_actViewButtons = nullptr;
    QAction       *m_actViewStatus  = nullptr;

    // Session clipboard for the tree's right-click "Copy"/"Paste".
    session_entry_t m_sessionClipboard{};
    bool            m_sessionClipboardValid = false;
};
