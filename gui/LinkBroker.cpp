#include "LinkBroker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QUuid>

namespace tscrt {

namespace {
constexpr int kDiscoveryMs   = 3000;
constexpr int kDialTimeoutMs = 500;

QString runtimeRoot()
{
    QString rt = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (rt.isEmpty())
        rt = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return rt;
}
} // namespace

LinkBroker *LinkBroker::instance()
{
    static LinkBroker *s = nullptr;
    if (!s) s = new LinkBroker(QCoreApplication::instance());
    return s;
}

LinkBroker::LinkBroker(QObject *parent)
    : QObject(parent)
{
    m_discoverTimer.setInterval(kDiscoveryMs);
    connect(&m_discoverTimer, &QTimer::timeout, this, &LinkBroker::discover);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &LinkBroker::shutdown);
}

LinkBroker::~LinkBroker() { shutdown(); }

QString LinkBroker::peersDir() const
{
    return runtimeRoot() + QStringLiteral("/tscrt/peers");
}

QString LinkBroker::selfRegistryPath() const
{
    return peersDir() + QStringLiteral("/") + m_uuid + QStringLiteral(".peer");
}

QString LinkBroker::socketNameFor(const QString &uuid) const
{
    return QStringLiteral("tscrt-link-") + uuid;
}

bool LinkBroker::start()
{
    if (m_server) return true;

    m_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_socketName = socketNameFor(m_uuid);

    QDir().mkpath(peersDir());

    m_server = new QLocalServer(this);
    QLocalServer::removeServer(m_socketName);
    if (!m_server->listen(m_socketName)) {
        qWarning("LinkBroker: listen(%s) failed: %s",
                 qPrintable(m_socketName),
                 qPrintable(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QLocalServer::newConnection,
            this, &LinkBroker::onNewConnection);

    writeRegistry();
    discover();
    m_discoverTimer.start();
    qInfo("LinkBroker started: uuid=%s socket=%s",
          qPrintable(m_uuid), qPrintable(m_socketName));
    return true;
}

void LinkBroker::shutdown()
{
    m_discoverTimer.stop();

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        QLocalSocket *sock = it.value();
        if (sock) {
            sendJson(sock, QStringLiteral("{\"type\":\"bye\"}"));
            sock->flush();
            sock->disconnectFromServer();
            sock->deleteLater();
        }
    }
    m_peers.clear();
    m_peerTabs.clear();
    m_dialing.clear();

    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    // On Unix the bound socket file lingers after close(); remove it so
    // /tmp doesn't accumulate stale entries and future listens don't hit
    // "address already in use".
    if (!m_socketName.isEmpty())
        QLocalServer::removeServer(m_socketName);
    removeRegistry();
    m_uuid.clear();
    m_socketName.clear();
}

void LinkBroker::writeRegistry()
{
    QFile f(selfRegistryPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("LinkBroker: cannot write registry %s",
                 qPrintable(f.fileName()));
        return;
    }
    QByteArray body = "socket=" + m_socketName.toUtf8() + "\n"
                    + "pid="    + QByteArray::number(qApp->applicationPid()) + "\n";
    if (m_role)
        body += "role=" + QByteArray(1, m_role) + "\n";
    f.write(body);
}

char LinkBroker::detectAvailableRole()
{
    QString rt = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (rt.isEmpty())
        rt = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString peersPath = rt + QStringLiteral("/tscrt/peers");
    QDir dir(peersPath);
    if (!dir.exists()) return 'A';
    const auto entries = dir.entryList(
        QStringList{ QStringLiteral("*.peer") }, QDir::Files);
    for (const QString &name : entries) {
        const QString fullPath = dir.filePath(name);
        QFile f(fullPath);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QString sockName;
        bool isRoleA = false;
        while (!f.atEnd()) {
            const QByteArray line = f.readLine().trimmed();
            if (line.startsWith("socket="))
                sockName = QString::fromUtf8(line.mid(7));
            else if (line == "role=A")
                isRoleA = true;
        }
        if (!isRoleA) continue;

        // Verify the socket is actually reachable. If the previous A
        // crashed, the .peer file is stale — delete it and keep scanning
        // so we don't falsely demote ourselves to B forever.
        QLocalSocket probe;
        probe.connectToServer(sockName);
        if (probe.waitForConnected(200)) {
            probe.disconnectFromServer();
            return 'B';
        }
        qInfo("LinkBroker: stale peer file %s (role=A but socket unreachable); removing",
              qPrintable(name));
        QFile::remove(fullPath);
    }
    return 'A';
}

void LinkBroker::removeRegistry()
{
    if (!m_uuid.isEmpty())
        QFile::remove(selfRegistryPath());
}

void LinkBroker::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket *sock = m_server->nextPendingConnection();
        connect(sock, &QLocalSocket::readyRead,
                this, &LinkBroker::onPeerReadyRead);
        connect(sock, &QLocalSocket::disconnected,
                this, &LinkBroker::onPeerDisconnected);
        sock->setProperty("tscrtUuid", QString());
    }
}

