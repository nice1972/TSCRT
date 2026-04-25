/*
 * LinkDialog - pick a peer TSCRT's specific tab to pair with.
 *
 * The dialog queries LinkBroker for every connected peer instance and
 * its current tab list. Duplicate tabs (same session opened twice) are
 * shown with their slot_index so the user can pair with the exact tab.
 * Result: (peer_uuid, session_name, slot_index). The MainWindow then
 * forms a tab_link_t with a fresh pair_id and persists it.
 */
#pragma once

#include "tscrt.h"

#include <QDialog>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;

namespace tscrt {

class LinkDialog : public QDialog {
    Q_OBJECT
public:
    /// currentWindowId: the MainWindow id this dialog was launched from
    /// (passed to MainWindow::windowId()). Tabs in that window are hidden
    /// from the picker — you can't link a tab to itself or to a sibling
    /// in the same window. 0 disables the same-process window group entirely.
    LinkDialog(const QString &currentSession, int currentSlot,
               int currentWindowId, QWidget *parent = nullptr);

    QString pickedPeerUuid()   const { return m_peerUuid; }
    QString pickedSession()    const { return m_session; }
    int     pickedSlot()       const { return m_slot; }
    char    pickedPeerRole()   const { return m_peerRole; }
    /// Non-zero if the user picked a tab in another MainWindow of *this*
    /// process. 0 means the pick was a tab in a remote TSCRT process
    /// (in which case window_id is irrelevant and not persisted).
    int     pickedWindowId()   const { return m_windowId; }

private slots:
    void refresh();

private:
    QTreeWidget *m_tree = nullptr;
    QString      m_localSession;
    int          m_localSlot = 0;
    int          m_currentWindowId = 0;

    // Populated on accept()
    QString m_peerUuid;
    QString m_session;
    int     m_slot = 0;
    char    m_peerRole = 0;
    int     m_windowId = 0;

private:
    void onAccept();
};

} // namespace tscrt
