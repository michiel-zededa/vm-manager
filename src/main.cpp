#include "core/AppController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

using namespace vmm;

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.0.0"
#endif

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Identity — also used by QSettings (SnapshotScheduler persistence).
    QGuiApplication::setOrganizationName("VM Manager");
    QGuiApplication::setOrganizationDomain("vm-manager.dev");
    QGuiApplication::setApplicationName("VM Manager");
    QGuiApplication::setApplicationVersion(PROJECT_VERSION);
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    // Neutral base style; the app's look comes from our own QML components.
    QQuickStyle::setStyle("Basic");

    AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("App", &controller);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("VMManager", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    controller.bootstrap();
    return app.exec();
}
