#include <notify/Manager.hpp>

#include <QtCore/QObject>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app::notify::Manager notifications;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        { "notifications"_L1, QVariant::fromValue(&notifications) },
    });
    engine.loadFromModule("Notify.Playground", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    notifications.postMessage(u"Posted from C++"_s);
    return QGuiApplication::exec();
}
