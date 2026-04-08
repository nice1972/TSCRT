#include <QApplication>

#include "Logging.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tscrt_win"));
    app.setOrganizationName(QStringLiteral("tscrt"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    tscrt::installLogging();
    tscrt::logBanner();

    MainWindow w;
    w.show();

    const int rc = app.exec();
    tscrt::shutdownLogging();
    return rc;
}
