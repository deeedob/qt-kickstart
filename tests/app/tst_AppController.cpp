#include <BuildInfo.hpp>
#include <app/AppController.hpp>

#include <QtCore/QMetaProperty>
#include <QtCore/QRegularExpression>

#include <QtCore/QDir>
#include <QtCore/QLibraryInfo>
#include <QtCore/QSettings>
#include <QtCore/QSysInfo>
#include <QtCore/QTemporaryDir>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QTest>
#include <notify/Notification.hpp>
#include <notify/NotificationManager.hpp>

using namespace Qt::StringLiterals;

namespace {

app::AppController::Directories directories(const QTemporaryDir &dir)
{
    return {
        .logs = QDir(dir.filePath("logs"_L1)),
        .licenses = QDir(dir.path()),
    };
}

} // namespace

class tst_AppController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void cleanup() { QDesktopServices::unsetUrlHandler("file"_L1); }

    void opensTheLicensesDirectory()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"_L1), QSettings::IniFormat);
        settings.setValue("theme/dark"_L1, false);
        app::notify::NotificationManager notifications;
        app::AppController controller(directories(dir), settings, notifications);
        QDesktopServices::setUrlHandler("file"_L1, this, "recordOpenedUrl");

        controller.openLicensesDirectory();

        QCOMPARE(m_openedUrl, QUrl::fromLocalFile(dir.path()));
    }

    void opensTheLogsDirectory()
    {
        QTemporaryDir dir;
        const auto appDirectories = directories(dir);
        QVERIFY(appDirectories.logs.mkpath("."_L1));
        QSettings settings(dir.filePath("settings.ini"_L1), QSettings::IniFormat);
        settings.setValue("theme/dark"_L1, false);
        app::notify::NotificationManager notifications;
        app::AppController controller(appDirectories, settings, notifications);
        QDesktopServices::setUrlHandler("file"_L1, this, "recordOpenedUrl");

        controller.openLogDirectory();

        QCOMPARE(m_openedUrl, QUrl::fromLocalFile(appDirectories.logs.absolutePath()));
    }

    void missingTranslationIsLoggedAndRetained()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"_L1), QSettings::IniFormat);
        app::notify::NotificationManager notifications;
        app::AppController controller(directories(dir), settings, notifications);
        const auto initialLanguage = controller.language();
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(u"No translation for language.*unsupported"_s));

        controller.setLanguage("unsupported"_L1);

        QCOMPARE(controller.language(), initialLanguage);
        QCOMPARE(notifications.count(), 1);
        auto *notification = notifications
                                 .data(notifications.index(0),
                                     app::notify::NotificationManager::NotificationRole)
                                 .value<app::notify::Notification *>();
        QVERIFY(notification);
        QCOMPARE(notification->severity(), app::notify::Severity::Error);
        QCOMPARE(notification->presentation(), app::notify::Presentation::Persistent);
    }

    void persistsTheThemeChoice()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"_L1), QSettings::IniFormat);
        settings.setValue("theme/dark"_L1, false);
        app::notify::NotificationManager notifications;
        app::AppController controller(directories(dir), settings, notifications);

        controller.setDarkTheme(true);

        QVERIFY(controller.darkTheme());
        QCOMPARE(settings.value("theme/dark"_L1).toBool(), true);
    }

    void exposesStructuredBuildDetails()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"_L1), QSettings::IniFormat);
        app::notify::NotificationManager notifications;
        app::AppController controller(directories(dir), settings, notifications);

        const auto details = controller.buildDetails();
        QVERIFY(!details.gitSha().isEmpty());
        QVERIFY(!details.buildType().isEmpty());
        QVERIFY(!details.buildTimestamp().isEmpty());
        QVERIFY(!details.compiler().isEmpty());
        QVERIFY(!details.system().isEmpty());
        QCOMPARE(details.qtVersion(), QString::fromLatin1(QT_VERSION_STR));
        QCOMPARE(details.sharedQt(), QLibraryInfo::isSharedBuild());
        const app::RuntimeInfo runtime = controller.runtimeInfo();
        QCOMPARE(runtime.operatingSystem(), QSysInfo::prettyProductName());
        QCOMPARE(runtime.kernelVersion(),
            QSysInfo::kernelType() + " "_L1 + QSysInfo::kernelVersion());
        QCOMPARE(runtime.processArchitecture(), QSysInfo::buildCpuArchitecture());
        QCOMPARE(runtime.windowSystem(), QGuiApplication::platformName());
    }

    void exposesGeneratedBuildInfo()
    {
        const auto &meta = app::BuildInfo::staticMetaObject;
        QVERIFY(meta.indexOfProperty("gitSha") >= 0);
        QVERIFY(meta.indexOfProperty("qtVersion") >= 0);
        QVERIFY(meta.indexOfProperty("sharedQt") >= 0);
    }

    void qmlReadsGeneratedBuildInfo()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"_L1), QSettings::IniFormat);
        app::notify::NotificationManager notifications;
        app::AppController controller(directories(dir), settings, notifications);
        QQmlEngine engine;
        engine.setExternalSingletonInstance("App"_L1, "AppController"_L1, &controller);
        QQmlComponent component(&engine);
        component.setData(R"(
            pragma ComponentBehavior: Bound

            import QtQml

            import App

            QtObject {
                property string sha: AppController.buildDetails.gitSha
                property runtimeInfo info: AppController.runtimeInfo
                property string osName: info.operatingSystem
                property string kernel: info.kernelVersion
                property string architecture: info.processArchitecture
                property string windowSystem: info.windowSystem
                property bool shared: AppController.buildDetails.sharedQt
            }
        )",
            QUrl());
        const std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QCOMPARE(object->property("sha").toString(), app::buildInfo.gitSha());
        QCOMPARE(object->property("osName").toString(), QSysInfo::prettyProductName());
        QCOMPARE(object->property("kernel").toString(),
            QSysInfo::kernelType() + " "_L1 + QSysInfo::kernelVersion());
        QCOMPARE(object->property("architecture").toString(), QSysInfo::buildCpuArchitecture());
        QCOMPARE(object->property("windowSystem").toString(), QGuiApplication::platformName());
        QCOMPARE(object->property("shared").toBool(), QLibraryInfo::isSharedBuild());
    }

    void recordOpenedUrl(const QUrl &url) { m_openedUrl = url; }

private:
    QUrl m_openedUrl;
};

QTEST_GUILESS_MAIN(tst_AppController)
#include "tst_AppController.moc"
