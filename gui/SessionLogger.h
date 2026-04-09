/*
 * SessionLogger - per-session output recorder.
 *
 * Receives the raw byte stream coming back from an ISession and writes
 * it to a text log file in the configured log directory. ANSI escape
 * sequences are stripped so the saved file is plain text.
 *
 * File name pattern : <name>_<host>_YYYYMMDD_HHMMSS.log
 * Per-line prefix   : [MMDD_HHmmss_zzz] <line>
 */
#pragma once

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QString>

class SessionLogger : public QObject {
    Q_OBJECT
public:
    SessionLogger(const QString &logDir,
                  const QString &sessionName,
                  const QString &host,
                  QObject       *parent = nullptr);
    ~SessionLogger() override;

    bool isOpen() const { return m_file.isOpen(); }
    QString filePath() const { return m_file.fileName(); }

public slots:
    void onBytesReceived(const QByteArray &data);

private:
    void writeTimestampPrefix();
    void flushPendingNewline();

    QFile      m_file;
    bool       m_atLineStart = true;

    /* ANSI escape stripper state.
     *   0 = normal
     *   1 = saw ESC, expect type byte
     *   2 = inside CSI ("ESC ["), waiting for final byte 0x40-0x7e
     *   3 = inside OSC ("ESC ]") or string sequence, waiting for BEL or ST
     *   4 = saw ESC inside OSC, expecting backslash to terminate
     */
    int        m_ansi = 0;
};