void LinkBroker::discover()
{
    if (!m_server) return;
    QDir dir(peersDir());
    if (!dir.exists()) return;
    const QStringList entries = dir.entryList(
        QStringList{ QStringLiteral("*.peer") }, QDir::Files);
    for (const QString &name : entries) {
        const QString uuid = QFileInfo(name).completeBaseName();
        if (uuid == m_uuid) continue;
        if (m_peers.contains(uuid)) continue;
        if (m_dialing.contains(uuid)) continue;

        QFile f(peersDir() + QStringLiteral("/") + name);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QString sockName;
        while (!f.atEnd()) {
            const QByteArray line = f.readLine().trimmed();
            if (line.startsWith("socket=")) {
                sockName = QString::fromUtf8(line.mid(7));
                break;
            }
        }
        if (sockName.isEmpty()) continue;

        auto *sock = new QLocalSocket(this);
        m_dialing.insert(uuid);
        sock->setProperty("tscrtDialUuid", uuid);
        connect(sock, &QLocalSocket::connected, this, [this, sock, uuid] {
            m_dialing.remove(uuid);
            attachPeer(sock, uuid);
            sendHelloAndTabs(sock);
        });
        connect(sock, &QLocalSocket::errorOccurred,
                this, [this, sock, uuid](QLocalSocket::LocalSocketError) {
            m_dialing.remove(uuid);
            sock->deleteLater();
            QFile::remove(peersDir() + QStringLiteral("/") + uuid + QStringLiteral(".peer"));
        });
        sock->connectToServer(sockName);
        QTimer::singleShot(kDialTimeoutMs, sock, [sock] {
            if (sock->state() == QLocalSocket::ConnectingState)
                sock->abort();
        });
    }
}

void LinkBroker::attachPeer(QLocalSocket *sock, const QString &uuid)
{
    sock->setProperty("tscrtUuid", uuid);
    m_peers.insert(uuid, sock);
    connect(sock, &QLocalSocket::readyRead,
            this, &LinkBroker::onPeerReadyRead,
            Qt::UniqueConnection);
    connect(sock, &QLocalSocket::disconnected,
            this, &LinkBroker::onPeerDisconnected,
            Qt::UniqueConnection);
    qInfo("LinkBroker: peer attached uuid=%s", qPrintable(uuid));
    emit peerConnected(uuid);
}

void LinkBroker::onPeerReadyRead()
{
    auto *sock = qobject_cast<QLocalSocket *>(sender());
    if (!sock) return;
    while (sock->canReadLine()) {
        QByteArray line = sock->readLine();
        while (!line.isEmpty() &&
               (line.endsWith('\n') || line.endsWith('\r')))
            line.chop(1);
        if (line.isEmpty()) continue;
        handleMessage(sock, line);
    }
}

