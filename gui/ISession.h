/*
 * ISession - abstract terminal session backend.
 *
 * One ISession per tab. Each backend (SSH, Serial) runs its I/O loop on
 * its own QThread and communicates with the GUI via queued signals/slots.
 */
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>

class ISession : public QObject {
    Q_OBJECT
public:
    explicit ISession(QObject *parent = nullptr) : QObject(parent) {}
    ~ISession() override = default;

    virtual QString displayName() const = 0;
    bool isRunning() const { return m_running; }

public slots:
    virtual void start()                       = 0;   // open + start I/O loop
    virtual void stop()                        = 0;   // graceful close
    virtual void sendBytes(const QByteArray &) = 0;   // user input
    virtual void resize(int cols, int rows)    = 0;   // PTY/window size

signals:
    void connecting();
    void connected();
    void disconnected(const QString &reason);
    void errorOccurred(const QString &message);
    void bytesReceived(const QByteArray &data);

protected:
    bool m_running = false;
};
