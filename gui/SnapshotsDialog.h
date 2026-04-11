/*
 * SnapshotsDialog - top-level dialog for managing snapshots and the
 * automation rules that fire them. Split out from SettingsDialog so the
 * day-to-day snapshot workflow lives in its own menu rather than buried
 * inside Preferences.
 *
 * Two tabs:
 *   Snapshots : name / commands / recipients / subject / email flags
 *   Rules     : on_connect / cron / pattern triggers
 *
 * Operates on a copy of profile_t; caller reads profile() after exec().
 */
#pragma once

#include "tscrt.h"

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTableWidget;

class SnapshotsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SnapshotsDialog(const profile_t &initial,
                             QWidget *parent = nullptr);

    profile_t profile() const { return m_p; }

    /// Open directly on the Rules tab (used by "Automation rules..." menu).
    void showRulesTab();

private slots:
    void onSnapshotSelected(int row);
    void addSnapshot();
    void duplicateSnapshot();
    void deleteSnapshot();
    void addSnapshotCmd();
    void deleteSnapshotCmd();

    void addSnapshotRule();
    void deleteSnapshotRule();

private:
    QWidget *buildSnapshotsTab();
    QWidget *buildRulesTab();

    void commitCurrentSnapshotEditor();
    void commitSnapshotRules();
    void accept() override;

    void reloadSnapshotList();
    void reloadSnapshotEditor(int index);
    void reloadRulesTable();

    profile_t       m_p{};

    // Snapshots
    class QTabWidget *m_tabs      = nullptr;
    QListWidget     *m_snapList   = nullptr;
    int              m_snapEditing = -1;
    QLineEdit       *m_snapName   = nullptr;
    QLineEdit       *m_snapDesc   = nullptr;
    QLineEdit       *m_snapSubject = nullptr;
    QCheckBox       *m_snapSendEmail = nullptr;
    QCheckBox       *m_snapAttach = nullptr;
    QTableWidget    *m_snapCmds   = nullptr;
    QPlainTextEdit  *m_snapRecips = nullptr;

    // Rules
    QTableWidget    *m_rulesTable = nullptr;
};
