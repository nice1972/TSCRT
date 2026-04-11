#include "LogSettingsDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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

} // namespace

LogSettingsDialog::LogSettingsDialog(const profile_t &initial, QWidget *parent)
    : QDialog(parent), m_p(initial)
{
    setWindowTitle(tr("Log settings"));
    setModal(true);
    resize(560, 200);

    auto *root = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("Session logs (and snapshot captures) are saved under the "
           "directory below. Each session writes its own timestamped "
           "file; snapshots go into a <b>snapshots/</b> subfolder."),
        this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *form = new QFormLayout;
    m_logDir = new QLineEdit(
        QString::fromLocal8Bit(m_p.common.log_dir), this);

    auto *row = new QHBoxLayout;
    row->addWidget(m_logDir, 1);
    auto *browse = new QPushButton(tr("Browse..."), this);
    row->addWidget(browse);
    form->addRow(tr("Log directory:"), row);
    root->addLayout(form);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(
            this, tr("Session log directory"), m_logDir->text());
        if (!d.isEmpty())
            m_logDir->setText(QDir::toNativeSeparators(d));
    });

    root->addStretch();

    auto *bb = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);
}

void LogSettingsDialog::accept()
{
    setStr(m_p.common.log_dir, sizeof(m_p.common.log_dir), m_logDir->text());
    QDialog::accept();
}
