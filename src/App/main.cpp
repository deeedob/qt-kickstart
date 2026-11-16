// The composition root: the only file that knows every concrete type and wires them together.

#include "AppController.hpp"
#include "LogSession.hpp"
#include <App/Gen/BuildInfo.hpp>

#include <Canvas/CanvasStore.hpp>         // demo
#include <Notify/NotificationManager.hpp>
#include <Sketchbook/SketchStorage.hpp>       // demo
#include <Sketchbook/SketchbookViewModel.hpp> // demo

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QLoggingCategory>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtGui/QGuiApplication>
#include <QtGui/QSurfaceFormat>
#include <QtQml/QQmlApplicationEngine>

#include <cstdlib>

using namespace Qt::StringLiterals;

Q_STATIC_LOGGING_CATEGORY(lcMain, "app.main")

int main(int argc, char *argv[])
{
    QGuiApplication::setApplicationName(App::Gen::buildInfo.name());
    QGuiApplication::setApplicationDisplayName(App::Gen::buildInfo.displayName());
    QGuiApplication::setApplicationVersion(App::Gen::buildInfo.version());

    // Multisampling smooths the edges of the strokes the canvas draws as scene graph geometry.
    auto format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    const QGuiApplication application(argc, argv);

    const QDir dataDirectory(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    const QDir logsDirectory(dataDirectory.filePath("logs"_L1));
    const App::LogSession logging(logsDirectory);
    const QDir licensesDirectory(
        QDir(QCoreApplication::applicationDirPath()).filePath("licenses"_L1));

    // Declared before the engine, so they outlive it: QML only holds non-owning references.
    const auto sketchDirectory = dataDirectory.filePath("sketches"_L1); // demo
    qCInfo(lcMain) << "Sketchbook directory:" << sketchDirectory;     // demo
    App::Sketchbook::SketchStorage storage(sketchDirectory);           // demo
    App::Canvas::CanvasStore canvas;                                  // demo
    App::Notify::NotificationManager notifications;
    App::Sketchbook::SketchbookViewModel sketchbook(storage, canvas, notifications); // demo
    QSettings settings;
    App::AppController controller({ .logs = logsDirectory, .licenses = licensesDirectory },
        settings, notifications);
    QQmlApplicationEngine engine;
    QObject::connect(&controller, &App::AppController::languageChanged, &engine,
        &QQmlEngine::retranslate);
    if (!engine.setExternalSingletonInstance("App", "AppController", &controller)) {
        qCCritical(lcMain) << "Cannot register application controller";
        return EXIT_FAILURE;
    }
    engine.setInitialProperties({
        { "notifications"_L1, QVariant::fromValue(&notifications) },
        // demo:begin
        { "sketchbook"_L1,    QVariant::fromValue(&sketchbook)    },
        { "canvas"_L1,        QVariant::fromValue(&canvas)        },
        // demo:end
    });
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] {
            qCCritical(lcMain) << "Cannot create application window";
            QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection);
    engine.loadFromModule("App", "Main");

    return QGuiApplication::exec();
}
