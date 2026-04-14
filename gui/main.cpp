#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QTranslator>

#ifndef _WIN32
#include <signal.h>
#endif

#include "CrashHandler.h"
#include "Logging.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
#ifndef _WIN32
    // Writing to a closed socket would otherwise kill the process on
    // macOS/Linux — libssh2 on macOS can't use MSG_NOSIGNAL, so we mask
    // SIGPIPE globally and rely on write() returning EPIPE instead.
    signal(SIGPIPE, SIG_IGN);
#endif

    QApplication app(argc, argv);
#if defined(Q_OS_MACOS)
    app.setApplicationName(QStringLiteral("tscrt_mac"));
#elif defined(Q_OS_LINUX)
    app.setApplicationName(QStringLiteral("tscrt_linux"));
#else
    app.setApplicationName(QStringLiteral("tscrt_win"));
#endif
    app.setOrganizationName(QStringLiteral("tscrt"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    // Localization policy: English by default, regardless of system
    // locale. The user can opt in to a translation via Settings ->
    // Language. Selection is persisted under HKCU\Software\tscrt.
    QTranslator appTranslator;
    QSettings   prefs;
    const QString lang = prefs.value(QStringLiteral("ui/language"),
                                     QStringLiteral("en")).toString();
    if (lang != QLatin1String("en")) {
        if (appTranslator.load(QStringLiteral(":/i18n/tscrt_win_%1.qm").arg(lang))) {
            QCoreApplication::installTranslator(&appTranslator);
        }
    }

    tscrt::installLogging();
    tscrt::installCrashHandler();
    tscrt::logBanner();

    // Surface previous crash dump if any
    const QString lastDump = tscrt::findLastCrashDump();
    if (!lastDump.isEmpty()) {
        const auto choice = QMessageBox::question(nullptr,
            QObject::tr("TSCRT - Crash report"),
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
