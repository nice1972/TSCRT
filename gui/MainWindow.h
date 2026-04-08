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
class QMenu;
class QTabWidget;
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
    void openQuickConnect();
    void openBroadcastDialog();
    void closeCurrentTab();
    void showSettingsDialog();
    void showAboutDialog();
    void setUiLanguage(const QString &langCode);

private:
    void openAdHoc(const session_entry_t &entry);

private slots:
    void onTabCloseRequested(int index);
    void rebuildSessionsMenu();
    void toggleButtonBars(bool visible);
    void toggleStatusBar(bool visible);

private:
    void createMenus();
    void createStatusBar();
    void loadProfile();
    void saveSettings();
    void loadSettings();

    profile_t      m_profile{};
    QTabWidget    *m_tabs       = nullptr;
    QMenu         *m_sessionsMenu = nullptr;
    QAction       *m_actSettings = nullptr;
    QAction       *m_actQuit     = nullptr;
    QAction       *m_actAbout    = nullptr;
    QAction       *m_actCloseTab = nullptr;
    QAction       *m_actReload   = nullptr;
    QAction       *m_actViewButtons = nullptr;
    QAction       *m_actViewStatus  = nullptr;
};
