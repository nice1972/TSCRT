/*
 * BroadcastDialog - send a command to multiple open session tabs.
 *
 * Replaces the Unix tscmd named-pipe broadcaster with a native Qt
 * dialog: pick which currently-open tabs should receive the command,
 * type the command, hit Send. Action escape sequences are honored
 * (\r \n \t \b \e \p \\) so the same button-action grammar works.
 */
#pragma once

#include <QDialog>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace tscrt {

class BroadcastDialog : public QDialog {
    Q_OBJECT
public:
    struct Target {
        int     tabIndex;     // index in MainWindow's tab widget
        QString displayName;
    };

    explicit BroadcastDialog(const QVector<Target> &targets,
                             QWidget *parent = nullptr);

    /// Tab indices selected by the user.
    QVector<int> selectedTabs() const;

    /// Action string (with escape sequences) the user wants to send.
    QString actionString() const;

private:
    QListWidget    *m_list = nullptr;
    QPlainTextEdit *m_cmd  = nullptr;
};

} // namespace tscrt
