#include "SnapshotsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
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

} // namespace

SnapshotsDialog::SnapshotsDialog(const profile_t &initial, QWidget *parent)
    : QDialog(parent), m_p(initial)
{
    setWindowTitle(tr("Snapshots"));
    setModal(true);
    resize(820, 560);

    auto *root = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildSnapshotsTab(), tr("Snapshots"));
    m_tabs->addTab(buildRulesTab(),     tr("Automation rules"));
    root->addWidget(m_tabs);

    auto *bb = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);
}

void SnapshotsDialog::showRulesTab()
{
    if (m_tabs) m_tabs->setCurrentIndex(1);
}

// ---- Snapshots tab -------------------------------------------------------

QWidget *SnapshotsDialog::buildSnapshotsTab()
{
    auto *page = new QWidget(this);
    auto *root = new QHBoxLayout(page);

    auto *left = new QVBoxLayout;
    m_snapList = new QListWidget(page);
    left->addWidget(m_snapList, 1);
    auto *leftBtns = new QHBoxLayout;
    auto *addBtn = new QPushButton(tr("Add"), page);
    auto *dupBtn = new QPushButton(tr("Duplicate"), page);
    auto *delBtn = new QPushButton(tr("Delete"), page);
    leftBtns->addWidget(addBtn);
    leftBtns->addWidget(dupBtn);
    leftBtns->addWidget(delBtn);
    left->addLayout(leftBtns);
    root->addLayout(left, 1);

    auto *right = new QVBoxLayout;

    auto *form = new QFormLayout;
    m_snapName    = new QLineEdit(page);
    m_snapDesc    = new QLineEdit(page);
    m_snapSubject = new QLineEdit(page);
    m_snapSubject->setPlaceholderText(
        tr("e.g. TSCRT {session} {snapshot} {timestamp}"));
    m_snapSendEmail = new QCheckBox(tr("Send email when finished"), page);
    m_snapAttach    = new QCheckBox(tr("Attach as file (otherwise inline)"), page);

    form->addRow(tr("Name:"),        m_snapName);
    form->addRow(tr("Description:"), m_snapDesc);
    form->addRow(tr("Subject:"),     m_snapSubject);
    form->addRow(QString(),          m_snapSendEmail);
    form->addRow(QString(),          m_snapAttach);
    right->addLayout(form);

    right->addWidget(new QLabel(
        tr("Commands (sent in order; escape sequences: \\r \\n \\t \\e):"), page));
    m_snapCmds = new QTableWidget(0, 4, page);
    m_snapCmds->setHorizontalHeaderLabels({
        tr("Command"), tr("Delay (ms)"),
        tr("Prompt regex"), tr("Max wait (ms)") });
    m_snapCmds->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_snapCmds->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_snapCmds->verticalHeader()->setVisible(false);
    right->addWidget(m_snapCmds, 2);

    auto *cmdBtns = new QHBoxLayout;
    auto *addCmdBtn = new QPushButton(tr("Add command"), page);
    auto *delCmdBtn = new QPushButton(tr("Delete command"), page);
    cmdBtns->addWidget(addCmdBtn);
    cmdBtns->addWidget(delCmdBtn);
    cmdBtns->addStretch();
    right->addLayout(cmdBtns);

    right->addWidget(new QLabel(tr("Recipients (one per line):"), page));
    m_snapRecips = new QPlainTextEdit(page);
    m_snapRecips->setFixedHeight(80);
    right->addWidget(m_snapRecips);

    root->addLayout(right, 2);

    connect(m_snapList, &QListWidget::currentRowChanged,
            this, &SnapshotsDialog::onSnapshotSelected);
    connect(addBtn, &QPushButton::clicked, this, &SnapshotsDialog::addSnapshot);
    connect(dupBtn, &QPushButton::clicked, this, &SnapshotsDialog::duplicateSnapshot);
    connect(delBtn, &QPushButton::clicked, this, &SnapshotsDialog::deleteSnapshot);
    connect(addCmdBtn, &QPushButton::clicked, this, &SnapshotsDialog::addSnapshotCmd);
    connect(delCmdBtn, &QPushButton::clicked, this, &SnapshotsDialog::deleteSnapshotCmd);

    reloadSnapshotList();
    if (m_p.snapshot_count > 0) {
        m_snapList->setCurrentRow(0);
    } else {
        reloadSnapshotEditor(-1);
    }
    return page;
}

