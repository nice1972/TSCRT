#include "LinkDialog.h"

#include "LinkBroker.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace tscrt {

LinkDialog::LinkDialog(const QString &currentSession, int currentSlot,
                       QWidget *parent)
    : QDialog(parent),
      m_localSession(currentSession),
      m_localSlot(currentSlot)
{
    setWindowTitle(tr("Link Tab to Peer Tab"));
    resize(520, 340);

    auto *root = new QVBoxLayout(this);
    auto *hint = new QLabel(
        tr("Select the exact tab in another TSCRT instance to pair with\n"
           "<b>%1 (slot %2)</b>. Switching to either tab will automatically\n"
           "activate the other.").arg(currentSession.toHtmlEscaped())
            .arg(currentSlot),
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({ tr("Peer / Tab"), tr("Slot") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    root->addWidget(m_tree, /*stretch*/ 1);

    auto *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &LinkDialog::onAccept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);

    connect(LinkBroker::instance(), &LinkBroker::peerTabsChanged,
            this, &LinkDialog::refresh);
    connect(LinkBroker::instance(), &LinkBroker::peerConnected,
            this, &LinkDialog::refresh);
    connect(LinkBroker::instance(), &LinkBroker::peerDisconnected,
            this, &LinkDialog::refresh);
    refresh();
}

void LinkDialog::refresh()
{
    m_tree->clear();
    auto *broker = LinkBroker::instance();
    const QStringList peers = broker->connectedPeerUuids();
    if (peers.isEmpty()) {
        auto *empty = new QTreeWidgetItem(m_tree,
            QStringList{ tr("(no other TSCRT instance detected)"), QString() });
        empty->setFlags(Qt::ItemIsEnabled);
        return;
    }
    for (const QString &uuid : peers) {
        const QString shortId = uuid.left(8);
        auto *peerItem = new QTreeWidgetItem(m_tree,
            QStringList{ tr("TSCRT %1").arg(shortId), QString() });
        peerItem->setData(0, Qt::UserRole, QStringLiteral("peer"));
        peerItem->setData(1, Qt::UserRole, uuid);
        // Peer rows are containers — keep them visible but not selectable
        // (we only accept a concrete tab selection).
        peerItem->setFlags(Qt::ItemIsEnabled);
        const auto tabs = broker->peerTabs(uuid);
        if (tabs.isEmpty()) {
            auto *empty = new QTreeWidgetItem(peerItem,
                QStringList{ tr("(no tabs)"), QString() });
            empty->setFlags(Qt::ItemIsEnabled);
        } else {
            for (const PeerTabInfo &t : tabs) {
                QString label = t.title.isEmpty() ? t.session : t.title;
                if (t.title != t.session && !t.title.isEmpty())
                    label = t.title + QStringLiteral(" — ") + t.session;
                auto *tabItem = new QTreeWidgetItem(peerItem,
                    QStringList{ label, QString::number(t.slot) });
                tabItem->setData(0, Qt::UserRole, QStringLiteral("tab"));
                tabItem->setData(1, Qt::UserRole, uuid);
                tabItem->setData(0, Qt::UserRole + 1, t.session);
                tabItem->setData(1, Qt::UserRole + 1, t.slot);
            }
        }
        // Expand only after children are added — Qt ignores setExpanded
        // on an item that has no children yet.
        peerItem->setExpanded(true);
    }
}

void LinkDialog::onAccept()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || item->data(0, Qt::UserRole).toString() != QLatin1String("tab")) {
        QMessageBox::information(this, tr("Pick a tab"),
            tr("Please select a specific peer tab to pair with."));
        return;
    }
    m_peerUuid = item->data(1, Qt::UserRole).toString();
    m_session  = item->data(0, Qt::UserRole + 1).toString();
    m_slot     = item->data(1, Qt::UserRole + 1).toInt();
    m_peerRole = LinkBroker::instance()->peerRole(m_peerUuid);
    accept();
}

} // namespace tscrt
