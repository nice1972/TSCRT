#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>

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