void SnapshotsDialog::reloadSnapshotList()
{
    if (!m_snapList) return;
    m_snapList->clear();
    for (int i = 0; i < m_p.snapshot_count; ++i)
        m_snapList->addItem(fromCStr(m_p.snapshots[i].name));
}

void SnapshotsDialog::reloadSnapshotEditor(int index)
{
    const bool valid = index >= 0 && index < m_p.snapshot_count;
    m_snapEditing = valid ? index : -1;

    if (!valid) {
        if (m_snapName)      m_snapName->clear();
        if (m_snapDesc)      m_snapDesc->clear();
        if (m_snapSubject)   m_snapSubject->clear();
        if (m_snapSendEmail) m_snapSendEmail->setChecked(false);
        if (m_snapAttach)    m_snapAttach->setChecked(false);
        if (m_snapCmds)      m_snapCmds->setRowCount(0);
        if (m_snapRecips)    m_snapRecips->clear();
        return;
    }

    const snapshot_entry_t &s = m_p.snapshots[index];
    m_snapName->setText(fromCStr(s.name));
    m_snapDesc->setText(fromCStr(s.description));
    m_snapSubject->setText(fromCStr(s.subject_tmpl));
    m_snapSendEmail->setChecked(s.send_email != 0);
    m_snapAttach->setChecked(s.attach_file != 0);

    m_snapCmds->setRowCount(s.cmd_count);
    for (int i = 0; i < s.cmd_count; ++i) {
        m_snapCmds->setItem(i, 0, new QTableWidgetItem(fromCStr(s.cmds[i].command)));
        m_snapCmds->setItem(i, 1, new QTableWidgetItem(QString::number(s.cmds[i].delay_ms)));
        m_snapCmds->setItem(i, 2, new QTableWidgetItem(fromCStr(s.cmds[i].expect_prompt)));
        m_snapCmds->setItem(i, 3, new QTableWidgetItem(QString::number(s.cmds[i].max_wait_ms)));
    }

    QStringList recips;
    for (int i = 0; i < s.recipient_count; ++i)
        recips << fromCStr(s.recipients[i]);
    m_snapRecips->setPlainText(recips.join(QLatin1Char('\n')));
}

void SnapshotsDialog::commitCurrentSnapshotEditor()
{
    if (m_snapEditing < 0 || m_snapEditing >= m_p.snapshot_count) return;
    snapshot_entry_t &s = m_p.snapshots[m_snapEditing];
    setStr(s.name,         sizeof(s.name),         m_snapName->text());
    setStr(s.description,  sizeof(s.description),  m_snapDesc->text());
    setStr(s.subject_tmpl, sizeof(s.subject_tmpl), m_snapSubject->text());
    s.send_email  = m_snapSendEmail->isChecked() ? 1 : 0;
    s.attach_file = m_snapAttach->isChecked() ? 1 : 0;

    const int rows = qMin<int>(m_snapCmds->rowCount(), MAX_SNAPSHOT_CMDS);
    s.cmd_count = 0;
    for (int i = 0; i < rows; ++i) {
        const auto *cmdItem = m_snapCmds->item(i, 0);
        const QString cmdText = cmdItem ? cmdItem->text() : QString();
        if (cmdText.isEmpty()) continue;
        snapshot_cmd_t &c = s.cmds[s.cmd_count++];
        memset(&c, 0, sizeof(c));
        setStr(c.command, sizeof(c.command), cmdText);
        const auto *d = m_snapCmds->item(i, 1);
        const auto *r = m_snapCmds->item(i, 2);
        const auto *w = m_snapCmds->item(i, 3);
        c.delay_ms = d ? d->text().toInt() : 0;
        setStr(c.expect_prompt, sizeof(c.expect_prompt),
               r ? r->text() : QString());
        c.max_wait_ms = w ? w->text().toInt() : 0;
    }

    const QStringList recipLines = m_snapRecips->toPlainText()
        .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    s.recipient_count = 0;
    for (const QString &line : recipLines) {
        const QString t = line.trimmed();
        if (t.isEmpty()) continue;
        if (s.recipient_count >= MAX_SNAPSHOT_RECIP) break;
        setStr(s.recipients[s.recipient_count], MAX_EMAIL_LEN, t);
        s.recipient_count++;
    }

    if (m_snapList) {
        QListWidgetItem *it = m_snapList->item(m_snapEditing);
        if (it) it->setText(fromCStr(s.name));
    }
}

