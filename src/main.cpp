#include "app/MainWindow.h"
#include "index/FileEntry.h"

#include <QApplication>
#include <QMetaType>
#include <QVector>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("MSearch"));
    app.setApplicationName(QStringLiteral("MSearch"));
    app.setApplicationVersion(QStringLiteral("0.3.0"));

    qRegisterMetaType<FileEntry>("FileEntry");
    qRegisterMetaType<QVector<FileEntry>>("QVector<FileEntry>");

    MainWindow window;
    window.show();
    return app.exec();
}
