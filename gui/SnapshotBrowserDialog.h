/*
 * SnapshotBrowserDialog - in-app browser for the snapshots/ folder.
 *
 * Unlike "Open snapshot folder" (which launches the OS file manager),
 * this dialog lists the files inside TSCRT so we can attach a custom
 * right-click menu with actions like "Send to administrator", which
 * mails the selected file to the SMTP From address via SmtpClient.
 */
#pragma once

#include "tscrt.h"

#include <QDialog>
#include <QString>

class QListWidget;
class QListWidgetItem;
class QLabel;

namespace tscrt { class SmtpClient; }

class SnapshotBrowserDialog : public QDialog {
    Q_OBJECT
public:
    SnapshotBrowserDialog(const profile_t &profile, QWidget *parent = nullptr);

private slots:
    void onContextMenu(const QPoint &pos);
    void onItemActivated(QListWidgetItem *item);

private:
    void reload();
    QString selectedPath() const;
    void openSelected();
    void revealSelected();
    void sendSelectedToAdmin();
    void deleteSelected();

    profile_t     m_profile{};
    QString       m_dir;
    QListWidget  *m_list  = nullptr;
    QLabel       *m_label = nullptr;
};
