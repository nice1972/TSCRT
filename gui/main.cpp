#include <QApplication>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tscrt_win"));
    app.setOrganizationName(QStringLiteral("tscrt"));

    MainWindow w;
    w.show();
    return app.exec();
}
