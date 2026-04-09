#include "SessionManagerDialog.h"

#include "Credentials.h"
#include "SessionEditDialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <cstring>

namespace {

void setStr(char *dst, size_t cap, const QString &src)
{
    const QByteArray b = src.toLocal8Bit();
    const int n = qMin<int>(static_cast<int>(cap) - 1, b.size());
    memcpy(dst, b.constData(), n);
    dst[n] = '\0';
}

QString fromCStr(const char *s)
{
    return QString::fromLocal8Bit(s ? s : "");
}

void encryptInPlace(session_entry_t &s)
{
    if (s.type != SESSION_TYPE_SSH || s.ssh.password[0] == '\0') return;
    const QString enc = tscrt::encryptSecret(QString::fromLocal8Bit(s.ssh.password));
    setStr(s.ssh.password, sizeof(s.ssh.password), enc);
}

void decryptInPlace(session_entry_t &s)
{
    if (s.type != SESSION_TYPE_SSH || s.ssh.password[0] == '\0') return;
    const QString plain = tscrt::decryptSecret(QString::fromLocal8Bit(s.ssh.password));
    setStr(s.ssh.password, sizeof(s.ssh.password), plain);
}

} // namespace

SessionManagerDialog::SessionManagerDialog(const profile_t &initial, QWidget *parent)
    : QDialog(parent), m_p(initial)
{
    setWindowTitle(tr("Session Manager"));
    setModal(true);
    resize(820, 480);

    auto *root = new QVBoxLayout(this);

    m_table = new QTreeWidget(this);
    m_table->setColumnCount(7);
    m_table->setHeaderLabels(
        { tr("Name"), tr("Type"), tr("Host / Device"),
          tr("Port / Baud"), tr("User"), tr("Key file"), tr("Log") });
    m_table->setRootIsDecorated(false);
    m_table->setUniformRowHeights(true);
    m_table->setAllColumnsShowFocus(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setFrameShape(QFrame::NoFrame);
    // NoFocus kills the dotted focus rectangle that the Windows style
    // otherwise paints around the whole tree (visible even before any
    // row is selected). Clicks still select rows.
    m_table->setFocusPolicy(Qt::NoFocus);
    // Light blue selection, matching the active-tab color.
    // ":!active" keeps the highlight when the window loses focus.
    m_table->setStyleSheet(QStringLiteral(
        "QTreeView { outline: 0; border: 0; }"
        "QTreeView::item:selected            { background:#aed6f1; color:#1a1a1a; }"
        "QTreeView::item:selected:!active    { background:#aed6f1; color:#1a1a1a; }"));
    m_table->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    root->addWidget(m_table, 1);

    auto *brow = new QHBoxLayout;
    auto *btnAdd   = new QPushButton(tr("&Add..."),    this);
    m_btnEdit      = new QPushButton(tr("&Edit..."),   this);
    m_btnDel       = new QPushButton(tr("&Delete"),    this);
    m_btnCopy      = new QPushButton(tr("&Copy"),      this);
    m_btnPaste     = new QPushButton(tr("&Paste"),     this);
    brow->addWidget(btnAdd);
    brow->addWidget(m_btnEdit);
    brow->addWidget(m_btnDel);
    brow->addSpacing(12);
    brow->addWidget(m_btnCopy);
    brow->addWidget(m_btnPaste);
    brow->addStretch(1);
    root->addLayout(brow);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                    this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);

    connect(btnAdd,     &QPushButton::clicked, this, &SessionManagerDialog::addSession);
    connect(m_btnEdit,  &QPushButton::clicked, this, &SessionManagerDialog::editSession);
    connect(m_btnDel,   &QPushButton::clicked, this, &SessionManagerDialog::deleteSession);
    connect(m_btnCopy,  &QPushButton::clicked, this, &SessionManagerDialog::copySession);
    connect(m_btnPaste, &QPushButton::clicked, this, &SessionManagerDialog::pasteSession);
    connect(m_table, &QTreeWidget::itemSelectionChanged,
            this, &SessionManagerDialog::onSelectionChanged);
    connect(m_table, &QTreeWidget::itemDoubleClicked,
            this, [this] { editSession(); });

    rebuildTable();
    onSelectionChanged();
}

void SessionManagerDialog::rebuildTable()
{
    m_table->clear();
    for (int i = 0; i < m_p.session_count; ++i) {
        const session_entry_t &s = m_p.sessions[i];
        const QString logCol = s.log_enabled ? tr("on") : tr("off");
        QStringList cols;
        if (s.type == SESSION_TYPE_SSH) {
            cols << fromCStr(s.name)
                 << QStringLiteral("SSH")
                 << fromCStr(s.ssh.host)
                 << QString::number(s.ssh.port)
                 << fromCStr(s.ssh.username)
                 << fromCStr(s.ssh.keyfile)
                 << logCol;
        } else {
            cols << fromCStr(s.name)
                 << QStringLiteral("Serial")
                 << fromCStr(s.serial.device)
                 << QString::number(s.serial.baudrate)
                 << QString()
                 << QString()
                 << logCol;
        }
        new QTreeWidgetItem(m_table, cols);
    }
}

