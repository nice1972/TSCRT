/*
 * CommandLineWidget - bottom command-input box for a session tab.
 *
 * Single-line entry that:
 *   - emits commandEntered() when the user presses Enter
 *   - keeps a per-session history file
 *   - lets Up/Down navigate that history (shell-style)
 */
#pragma once

#include <QLineEdit>
#include <QString>
#include <QStringList>

class CommandLineWidget : public QLineEdit {
    Q_OBJECT
public:
    explicit CommandLineWidget(QWidget *parent = nullptr);

    /// Bind to a per-session history file. Loads existing entries.
    void setHistoryFile(const QString &path);

    QString historyFile() const { return m_historyPath; }

signals:
    void commandEntered(const QString &cmd);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onReturnPressed();

private:
    void loadHistory();
    void appendToHistoryFile(const QString &cmd);

    static constexpr int kMaxHistory = 500;

    QString     m_historyPath;
    QStringList m_history;     // oldest -> newest
    int         m_browseIdx = -1;  // -1 = not browsing
};
