#include <QApplication>
#include <QFont>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("BookCreateEdit");
    app.setOrganizationName("BookCreateEdit");
    app.setStyle("Fusion");

    MainWindow window;
    window.showMaximized();

    return app.exec();
}
