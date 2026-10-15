#include "AppController.hpp"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[])
{
    QGuiApplication::setOrganizationName(QStringLiteral("Thedorius"));
    QGuiApplication::setApplicationName(QStringLiteral("Thedorius"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.3"));
    QGuiApplication app(argc, argv);

    const bool mockMode = app.arguments().contains(QStringLiteral("--mock")) ||
        !app.arguments().contains(QStringLiteral("--live"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Thedorius/qml/assets/fonts/GoogleSans-SemiBold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Thedorius/qml/assets/fonts/GoogleSans-Bold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Thedorius/qml/assets/fonts/FontAwesome-Free-Solid-900.otf"));

    AppController controller(mockMode);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Thedorius"), QStringLiteral("Main"));
    return app.exec();
}