void SnapshotsDialog::onSnapshotSelected(int row)
{
    commitCurrentSnapshotEditor();
    reloadSnapshotEditor(row);
}

void SnapshotsDialog::addSnapshot()
{
    if (m_p.snapshot_count >= MAX_SNAPSHOTS) {
        QMessageBox::warning(this, tr("Limit"),
            tr("Maximum snapshot count reached."));
        return;
    }
    commitCurrentSnapshotEditor();
    snapshot_entry_t &s = m_p.snapshots[m_p.snapshot_count++];
    memset(&s, 0, sizeof(s));
    snprintf(s.name, sizeof(s.name), "snapshot%d", m_p.snapshot_count);
    snprintf(s.subject_tmpl, sizeof(s.subject_tmpl),
             "TSCRT {session} {snapshot} {timestamp}");
    s.send_email = 1;
    s.attach_file = 1;
    reloadSnapshotList();
    m_snapList->setCurrentRow(m_p.snapshot_count - 1);
}

void SnapshotsDialog::duplicateSnapshot()
{
    if (m_snapEditing < 0 || m_snapEditing >= m_p.snapshot_count) return;
    if (m_p.snapshot_count >= MAX_SNAPSHOTS) {
        QMessageBox::warning(this, tr("Limit"),
            tr("Maximum snapshot count reached."));
        return;
    }
    commitCurrentSnapshotEditor();
    snapshot_entry_t &src = m_p.snapshots[m_snapEditing];
    snapshot_entry_t &dst = m_p.snapshots[m_p.snapshot_count++];
    dst = src;
    QByteArray newName = QByteArray(src.name) + "_copy";
    if (newName.size() >= (int)sizeof(dst.name))
        newName.truncate(sizeof(dst.name) - 1);
    memcpy(dst.name, newName.constData(), newName.size());
    dst.name[newName.size()] = '\0';
    reloadSnapshotList();
    m_snapList->setCurrentRow(m_p.snapshot_count - 1);
}

void SnapshotsDialog::deleteSnapshot()
{
    if (m_snapEditing < 0 || m_snapEditing >= m_p.snapshot_count) return;
    const int idx = m_snapEditing;
    m_snapEditing = -1;
    if (idx < m_p.snapshot_count - 1)
        memmove(&m_p.snapshots[idx], &m_p.snapshots[idx + 1],
                (m_p.snapshot_count - idx - 1) * sizeof(snapshot_entry_t));
    m_p.snapshot_count--;
    reloadSnapshotList();
    if (m_p.snapshot_count > 0)
        m_snapList->setCurrentRow(qMin(idx, m_p.snapshot_count - 1));
    else
        reloadSnapshotEditor(-1);
}

void SnapshotsDialog::addSnapshotCmd()
{
    if (!m_snapCmds) return;
    const int row = m_snapCmds->rowCount();
    if (row >= MAX_SNAPSHOT_CMDS) return;
    m_snapCmds->insertRow(row);
    m_snapCmds->setItem(row, 0, new QTableWidgetItem(QString()));
    m_snapCmds->setItem(row, 1, new QTableWidgetItem(QStringLiteral("500")));
    m_snapCmds->setItem(row, 2, new QTableWidgetItem(QString()));
    m_snapCmds->setItem(row, 3, new QTableWidgetItem(QStringLiteral("0")));
    m_snapCmds->editItem(m_snapCmds->item(row, 0));
}

void SnapshotsDialog::deleteSnapshotCmd()
{
    if (!m_snapCmds) return;
    const int row = m_snapCmds->currentRow();
    if (row < 0) return;
    m_snapCmds->removeRow(row);
}

// ---- Rules tab -----------------------------------------------------------

