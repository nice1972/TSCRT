#include "UpdateChecker.h"

#include "tscrt.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

namespace tscrt {

namespace {
const char *kApiUrl =
    "https://api.github.com/repos/nice1972/TSCRT/releases/latest";
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
}

void UpdateChecker::start()
{
    if (m_inFlight) return;

    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
        connect(m_nam, &QNetworkAccessManager::finished,
                this, &UpdateChecker::onReply);
    }

    QNetworkRequest req(QUrl(QString::fromLatin1(kApiUrl)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TSCRT/%1").arg(QString::fromLatin1(TSCRT_VERSION)));
    req.setRawHeader("Accept", "application/vnd.github+json");

    m_inFlight = true;
    m_nam->get(req);
}

void UpdateChecker::onReply(QNetworkReply *reply)
{
    m_inFlight = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(reply->errorString());
        return;
    }

    const QByteArray body = reply->readAll();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit checkFailed(QStringLiteral("Invalid response from GitHub API"));
        return;
    }

    // tag_name is typically "v1.0.8"
    QString tag = doc.object().value(QStringLiteral("tag_name")).toString();
    if (tag.startsWith(QLatin1Char('v')))
        tag = tag.mid(1);

    const QVersionNumber remote = QVersionNumber::fromString(tag);
    const QVersionNumber local  =
        QVersionNumber::fromString(QString::fromLatin1(TSCRT_VERSION));

    if (remote.isNull() || local.isNull()) {
        emit checkFailed(QStringLiteral("Could not parse version"));
        return;
    }

    if (remote > local) {
        const QString url =
            doc.object().value(QStringLiteral("html_url")).toString();
        emit updateAvailable(tag, url);
    } else {
        emit upToDate();
    }
}

} // namespace tscrt
