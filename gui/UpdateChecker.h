/*
 * UpdateChecker - check GitHub Releases for a newer TSCRT version.
 *
 * On start(), fires a single GET to the GitHub API, parses the latest
 * release tag, and compares it to the compiled-in TSCRT_VERSION. If a
 * newer version exists, emits updateAvailable() so MainWindow can show
 * a non-blocking notification. Network errors are silently ignored.
 */
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace tscrt {

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);

    /// Kick off a single async check. Safe to call multiple times; only
    /// the first in-flight request is kept (duplicates silently dropped).
    void start();

signals:
    /// Emitted when a newer version is detected.
    /// `version` is the bare version string (e.g. "1.0.10").
    /// `releaseUrl` points to the GitHub Releases page for that tag.
    void updateAvailable(const QString &version, const QString &releaseUrl);

    /// Emitted when the check completed and the current version is the latest.
    void upToDate();

    /// Emitted when the check failed (network error, parse error, etc.).
    void checkFailed(const QString &reason);

private slots:
    void onReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam = nullptr;
    bool m_inFlight = false;
};

} // namespace tscrt
