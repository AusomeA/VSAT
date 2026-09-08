#include <QQmlApplicationEngine>
#include <QGuiApplication>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");
    QQmlApplicationEngine engine;

    engine.loadFromModule("GroundControl", "Main");

    return app.exec();
}