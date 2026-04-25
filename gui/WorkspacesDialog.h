/*
 * WorkspacesDialog - browse / load / delete saved workspaces.
 *
 * The dialog is intentionally read-only with respect to disk: it only
 * returns the user's chosen action and name. The caller (MainWindow)
 * applies the loaded JSON via applyWorkspaceForSelf().
 */
#pragma once

#include <QDialog>
#include <QString>

class QTableWidget;

namespace tscrt {

class WorkspacesDialog : public QDialog {
    Q_OBJECT
public:
    enum Action { None, Load };

    explicit WorkspacesDialog(QWidget *parent = nullptr);

    Action  pickedAction() const { return m_action; }
    QString pickedName()   const { return m_name; }

private slots:
    void refresh();
    void onLoad();
    void onDelete();
    void onSetLast();

private:
    QTableWidget *m_table = nullptr;
    Action        m_action = None;
    QString       m_name;
};

} // namespace tscrt
