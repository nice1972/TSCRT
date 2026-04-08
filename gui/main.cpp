#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QTranslator>

#include "CrashHandler.h"
#include "Logging.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tscrt_win"));
    app.setOrganizationName(QStringLiteral("tscrt"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    // Localization. Pick the bundled translation that best matches the
    // user's system locale; fall back to the source language otherwise.
    QTranslator appTranslator;
    const QLocale sysLocale = QLocale::system();
    const QStringList uiLangs = sysLocale.uiLanguages();
    bool loaded = false;
    for (const QString &lang : uiLangs) {
        QString tag = lang;
        tag.replace(QLatin1Char('-'), QLatin1Char('_'));
        if (appTranslator.load(QStringLiteral(":/i18n/tscrt_win_%1.qm").arg(tag))
            || appTranslator.load(QStringLiteral(":/i18n/tscrt_win_%1.qm")
                                      .arg(tag.section('_', 0, 0)))) {
            QCoreApplication::installTranslator(&appTranslator);
            loaded = true;
            break;
        }
    }
    QTranslator qtTranslator;
    if (qtTranslator.load(sysLocale,
                          QStringLiteral("qt"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&qtTranslator);
    }
    Q_UNUSED(loaded);

    tscrt::installLogging();
    tscrt::installCrashHandler();
    tscrt::logBanner();

    // Surface previous crash dump if any
    const QString lastDump = tscrt::findLastCrashDump();
    if (!lastDump.isEmpty()) {
        const auto choice = QMessageBox::question(nullptr,
            QObject::tr("tscrt - Crash report"),
            QObject::tr("A crash report from a previous run was found:\n%1\n\n"
                        "Would you like to keep it for inspection?")
                .arg(QFileInfo(lastDump).fileName()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (choice == QMessageBox::No)
            tscrt::clearCrashDump(lastDump);
    }

    MainWindow w;
    w.show();

    const int rc = app.exec();
    tscrt::shutdownLogging();
    return rc;
}
