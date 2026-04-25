#include "WorkspacesDialog.h"

#include "WorkspaceStore.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace tscrt {

WorkspacesDialog::WorkspacesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage Workspaces"));
    resize(560, 360);

    auto *root = new QVBoxLayout(this);
    auto *hint = new QLabel(
        tr("Saved workspaces capture every connected TSCRT instance's tabs "
           "and tab links. Loading a workspace replaces the current layout."),
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({ tr("Name"), tr("Saved At"), tr("Last") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    root->addWidget(m_table, /*stretch*/ 1);

    auto *row = new QHBoxLayout();
    auto *btnLoad     = new QPushButton(tr("Load"),         this);
    auto *btnDelete   = new QPushButton(tr("Delete"),       this);
    auto *btnSetLast  = new QPushButton(tr("Set as Last"),  this);
    auto *closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    row->addWidget(btnLoad);
    row->addWidget(btnDelete);
    row->addWidget(btnSetLast);
    row->addStretch(1);
    row->addWidget(closeBox);
    root->addLayout(row);

    connect(btnLoad,    &QPushButton::clicked, this, &WorkspacesDialog::onLoad);
    connect(btnDelete,  &QPushButton::clicked, this, &WorkspacesDialog::onDelete);
    connect(btnSetLast, &QPushButton::clicked, this, &WorkspacesDialog::onSetLast);
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refresh();
}

void WorkspacesDialog::refresh()
{
    const QStringList names = WorkspaceStore::list();
    const QString last = WorkspaceStore::lastName();
    m_table->setRowCount(names.size());
    for (int i = 0; i < names.size(); ++i) {
        const QString &name = names.at(i);
        const QJsonObject obj = WorkspaceStore::load(name);
        const QString iso = obj.value(QStringLiteral("saved_at")).toString();
        QString when = iso;
        const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
        if (dt.isValid())
            when = dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        m_table->setItem(i, 0, new QTableWidgetItem(name));
        m_table->setItem(i, 1, new QTableWidgetItem(when));
        m_table->setItem(i, 2, new QTableWidgetItem(name == last
            ? QStringLiteral("✔") : QString()));
    }
    if (!names.isEmpty()) m_table->selectRow(0);
}

static QString currentName(QTableWidget *t)
{
    const int r = t->currentRow();
    if (r < 0) return {};
    auto *it = t->item(r, 0);
    return it ? it->text() : QString();
}

void WorkspacesDialog::onLoad()
{
    const QString name = currentName(m_table);
    if (name.isEmpty()) {
        QMessageBox::information(this, tr("Pick a workspace"),
            tr("Select a workspace first."));
        return;
    }
    m_action = Load;
    m_name   = name;
    accept();
}

void WorkspacesDialog::onDelete()
{
    const QString name = currentName(m_table);
    if (name.isEmpty()) return;
    const auto reply = QMessageBox::question(this, tr("Delete Workspace"),
        tr("Delete workspace '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    WorkspaceStore::remove(name);
    refresh();
}

void WorkspacesDialog::onSetLast()
{
    const QString name = currentName(m_table);
    if (name.isEmpty()) return;
    WorkspaceStore::setLastName(name);
    refresh();
}

} // namespace tscrt
