#include "BroadcastDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace tscrt {

BroadcastDialog::BroadcastDialog(const QVector<Target> &targets, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Broadcast command"));
    setModal(true);
    resize(560, 420);

    auto *root = new QVBoxLayout(this);

    root->addWidget(new QLabel(tr("Send to tabs:"), this));
    m_list = new QListWidget(this);
    for (const Target &t : targets) {
        auto *item = new QListWidgetItem(t.displayName, m_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, t.tabIndex);
    }
    root->addWidget(m_list, 1);

    // Select-all / none controls
    auto *btnRow = new QHBoxLayout;
    auto *all  = new QPushButton(tr("Check &all"),  this);
    auto *none = new QPushButton(tr("Check &none"), this);
    connect(all, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setCheckState(Qt::Checked);
    });
    connect(none, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setCheckState(Qt::Unchecked);
    });
    btnRow->addWidget(all);
    btnRow->addWidget(none);
    btnRow->addStretch();
    root->addLayout(btnRow);

    root->addWidget(new QLabel(
        tr("Command (escapes: \\r \\n \\t \\b \\e \\p \\\\):"), this));
    m_cmd = new QPlainTextEdit(this);
    m_cmd->setPlaceholderText(QStringLiteral("uname -a\\r"));
    m_cmd->setMaximumHeight(120);
    root->addWidget(m_cmd);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *send = bb->addButton(tr("&Send"), QDialogButtonBox::AcceptRole);
    send->setDefault(true);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);

    m_cmd->setFocus();
}

QVector<int> BroadcastDialog::selectedTabs() const
{
    QVector<int> out;
    for (int i = 0; i < m_list->count(); ++i) {
        const auto *item = m_list->item(i);
        if (item->checkState() == Qt::Checked)
            out.push_back(item->data(Qt::UserRole).toInt());
    }
    return out;
}

QString BroadcastDialog::actionString() const
{
    return m_cmd->toPlainText();
}

} // namespace tscrt
