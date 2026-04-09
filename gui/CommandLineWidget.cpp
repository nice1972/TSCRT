#include "CommandLineWidget.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QTextStream>

CommandLineWidget::CommandLineWidget(QWidget *parent)
    : QLineEdit(parent)
{
    setPlaceholderText(tr("Type a command and press Enter…"));
    setClearButtonEnabled(true);
    connect(this, &QLineEdit::returnPressed,
            this, &CommandLineWidget::onReturnPressed);
}

void CommandLineWidget::setHistoryFile(const QString &path)
{
    m_historyPath = path;
    m_history.clear();
    m_browseIdx = -1;
    loadHistory();
}

void CommandLineWidget::loadHistory()
{
    if (m_historyPath.isEmpty()) return;
    QFile f(m_historyPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.isEmpty())
            m_history.append(line);
    }
    // Trim to cap (oldest first dropped).
    if (m_history.size() > kMaxHistory)
        m_history = m_history.mid(m_history.size() - kMaxHistory);
}

void CommandLineWidget::appendToHistoryFile(const QString &cmd)
{
    if (m_historyPath.isEmpty()) return;

    QDir().mkpath(QFileInfo(m_historyPath).absolutePath());

    QFile f(m_historyPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << cmd << '\n';
}

void CommandLineWidget::onReturnPressed()
{
    const QString cmd = text();
    if (cmd.isEmpty()) return;

    // Drop consecutive duplicates of the most recent entry.
    if (m_history.isEmpty() || m_history.last() != cmd) {
        m_history.append(cmd);
        if (m_history.size() > kMaxHistory)
            m_history.removeFirst();
        appendToHistoryFile(cmd);
    }

    m_browseIdx = -1;
    emit commandEntered(cmd);
    clear();
}

void CommandLineWidget::keyPressEvent(QKeyEvent *event)
{
    if (m_history.isEmpty()) {
        QLineEdit::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Up) {
        if (m_browseIdx == -1)
            m_browseIdx = m_history.size() - 1;
        else if (m_browseIdx > 0)
            --m_browseIdx;
        setText(m_history.at(m_browseIdx));
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Down) {
        if (m_browseIdx == -1) {
            QLineEdit::keyPressEvent(event);
            return;
        }
        if (m_browseIdx < m_history.size() - 1) {
            ++m_browseIdx;
            setText(m_history.at(m_browseIdx));
        } else {
            m_browseIdx = -1;
            clear();
        }
        event->accept();
        return;
    }
    QLineEdit::keyPressEvent(event);
}
