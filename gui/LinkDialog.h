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
    LinkDialog(const QString &currentSession, int currentSlot,
               QWidget *parent = nullptr);

    QString pickedPeerUuid()   const { return m_peerUuid; }
    QString pickedSession()    const { return m_session; }
    int     pickedSlot()       const { return m_slot; }
    char    pickedPeerRole()   const { return m_peerRole; }

private slots:
    void refresh();

private:
    QTreeWidget *m_tree = nullptr;
    QString      m_localSession;
    int          m_localSlot = 0;

    // Populated on accept()
    QString m_peerUuid;
    QString m_session;
    int     m_slot = 0;
    char    m_peerRole = 0;

private:
    void onAccept();
};

} // namespace tscrt
