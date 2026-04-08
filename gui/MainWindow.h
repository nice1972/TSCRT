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

public slots:
    void openSessionByIndex(int profileIndex);
    void closeCurrentTab();
    void showSettingsDialog();   // placeholder until task #21
    void showAboutDialog();

private slots:
    void onTabCloseRequested(int index);
    void rebuildSessionsMenu();

private:
    void createMenus();
    void createStatusBar();
    void loadProfile();

    profile_t      m_profile{};
    QTabWidget    *m_tabs       = nullptr;
    QMenu         *m_sessionsMenu = nullptr;
    QAction       *m_actSettings = nullptr;
    QAction       *m_actQuit     = nullptr;
    QAction       *m_actAbout    = nullptr;
    QAction       *m_actCloseTab = nullptr;
    QAction       *m_actReload   = nullptr;
};
