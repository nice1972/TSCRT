#include "HelpDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QSettings>
#include <QString>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("TSCRT — Usage guide"));
    resize(820, 640);

    QSettings prefs;
    QString lang = prefs.value(QStringLiteral("ui/language"),
                               QStringLiteral("en")).toString();
    if (lang != QLatin1String("en") && lang != QLatin1String("ko") &&
        lang != QLatin1String("ja") && lang != QLatin1String("zh"))
        lang = QStringLiteral("en");

    QString path = QStringLiteral(":/help/usage_%1.html").arg(lang);
    QFile f(path);
    QString html;
    if (f.open(QIODevice::ReadOnly)) {
        html = QString::fromUtf8(f.readAll());
        f.close();
    } else {
        html = tr("<p>Usage guide not available for language <b>%1</b>.</p>")
                   .arg(lang);
    }

    m_view = new QTextBrowser(this);
    m_view->setOpenExternalLinks(true);
    m_view->setStyleSheet(QStringLiteral(
        "QTextBrowser { background:#1e1e1e; color:#e0e0e0; }"));
    m_view->setHtml(html);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    layout->addWidget(buttons);
}
