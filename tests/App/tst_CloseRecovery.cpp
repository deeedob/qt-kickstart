#include <App/AppController.hpp>
#include <Canvas/CanvasStore.hpp>
#include <Notify/Notification.hpp>
#include <Notify/NotificationManager.hpp>
#include <Sketchbook/SketchStorage.hpp>
#include <Sketchbook/SketchbookViewModel.hpp>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryDir>
#include <QtGui/QFont>
#include <QtGui/QFontDatabase>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace Qt::StringLiterals;
using App::Canvas::CanvasStore;
using App::Sketchbook::SketchbookViewModel;
using App::Sketchbook::SketchStorage;

namespace {

QString storageDirectory(QTemporaryDir &directory, bool blocked)
{
    QString path = directory.filePath("sketches"_L1);
    if (!blocked)
        return path;
    QFile blocker(directory.filePath("blocker"_L1));
    if (blocker.open(QIODevice::WriteOnly))
        blocker.close();
    return directory.filePath("blocker/child"_L1);
}

struct MainFixture
{
    QTemporaryDir directory;
    SketchStorage storage;
    CanvasStore canvas;
    App::Notify::NotificationManager notifications;
    SketchbookViewModel sketchbook{ storage, canvas, notifications };
    QSettings settings{ directory.filePath("settings.ini"_L1), QSettings::IniFormat };
    App::AppController controller{
        { .logs = QDir(directory.filePath("logs"_L1)), .licenses = QDir(directory.path()) },
        settings, notifications
    };
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;


    explicit MainFixture(bool blockedStorage = false)
        : storage(storageDirectory(directory, blockedStorage))
    {
        controller.setLanguage("en"_L1);
        engine.setExternalSingletonInstance("App", "AppController", &controller);
        QObject::connect(&controller, &App::AppController::languageChanged, &engine,
            &QQmlEngine::retranslate);
        engine.setInitialProperties({
            { "notifications"_L1, QVariant::fromValue(&notifications) },
            { "sketchbook"_L1,    QVariant::fromValue(&sketchbook)    },
            { "canvas"_L1,        QVariant::fromValue(&canvas)        },
        });
        engine.loadFromModule("App", "Main");
        if (!engine.rootObjects().isEmpty())
            window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    }
};

} // namespace

class tst_CloseRecovery : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        const QDir source(QString::fromUtf8(APP_SOURCE_DIR));
        const int fontId = QFontDatabase::addApplicationFont(
            source.filePath("src/Controls/fonts/InterVariable.ttf"_L1));
        QVERIFY(fontId >= 0);
        QGuiApplication::setFont(
            QFont(QFontDatabase::applicationFontFamilies(fontId).constFirst()));
    }

    void init() { QTest::failOnWarning(); }

    void healthyPendingSaveWaitsBeforeClosing()
    {
        MainFixture fixture;
        QVERIFY(fixture.window);
        QTRY_VERIFY(fixture.sketchbook.initialized());

        fixture.sketchbook.create();
        QVERIFY(fixture.sketchbook.hasUnsavedChanges());
        fixture.window->close();

        QVERIFY(fixture.window->isVisible());
        QTRY_VERIFY(!fixture.window->isVisible());
    }

    void repeatedSaveFailuresReuseOneRecoveryDecision()
    {
        MainFixture fixture(true);
        QVERIFY(fixture.window);
        QTRY_VERIFY(fixture.sketchbook.initialized());
        QSignalSpy saves(&fixture.storage, &SketchStorage::mutationFinished);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*Cannot save.*"_s));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*Save sketch failed.*"_s));
        fixture.sketchbook.create();
        QTRY_COMPARE(saves.count(), 1);
        QTRY_COMPARE(fixture.notifications.count(), 1);
        auto *recovery = qvariant_cast<App::Notify::Notification *>(fixture.notifications.data(
            fixture.notifications.index(0), App::Notify::NotificationManager::NotificationRole));
        QVERIFY(recovery);

        fixture.window->close();
        QCOMPARE(fixture.notifications.currentDecision(), recovery);
        recovery->dismiss();
        QTRY_VERIFY(!fixture.notifications.currentDecision());
        QTRY_COMPARE(fixture.notifications.count(), 1);
        recovery = qvariant_cast<App::Notify::Notification *>(fixture.notifications.data(
            fixture.notifications.index(0), App::Notify::NotificationManager::NotificationRole));
        QVERIFY(recovery);
        fixture.window->close();
        QCOMPARE(fixture.notifications.currentDecision(), recovery);
        const auto recoveryId = recovery->id();

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*Cannot save.*"_s));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*Save sketch failed.*"_s));
        fixture.canvas.addStroke(
            {
                { 0,  0  },
                { 10, 10 }
        },
            Qt::black);
        QTRY_COMPARE(saves.count(), 2);
        QCOMPARE(recovery->id(), recoveryId);
        QCOMPARE(recovery->presentation(), App::Notify::Presentation::Decision);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*Cannot save.*"_s));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*Save sketch failed.*"_s));
        fixture.canvas.addStroke(
            {
                { 20, 0  },
                { 30, 10 }
        },
            Qt::black);
        QTRY_COMPARE(saves.count(), 3);
        QCOMPARE(fixture.notifications.currentDecision(), recovery);

        recovery->choose(recovery->actions().at(2).id);
        QTRY_VERIFY(!fixture.notifications.currentDecision());
        QVERIFY(fixture.window->isVisible());

        fixture.window->close();
        QVERIFY(fixture.notifications.currentDecision());
        fixture.notifications.currentDecision()->choose(
            fixture.notifications.currentDecision()->actions().at(1).id);
        QTRY_VERIFY(!fixture.window->isVisible());
    }

    void retryRecoverySavesBeforeClosing()
    {
        MainFixture fixture(true);
        QVERIFY(fixture.window);
        QTRY_VERIFY(fixture.sketchbook.initialized());
        QSignalSpy saves(&fixture.storage, &SketchStorage::mutationFinished);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*Cannot save.*"_s));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u".*Save sketch failed.*"_s));
        fixture.sketchbook.create();
        QTRY_COMPARE(saves.count(), 1);
        fixture.window->close();
        auto *decision = fixture.notifications.currentDecision();
        QVERIFY(decision);
        QVERIFY(QFile::remove(fixture.directory.filePath("blocker"_L1)));

        decision->choose(decision->actions().front().id);

        QTRY_VERIFY(!fixture.window->isVisible());
        const auto id = fixture.sketchbook.model()->entry(0).id;
        QVERIFY(QFile::exists(fixture.directory.filePath(u"blocker/child/%1.json"_s.arg(id))));
    }
};

QTEST_MAIN(tst_CloseRecovery)
#include "tst_CloseRecovery.moc"
