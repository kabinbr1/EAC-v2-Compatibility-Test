#include "mainwindow.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QPalette systemPalette = app.palette();

    QStyle *style = nullptr;
    for (const QString &styleName : QStyleFactory::keys()) {
        if (styleName.compare(QStringLiteral("Breeze"), Qt::CaseInsensitive) == 0) {
            style = QStyleFactory::create(styleName);
            break;
        }
    }
    if (!style) {
        style = QStyleFactory::create(QStringLiteral("Fusion"));
    }
    if (style) {
        QApplication::setStyle(style);
        QApplication::setPalette(systemPalette);
    }

    QApplication::setApplicationName("EAC v2 Compatibility Test");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("EAC v2 Compatibility Test");

    MainWindow window;
    window.show();

    return app.exec();
}
