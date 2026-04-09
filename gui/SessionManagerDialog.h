/*
 * SessionManagerDialog - full-profile session table.
 *
 * Replaces the old "Sessions" tab in SettingsDialog. Provides a single
 * table over all sessions with Add / Edit / Delete / Copy / Paste.
 *
 * Operates on a copy of profile_t. Caller calls profile() after exec()
 * returns Accepted, then writes via profile_save().
 */
#pragma once

#include "tscrt.h"

#include <QDialog>

class QPushButton;
class QTreeWidget;

class SessionManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionManagerDialog(const profile_t &initial, QWidget *parent = nullptr);

    profile_t profile() const { return m_p; }

private slots:
    void addSession();
    void editSession();
    void deleteSession();
    void copySession();
    void pasteSession();
    void onSelectionChanged();

private:
    void rebuildTable();
    void selectRow(int row);
    int  currentRow() const;

    profile_t       m_p{};
    QTreeWidget    *m_table   = nullptr;
    QPushButton    *m_btnEdit = nullptr;
    QPushButton    *m_btnDel  = nullptr;
    QPushButton    *m_btnCopy = nullptr;
    QPushButton    *m_btnPaste = nullptr;

    session_entry_t m_clipboard{};
    bool            m_clipboardValid = false;
};
