#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("EAC v2 Compatibility Test");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("EAC v2 Compatibility Test");

    MainWindow window;
    window.show();

    return app.exec();
}
