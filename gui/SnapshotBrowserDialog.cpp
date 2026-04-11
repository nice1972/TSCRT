#include "SnapshotBrowserDialog.h"

#include "SmtpClient.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStringList>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

SnapshotBrowserDialog::SnapshotBrowserDialog(const profile_t &profile,
                                             QWidget *parent)
    : QDialog(parent), m_profile(profile)
{
    setWindowTitle(tr("Snapshot browser"));
    resize(760, 520);

    QString base = QString::fromLocal8Bit(m_profile.common.log_dir);
    if (!base.isEmpty()) {
        QDir d(base);
        if (!d.exists(QStringLiteral("snapshots")))
            d.mkpath(QStringLiteral("snapshots"));
        d.cd(QStringLiteral("snapshots"));
        m_dir = d.absolutePath();
    }

    m_label = new QLabel(m_dir.isEmpty()
                             ? tr("Snapshot folder is not configured.")
                             : m_dir,
                         this);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &SnapshotBrowserDialog::onContextMenu);
    connect(m_list, &QListWidget::itemActivated,
            this, &SnapshotBrowserDialog::onItemActivated);

    auto *buttons = new QDialogButtonBox(this);
    auto *btnRefresh = buttons->addButton(tr("Refresh"),
                                          QDialogButtonBox::ActionRole);
    auto *btnSend = buttons->addButton(tr("Send to administrator"),
                                       QDialogButtonBox::ActionRole);
    auto *btnClose = buttons->addButton(QDialogButtonBox::Close);
    connect(btnRefresh, &QPushButton::clicked, this,
            &SnapshotBrowserDialog::reload);
    connect(btnSend, &QPushButton::clicked, this,
            &SnapshotBrowserDialog::sendSelectedToAdmin);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_label);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);

    reload();
}

void SnapshotBrowserDialog::reload()
{
    m_list->clear();
    if (m_dir.isEmpty()) return;

    QDir d(m_dir);
    d.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    d.setSorting(QDir::Time);        // most recent first

    const auto entries = d.entryInfoList();
    for (const QFileInfo &fi : entries) {
        const QString ts = fi.lastModified().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        const QString size = QString::number(fi.size());
        const QString display = QStringLiteral("%1    %2    %3 bytes")
                                    .arg(fi.fileName(), -40)
                                    .arg(ts)
                                    .arg(size);
        auto *item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        item->setToolTip(fi.absoluteFilePath());
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

QString SnapshotBrowserDialog::selectedPath() const
{
    auto *item = m_list->currentItem();
    if (!item) return QString();
    return item->data(Qt::UserRole).toString();
}

void SnapshotBrowserDialog::onItemActivated(QListWidgetItem *item)
{
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void SnapshotBrowserDialog::onContextMenu(const QPoint &pos)
{
    auto *item = m_list->itemAt(pos);
    if (!item) return;
    m_list->setCurrentItem(item);

    QMenu menu(this);
    auto *actOpen   = menu.addAction(tr("Open"));
    auto *actReveal = menu.addAction(tr("Reveal in file manager"));
    menu.addSeparator();
    auto *actSend   = menu.addAction(tr("Send to administrator"));
    menu.addSeparator();
    auto *actDelete = menu.addAction(tr("Delete"));

    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == actOpen)        openSelected();
    else if (chosen == actReveal) revealSelected();
    else if (chosen == actSend)   sendSelectedToAdmin();
    else if (chosen == actDelete) deleteSelected();
}

void SnapshotBrowserDialog::openSelected()
{
    const QString path = selectedPath();
    if (path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void SnapshotBrowserDialog::revealSelected()
{
    const QString path = selectedPath();
    if (path.isEmpty()) return;
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            { QStringLiteral("/select,"),
                              QDir::toNativeSeparators(path) });
#elif defined(Q_OS_MAC)
    QProcess::startDetached(QStringLiteral("open"),
                            { QStringLiteral("-R"), path });
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void SnapshotBrowserDialog::deleteSelected()
{
    const QString path = selectedPath();
    if (path.isEmpty()) return;
    const QString name = QFileInfo(path).fileName();
    if (QMessageBox::question(this, tr("Delete snapshot"),
            tr("Delete \"%1\"?").arg(name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes)
        return;
    if (!QFile::remove(path)) {
        QMessageBox::warning(this, tr("Delete failed"),
            tr("Could not delete %1").arg(path));
        return;
    }
    reload();
}

void SnapshotBrowserDialog::sendSelectedToAdmin()
{
    const QString path = selectedPath();
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Send to administrator"),
            tr("Select a snapshot file first."));
        return;
    }

    if (m_profile.smtp.host[0] == '\0') {
        QMessageBox::warning(this, tr("SMTP not configured"),
            tr("Configure an SMTP server under Settings → SMTP before "
               "using this action."));
        return;
    }

    const QString admin = QString::fromLocal8Bit(m_profile.smtp.from_addr);
    if (admin.isEmpty()) {
        QMessageBox::warning(this, tr("No administrator address"),
            tr("The SMTP \"From\" address is empty. Set it in "
               "Settings → SMTP; that address is used as the "
               "administrator recipient."));
        return;
    }

    const QString stamp = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString fileName = QFileInfo(path).fileName();
    const QString subject = tr("[TSCRT] Snapshot %1").arg(fileName);
    const QString body = tr(
        "The attached TSCRT snapshot file was forwarded by %1 at %2.\n"
        "\n"
        "File: %3\n")
        .arg(QString::fromLocal8Bit(qgetenv("USERNAME")).isEmpty()
                 ? QString::fromLocal8Bit(qgetenv("USER"))
                 : QString::fromLocal8Bit(qgetenv("USERNAME")),
             stamp,
             fileName);

    auto *smtp = new tscrt::SmtpClient(m_profile.smtp, this);
    setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    connect(smtp, &tscrt::SmtpClient::sent, this,
            [this, smtp, admin, fileName] {
        QApplication::restoreOverrideCursor();
        setEnabled(true);
        smtp->deleteLater();
        QMessageBox::information(this, tr("Sent"),
            tr("Snapshot \"%1\" sent to %2.").arg(fileName, admin));
    });
    connect(smtp, &tscrt::SmtpClient::failed, this,
            [this, smtp](const QString &reason) {
        QApplication::restoreOverrideCursor();
        setEnabled(true);
        smtp->deleteLater();
        QMessageBox::warning(this, tr("Send failed"), reason);
    });

    smtp->send(QStringList{ admin }, subject, body, path);
}