int SessionManagerDialog::currentRow() const
{
    if (!m_table) return -1;
    QTreeWidgetItem *it = m_table->currentItem();
    return it ? m_table->indexOfTopLevelItem(it) : -1;
}

void SessionManagerDialog::selectRow(int row)
{
    if (!m_table) return;
    if (row < 0 || row >= m_table->topLevelItemCount()) return;
    m_table->setCurrentItem(m_table->topLevelItem(row));
}

void SessionManagerDialog::onSelectionChanged()
{
    const bool hasRow = currentRow() >= 0;
    m_btnEdit ->setEnabled(hasRow);
    m_btnDel  ->setEnabled(hasRow);
    m_btnCopy ->setEnabled(hasRow);
    m_btnPaste->setEnabled(m_clipboardValid && m_p.session_count < MAX_SESSIONS);
}

void SessionManagerDialog::addSession()
{
    if (m_p.session_count >= MAX_SESSIONS) {
        QMessageBox::warning(this, tr("Limit reached"),
            tr("Maximum number of sessions (%1) reached.").arg(MAX_SESSIONS));
        return;
    }
    SessionEditDialog dlg(this);
    session_entry_t blank;
    memset(&blank, 0, sizeof(blank));
    blank.type        = SESSION_TYPE_SSH;
    blank.log_enabled = 1;
    blank.ssh.port    = 22;
    dlg.setSession(blank);
    if (dlg.exec() != QDialog::Accepted) return;

    session_entry_t s = dlg.session();
    if (s.name[0] == '\0') {
        QMessageBox::warning(this, tr("Invalid"), tr("Session name is required."));
        return;
    }
    encryptInPlace(s);
    m_p.sessions[m_p.session_count++] = s;
    rebuildTable();
    selectRow(m_p.session_count - 1);
    onSelectionChanged();
}

void SessionManagerDialog::editSession()
{
    const int row = currentRow();
    if (row < 0 || row >= m_p.session_count) return;

    session_entry_t edit = m_p.sessions[row];
    decryptInPlace(edit);

    SessionEditDialog dlg(this);
    dlg.setSession(edit);
    if (dlg.exec() != QDialog::Accepted) return;

    session_entry_t s = dlg.session();
    if (s.name[0] == '\0') {
        QMessageBox::warning(this, tr("Invalid"), tr("Session name is required."));
        return;
    }
    encryptInPlace(s);
    m_p.sessions[row] = s;
    rebuildTable();
    selectRow(row);
}

void SessionManagerDialog::deleteSession()
{
    const int row = currentRow();
    if (row < 0 || row >= m_p.session_count) return;
    const QString name = fromCStr(m_p.sessions[row].name);
    if (QMessageBox::question(this, tr("Delete session"),
            tr("Delete session \"%1\"?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;

    for (int i = row; i < m_p.session_count - 1; ++i)
        m_p.sessions[i] = m_p.sessions[i + 1];
    --m_p.session_count;
    memset(&m_p.sessions[m_p.session_count], 0, sizeof(m_p.sessions[0]));

    rebuildTable();
    selectRow(qMin(row, m_p.session_count - 1));
    onSelectionChanged();
}

void SessionManagerDialog::copySession()
{
    const int row = currentRow();
    if (row < 0 || row >= m_p.session_count) return;
    m_clipboard      = m_p.sessions[row];
    m_clipboardValid = true;
    onSelectionChanged();
}

void SessionManagerDialog::pasteSession()
{
    if (!m_clipboardValid) return;
    if (m_p.session_count >= MAX_SESSIONS) {
        QMessageBox::warning(this, tr("Paste"),
            tr("Profile already holds the maximum number of sessions (%1).")
                .arg(MAX_SESSIONS));
        return;
    }

    session_entry_t s = m_clipboard;

    // Build a unique name "<base> (copy)" / "(copy 2)" / ...
    const QString base = fromCStr(s.name);
    auto nameInUse = [this](const QString &candidate) {
        for (int i = 0; i < m_p.session_count; ++i) {
            if (candidate == fromCStr(m_p.sessions[i].name)) return true;
        }
        return false;
    };
    QString candidate = base + QStringLiteral(" (copy)");
    int n = 2;
    while (nameInUse(candidate))
        candidate = base + QStringLiteral(" (copy %1)").arg(n++);
    setStr(s.name, sizeof(s.name), candidate);

    m_p.sessions[m_p.session_count++] = s;
    rebuildTable();
    selectRow(m_p.session_count - 1);
    onSelectionChanged();
}