QString LinkBroker::encodeLocalTabs() const
{
    QJsonArray arr;
    for (const PeerTabInfo &t : m_localTabs) {
        QJsonObject o;
        o[QStringLiteral("session")] = t.session;
        o[QStringLiteral("slot")]    = t.slot;
        o[QStringLiteral("title")]   = t.title;
        arr.append(o);
    }
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("tabs");
    msg[QStringLiteral("list")] = arr;
    return QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void LinkBroker::sendHelloAndTabs(QLocalSocket *sock)
{
    QJsonObject hello;
    hello[QStringLiteral("type")] = QStringLiteral("hello");
    hello[QStringLiteral("uuid")] = m_uuid;
    if (m_role)
        hello[QStringLiteral("role")] = QString(QChar::fromLatin1(m_role));
    sendJson(sock,
        QString::fromUtf8(QJsonDocument(hello).toJson(QJsonDocument::Compact)));
    sendJson(sock, encodeLocalTabs());
}

void LinkBroker::handleMessage(QLocalSocket *sock, const QByteArray &line)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning("LinkBroker: malformed message: %s", line.constData());
        return;
    }
    const QJsonObject obj  = doc.object();
    const QString     type = obj.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("hello")) {
        const QString uuid = obj.value(QStringLiteral("uuid")).toString();
        if (uuid.isEmpty() || uuid == m_uuid) return;
        const QString roleStr = obj.value(QStringLiteral("role")).toString();
        if (!roleStr.isEmpty())
            m_peerRoles.insert(uuid, roleStr.at(0).toLatin1());
        const bool isNew = !m_peers.contains(uuid);
        if (isNew) {
            attachPeer(sock, uuid);
            // Only reply for the *first* hello so we don't ping-pong forever.
            sendHelloAndTabs(sock);
        }
    } else if (type == QStringLiteral("bye")) {
        sock->disconnectFromServer();
    } else if (type == QStringLiteral("tabs")) {
        const QString uuid = sock->property("tscrtUuid").toString();
        if (uuid.isEmpty()) return;
        QVector<PeerTabInfo> tabs;
        const QJsonArray arr = obj.value(QStringLiteral("list")).toArray();
        tabs.reserve(arr.size());
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            PeerTabInfo t;
            t.session = o.value(QStringLiteral("session")).toString();
            t.slot    = o.value(QStringLiteral("slot")).toInt();
            t.title   = o.value(QStringLiteral("title")).toString();
            tabs.append(t);
        }
        m_peerTabs.insert(uuid, tabs);
        emit peerTabsChanged(uuid);
    } else if (type == QStringLiteral("activate_pair")) {
        const QString pairId = obj.value(QStringLiteral("pair_id")).toString();
        qInfo("LinkBroker: received activate_pair %s", qPrintable(pairId));
        if (!pairId.isEmpty())
            emit remotePairActivated(pairId);
    } else if (type == QStringLiteral("links")) {
        const QJsonArray arr = obj.value(QStringLiteral("list")).toArray();
        qInfo("LinkBroker: received links list with %d entries", int(arr.size()));
        emit linksReceived(arr);
    }
}

void LinkBroker::broadcastLinks(const QJsonArray &links)
{
    if (m_peers.isEmpty()) return;
    QJsonObject msg;
    msg[QStringLiteral("type")] = QStringLiteral("links");
    msg[QStringLiteral("list")] = links;
    const QString payload =
        QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    for (QLocalSocket *sock : m_peers)
        sendJson(sock, payload);
}

void LinkBroker::onPeerDisconnected()
{
    auto *sock = qobject_cast<QLocalSocket *>(sender());
    if (!sock) return;
    const QString uuid = sock->property("tscrtUuid").toString();
    if (!uuid.isEmpty()) {
        m_peers.remove(uuid);
        m_peerTabs.remove(uuid);
        m_peerRoles.remove(uuid);
        emit peerDisconnected(uuid);
    }
    sock->deleteLater();
}

char LinkBroker::peerRole(const QString &uuid) const
{
    return m_peerRoles.value(uuid, 0);
}

void LinkBroker::sendJson(QLocalSocket *sock, const QString &json)
{
    if (!sock || sock->state() != QLocalSocket::ConnectedState) return;
    QByteArray out = json.toUtf8();
    out.append('\n');
    sock->write(out);
}

void LinkBroker::publishLocalTabs(const QVector<PeerTabInfo> &tabs)
{
    m_localTabs = tabs;
    const QString payload = encodeLocalTabs();
    qInfo("LinkBroker: publishing %d local tab(s) to %d peer(s)",
          int(tabs.size()), int(m_peers.size()));
    for (QLocalSocket *sock : m_peers)
        sendJson(sock, payload);
}

void LinkBroker::activatePair(const QString &pairId)
{
    if (m_suppressDepth > 0) return;
    if (pairId.isEmpty())     return;
    if (m_peers.isEmpty()) {
        qInfo("LinkBroker: activatePair %s — no peers connected, skipped",
              qPrintable(pairId));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("type")]    = QStringLiteral("activate_pair");
    msg[QStringLiteral("pair_id")] = pairId;
    const QString payload =
        QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    qInfo("LinkBroker: broadcasting activate_pair %s to %d peer(s)",
          qPrintable(pairId), int(m_peers.size()));
    for (QLocalSocket *sock : m_peers)
        sendJson(sock, payload);
}

QStringList LinkBroker::connectedPeerUuids() const
{
    return QStringList(m_peers.keyBegin(), m_peers.keyEnd());
}

QVector<PeerTabInfo> LinkBroker::peerTabs(const QString &uuid) const
{
    return m_peerTabs.value(uuid);
}

// ---- LinkBrokerSuppress ---------------------------------------------------

LinkBrokerSuppress::LinkBrokerSuppress()
{
    LinkBroker::instance()->m_suppressDepth++;
}

LinkBrokerSuppress::~LinkBrokerSuppress()
{
    auto *b = LinkBroker::instance();
    if (b->m_suppressDepth > 0)
        b->m_suppressDepth--;
}

} // namespace tscrt
