/*
 * LinkBroker - cross-instance tab-link IPC over QLocalServer/Socket.
 *
 * Each TSCRT process generates a UUID and listens on a QLocalServer named
 * "tscrt-link-<uuid>". A registry file under <runtime>/tscrt/peers/
 * advertises the socket name so other instances can discover and dial.
 *
 * Runtime model:
 *   - Each tab in each MainWindow has a session name + slot_index (slot
 *     disambiguates duplicate tabs of the same session within a window).
 *   - The broker mirrors each peer's current tab list so the link dialog
 *     can present specific peer tabs (including duplicates).
 *   - Links are identified by an opaque pair_id. Activating a tab bound
 *     to a pair sends activatePair(pair_id) to all peers, who look up the
 *     pair_id locally and switch the matching tab.
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

class QLocalServer;
class QLocalSocket;

namespace tscrt {

struct PeerTabInfo {
    QString session;
    int     slot = 0;
    QString title;
    /// Per-process MainWindow id (>=1). Lets the link dialog distinguish
    /// same-role tabs that live in different windows of the same process,
    /// so "Detach to New Window" splits can still be paired via Tab Link.
    /// Defaults to 0 for cross-process peer tabs (window grouping irrelevant).
    int     window_id = 0;
};

struct PeerWindowInfo {
    /// Per-process MainWindow id (matches PeerTabInfo::window_id).
    int   window_id = 0;
    /// Outer frame geometry in screen coordinates (suitable for setGeometry
    /// after subtracting frameMargins, or for restoreGeometry-equivalent).
    int   x = 0, y = 0, w = 0, h = 0;
    bool  maximized = false;
};

class LinkBroker : public QObject {
    Q_OBJECT
public:
    static LinkBroker *instance();

    bool start();
    void shutdown();

    /// Advertise this process's role (A / B / 0=none) in the peer registry
    /// so siblings can pick the opposite role on launch. Must be called
    /// before start().
    void setSelfRole(char role) { m_role = role; }
    char selfRole() const { return m_role; }

    /// Scan the peers directory for a running instance claiming role=A.
    /// Used by new processes to decide whether they should become B.
    /// Returns 'A' if no peer claims A, 'B' otherwise.
    static char detectAvailableRole();

    QString selfUuid() const { return m_uuid; }
    QStringList connectedPeerUuids() const;
    QVector<PeerTabInfo> peerTabs(const QString &uuid) const;
    /// Last list published via publishLocalTabs(). Used by the link
    /// dialog so same-process tabs across multiple MainWindows can be
    /// listed with the same monotone slot indices that peers see.
    const QVector<PeerTabInfo> &localTabs() const { return m_localTabs; }
    char peerRole(const QString &uuid) const;

    /// Publish our local tab list to every connected peer.
    /// Windows call this whenever tabs are added/removed/renamed.
    void publishLocalTabs(const QVector<PeerTabInfo> &tabs);

    /// Publish our local MainWindow geometry list. MainWindow calls this
    /// from move/resize events (debounced) and after layout changes so
    /// peers — and the workspace snapshot builder — always have a
    /// recent picture of where every window sits on screen.
    void publishLocalWindows(const QVector<PeerWindowInfo> &windows);

    /// Cached window geometries.
    const QVector<PeerWindowInfo> &localWindows() const { return m_localWindows; }
    QVector<PeerWindowInfo> peerWindows(const QString &uuid) const;

    /// Ask peers to activate the tab bound to this pair_id.
    /// Swallowed if currently applying an inbound activation (echo guard).
    void activatePair(const QString &pairId);

    /// Broadcast the current link table to every peer. MainWindow calls
    /// this whenever the profile's tab_links change, and also when a
    /// new peer first connects (so the peer sees our authoritative list).
    void broadcastLinks(const class QJsonArray &links);

signals:
    /// A peer asked us to activate whatever local tab is bound to pair_id.
    /// MainWindow looks up its pair_id→tab binding and switches.
    void remotePairActivated(const QString &pairId);

    /// Peer connected and sent its first tab list — dialogs can refresh.
    void peerConnected(const QString &uuid);
    /// Peer disconnected.
    void peerDisconnected(const QString &uuid);
    /// Peer's tab list changed.
    void peerTabsChanged(const QString &uuid);
    /// Peer sent us a fresh links list to adopt.
    void linksReceived(const class QJsonArray &links);

private slots:
    void onNewConnection();
    void onPeerReadyRead();
    void onPeerDisconnected();
    void discover();

private:
    explicit LinkBroker(QObject *parent = nullptr);
    ~LinkBroker() override;

    QString peersDir() const;
    QString selfRegistryPath() const;
    QString socketNameFor(const QString &uuid) const;
    void writeRegistry();
    void removeRegistry();

    void attachPeer(QLocalSocket *sock, const QString &uuid);
    void sendJson(QLocalSocket *sock, const QString &json);
    void handleMessage(QLocalSocket *sock, const QByteArray &line);
    void sendHelloAndTabs(QLocalSocket *sock);
    QString encodeLocalTabs() const;
    QString encodeLocalWindows() const;

    QLocalServer                            *m_server = nullptr;
    QString                                  m_uuid;
    QString                                  m_socketName;
    char                                     m_role    = 0;
    QHash<QString, QLocalSocket *>           m_peers;       // uuid → socket
    QHash<QString, QVector<PeerTabInfo>>     m_peerTabs;    // uuid → tabs
    QHash<QString, QVector<PeerWindowInfo>>  m_peerWindows; // uuid → windows
    QHash<QString, char>                     m_peerRoles;   // uuid → 'A'/'B'/0
    QVector<PeerTabInfo>                     m_localTabs;
    QVector<PeerWindowInfo>                  m_localWindows;
    QSet<QString>                          m_dialing;
    QTimer                                 m_discoverTimer;
    int                                    m_suppressDepth = 0;

    friend class LinkBrokerSuppress;
};

/// Scope guard: while alive, the broker won't rebroadcast tab activations.
/// MainWindow uses this around setCurrentIndex() when applying an inbound
/// activation from a peer.
class LinkBrokerSuppress {
public:
    LinkBrokerSuppress();
    ~LinkBrokerSuppress();
};

} // namespace tscrt