QWidget *SnapshotsDialog::buildRulesTab()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);

    root->addWidget(new QLabel(
        tr("Rules fire a registered snapshot automatically. Kinds: "
           "on_connect, cron, pattern. Leave Session blank to apply to "
           "every session."), page));

    m_rulesTable = new QTableWidget(0, 6, page);
    m_rulesTable->setHorizontalHeaderLabels({
        tr("Kind"), tr("Session"), tr("Snapshot"),
        tr("Cron expr"), tr("Pattern"), tr("Cooldown (s)") });
    m_rulesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_rulesTable->verticalHeader()->setVisible(false);
    root->addWidget(m_rulesTable);

    auto *btns = new QHBoxLayout;
    auto *addBtn = new QPushButton(tr("Add"), page);
    auto *delBtn = new QPushButton(tr("Delete"), page);
    btns->addWidget(addBtn);
    btns->addWidget(delBtn);
    btns->addStretch();
    root->addLayout(btns);

    connect(addBtn, &QPushButton::clicked, this, &SnapshotsDialog::addSnapshotRule);
    connect(delBtn, &QPushButton::clicked, this, &SnapshotsDialog::deleteSnapshotRule);

    reloadRulesTable();
    return page;
}

void SnapshotsDialog::reloadRulesTable()
{
    if (!m_rulesTable) return;
    m_rulesTable->setRowCount(m_p.snapshot_rule_count);
    for (int i = 0; i < m_p.snapshot_rule_count; ++i) {
        const snapshot_rule_t &r = m_p.snapshot_rules[i];
        const char *k = (r.kind == 1) ? "cron"
                       : (r.kind == 2) ? "pattern" : "on_connect";
        m_rulesTable->setItem(i, 0, new QTableWidgetItem(QString::fromLatin1(k)));
        m_rulesTable->setItem(i, 1, new QTableWidgetItem(fromCStr(r.session)));
        m_rulesTable->setItem(i, 2, new QTableWidgetItem(fromCStr(r.snapshot)));
        m_rulesTable->setItem(i, 3, new QTableWidgetItem(fromCStr(r.cron_expr)));
        m_rulesTable->setItem(i, 4, new QTableWidgetItem(fromCStr(r.pattern)));
        m_rulesTable->setItem(i, 5, new QTableWidgetItem(QString::number(r.cooldown_sec)));
    }
}

void SnapshotsDialog::commitSnapshotRules()
{
    if (!m_rulesTable) return;
    const int rows = qMin<int>(m_rulesTable->rowCount(), MAX_SNAPSHOT_RULES);
    m_p.snapshot_rule_count = 0;
    for (int i = 0; i < rows; ++i) {
        snapshot_rule_t &r = m_p.snapshot_rules[m_p.snapshot_rule_count];
        memset(&r, 0, sizeof(r));
        const auto *k = m_rulesTable->item(i, 0);
        const QString ks = k ? k->text() : QString();
        if (ks == QLatin1String("cron"))         r.kind = 1;
        else if (ks == QLatin1String("pattern")) r.kind = 2;
        else                                     r.kind = 0;

        auto setCell = [&](int col, char *dst, size_t cap) {
            const auto *it = m_rulesTable->item(i, col);
            setStr(dst, cap, it ? it->text() : QString());
        };
        setCell(1, r.session,   sizeof(r.session));
        setCell(2, r.snapshot,  sizeof(r.snapshot));
        setCell(3, r.cron_expr, sizeof(r.cron_expr));
        setCell(4, r.pattern,   sizeof(r.pattern));

        const auto *cd = m_rulesTable->item(i, 5);
        r.cooldown_sec = cd ? cd->text().toInt() : 0;

        if (r.snapshot[0] == '\0') continue;
        m_p.snapshot_rule_count++;
    }
}

void SnapshotsDialog::addSnapshotRule()
{
    if (!m_rulesTable) return;
    if (m_rulesTable->rowCount() >= MAX_SNAPSHOT_RULES) return;
    const int row = m_rulesTable->rowCount();
    m_rulesTable->insertRow(row);
    m_rulesTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("on_connect")));
    m_rulesTable->setItem(row, 1, new QTableWidgetItem(QString()));
    m_rulesTable->setItem(row, 2, new QTableWidgetItem(QString()));
    m_rulesTable->setItem(row, 3, new QTableWidgetItem(QString()));
    m_rulesTable->setItem(row, 4, new QTableWidgetItem(QString()));
    m_rulesTable->setItem(row, 5, new QTableWidgetItem(QStringLiteral("0")));
}

void SnapshotsDialog::deleteSnapshotRule()
{
    if (!m_rulesTable) return;
    const int row = m_rulesTable->currentRow();
    if (row < 0) return;
    m_rulesTable->removeRow(row);
}

void SnapshotsDialog::accept()
{
    commitCurrentSnapshotEditor();
    commitSnapshotRules();
    QDialog::accept();
}
