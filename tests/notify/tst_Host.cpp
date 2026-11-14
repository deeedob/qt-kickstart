#include <notify/Manager.hpp>

#include <QtCore/QDir>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QString>
#include <QtGui/QFontDatabase>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace Qt::StringLiterals;

class tst_Host : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        const QDir source(QString::fromUtf8(QT_TESTCASE_SOURCEDIR));
        const int fontId = QFontDatabase::addApplicationFont(
            source.filePath(QStringLiteral("../../src/controls/fonts/InterVariable.ttf")));
        QVERIFY(fontId >= 0);
        QGuiApplication::setFont(
            QFont(QFontDatabase::applicationFontFamilies(fontId).constFirst()));
    }

    void init() { QTest::failOnWarning(); }

    void qmlUsesFeatureTypesDirectly_data()
    {
        QTest::addColumn<bool>("qualified");
        QTest::newRow("qualified") << true;
        QTest::newRow("unqualified") << false;
    }

    void qmlUsesFeatureTypesDirectly()
    {
        QFETCH(bool, qualified);
        app::notify::Manager manager;
        QQmlEngine engine;
        QQmlComponent component(&engine);
        QString source = uR"(
            pragma ComponentBehavior: Bound
            import QtQuick
            import App.Notify as Notify
            QtObject {
                id: root
                required property Notify.Manager manager
                property Notify.action acceptAction: ({ id: "accept", text: "Accept" })
                property Notify.options settings: ({
                    message: "Direct feature types",
                    severity: Notify.Severity.Warning,
                    presentation: Notify.Presentation.Persistent,
                    actions: [root.acceptAction]
                })
                property list<int> severities: [Notify.Severity.Information, Notify.Severity.Success,
                    Notify.Severity.Warning, Notify.Severity.Error]
                property list<int> presentations: [Notify.Presentation.Passive,
                    Notify.Presentation.Persistent, Notify.Presentation.Decision]
                property list<int> finishReasons: [Notify.FinishReason.Action,
                    Notify.FinishReason.Dismissed, Notify.FinishReason.Expired,
                    Notify.FinishReason.Cancelled]
                property int actionReason: Notify.FinishReason.Action
                property int result: -1
                function post(): void {
                    root.manager.post(root.settings, root, function(reason, actionId) {
                        root.result = reason;
                    });
                }
            }
        )"_s;
        if (!qualified) {
            source.replace(u"import App.Notify as Notify"_s, u"import App.Notify"_s);
            source.replace(u"Notify."_s, QString());
        }
        component.setData(source.toUtf8(), QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({
            { u"manager"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(object);
        const auto compareValues = [&object](const char *propertyName, QMetaEnum values) {
            const auto actual = object->property(propertyName).value<QList<int>>();
            QCOMPARE(actual.size(), values.keyCount());
            for (int index = 0; index < values.keyCount(); ++index)
                QCOMPARE(actual.at(index), values.value(index));
        };
        compareValues("severities", QMetaEnum::fromType<app::notify::Severity>());
        compareValues("presentations", QMetaEnum::fromType<app::notify::Presentation>());
        compareValues("finishReasons", QMetaEnum::fromType<app::notify::FinishReason>());
        QVERIFY(QMetaObject::invokeMethod(object.data(), "post"));
        QCOMPARE(manager.count(), 1);
        auto *notification = manager.data(manager.index(0), app::notify::Manager::NotificationRole)
                                 .value<app::notify::Notification *>();
        QVERIFY(notification);
        QCOMPARE(notification->severity(), app::notify::Severity::Warning);
        QCOMPARE(notification->presentation(), app::notify::Presentation::Persistent);
        QCOMPARE(notification->actions().first().text, u"Accept"_s);
        QCOMPARE(object->property("actionReason").toInt(),
            static_cast<int>(app::notify::FinishReason::Action));
        notification->choose(u"accept"_s);
        QCOMPARE(object->property("result").toInt(), object->property("actionReason").toInt());
    }

    void qmlPostsStructuredNotification()
    {
        app::notify::Manager manager;
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import App.Notify as Notify
            QtObject {
                id: root
                required property Notify.Manager manager
                property string result: ""
                property Notify.Notification request: root.manager.post({message: "QML message", presentation: Notify.Presentation.Decision,
                    actions: [{id: "accept", text: "Accept", enabled: true}]}, root,
                    function(reason, actionId) { root.result = actionId; })
                function makePersistent(): void { root.request.setPresentation(Notify.Presentation.Persistent); }
                function makeDecision(): void { root.request.setPresentation(Notify.Presentation.Decision); }
                function updateActions(): void {
                    root.request.actions = [{id: "updated", text: "Updated", enabled: true}];
                }
            }
        )",
            QUrl());
        QScopedPointer<QObject> object(component.createWithInitialProperties({
            { u"manager"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(object);
        auto *message = manager.currentDecision();
        QVERIFY(message);
        QCOMPARE(message->message(), u"QML message"_s);
        QCOMPARE(message->actions().first().id, u"accept"_s);
        QVERIFY(QMetaObject::invokeMethod(object.data(), "makePersistent"));
        QCOMPARE(manager.count(), 1);
        QVERIFY(QMetaObject::invokeMethod(object.data(), "makeDecision"));
        QCOMPARE(manager.currentDecision(), message);
        QVERIFY(QMetaObject::invokeMethod(object.data(), "updateActions"));
        QCOMPARE(message->actions().first().id, u"updated"_s);
        QSignalSpy finished(message, &app::notify::Notification::finished);
        message->choose(u"updated"_s);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(1).toString(), u"updated"_s);
        QCOMPARE(object->property("result").toString(), u"updated"_s);
    }

    void qmlCallbackCompletesDuringPost()
    {
        app::notify::Manager manager;
        connect(&manager, &app::notify::Manager::countChanged, this, [&manager] {
            manager.dismissPassive();
        });
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import App.Notify as Notify
            QtObject {
                id: root
                required property Notify.Manager manager
                property int calls: 0
                property int reason: -1
                Component.onCompleted: root.manager.post({}, root, function(reason, actionId) {
                    root.calls++;
                    root.reason = reason;
                })
            }
        )",
            QUrl());
        QScopedPointer<QObject> object(component.createWithInitialProperties({
            { u"manager"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(object);
        QCOMPARE(object->property("calls").toInt(), 1);
        QCOMPARE(object->property("reason").toInt(),
            static_cast<int>(app::notify::FinishReason::Dismissed));
        QCOMPARE(manager.count(), 0);
    }

    void qmlCallbackFollowsContextLifetime()
    {
        app::notify::Manager manager;
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import App.Notify as Notify
            QtObject {
                id: root
                required property Notify.Manager manager
                property int calls: 0
                property QtObject context: QtObject { objectName: "callbackContext" }
                Component.onCompleted: root.manager.post({}, root.context, function(reason, actionId) {
                    root.calls++;
                })
            }
        )",
            QUrl());
        QScopedPointer<QObject> object(component.createWithInitialProperties({
            { u"manager"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(object);
        QCOMPARE(manager.count(), 1);
        auto *context = object->findChild<QObject *>(u"callbackContext"_s);
        QVERIFY(context);
        delete context;
        QTRY_COMPARE(manager.count(), 0);
        QCOMPARE(object->property("calls").toInt(), 0);
    }

    void qmlCallbackReportsErrors_data()
    {
        QTest::addColumn<QString>("body");
        QTest::addColumn<QByteArray>("warning");
        QTest::newRow("error")
            << u"throw new Error('completion failed');"_s
            << QByteArray("Notification completion callback failed: Error: completion failed");
        QTest::newRow("primitive")
            << u"throw 'completion failed';"_s
            << QByteArray("Notification completion callback failed: completion failed");
        QTest::newRow("returned-error") << u"return new Error('not thrown');"_s << QByteArray();
    }

    void qmlCallbackReportsErrors()
    {
        app::notify::Manager manager;
        QQmlEngine engine;
        QQmlComponent component(&engine);
        QFETCH(QString, body);
        QFETCH(QByteArray, warning);
        const auto source = uR"(
            import QtQuick
            import App.Notify as Notify
            QtObject {
                id: root
                required property Notify.Manager manager
                Component.onCompleted: root.manager.post({}, root, function(reason, actionId) {
                    %1
                })
            }
        )"_s.arg(body);
        component.setData(source.toUtf8(), QUrl());
        QScopedPointer<QObject> object(component.createWithInitialProperties({
            { u"manager"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(object);
        QCOMPARE(manager.count(), 1);
        if (!warning.isEmpty())
            QTest::ignoreMessage(QtWarningMsg, warning.constData());
        manager.dismissPassive();
        QCOMPARE(manager.count(), 0);
    }

    void qmlContextDestructionCancelsNotification()
    {
        app::notify::Manager manager;
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import App.Notify as Notify
            QtObject {
                id: root
                required property Notify.Manager manager
                property Notify.Notification request: root.manager.post({message: "Pending"}, root)
            }
        )",
            QUrl());
        QScopedPointer<QObject> object(component.createWithInitialProperties({
            { u"manager"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(object);
        auto *message = manager.data(manager.index(0), app::notify::Manager::NotificationRole)
                            .value<app::notify::Notification *>();
        QVERIFY(message);
        QSignalSpy finished(message, &app::notify::Notification::finished);
        object.reset();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).value<app::notify::FinishReason>(),
            app::notify::FinishReason::Cancelled);
        QCOMPARE(manager.count(), 0);
    }

    void qmlCanPostWithoutKeepingResult()
    {
        app::notify::Manager manager;
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import App.Notify as Notify
            QtObject {
                id: root
                required property Notify.Manager manager
                Component.onCompleted: root.manager.postMessage("Fire and forget")
            }
        )",
            QUrl());
        QScopedPointer<QObject> object(component.createWithInitialProperties({
            { u"manager"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(object);
        QCOMPARE(manager.count(), 1);
        engine.collectGarbage();
        QCOMPARE(manager.count(), 1);
        auto *message = manager.data(manager.index(0), app::notify::Manager::NotificationRole)
                            .value<app::notify::Notification *>();
        QVERIFY(message);
        QCOMPARE(message->message(), u"Fire and forget"_s);
        message->dismiss();
        QCOMPARE(manager.count(), 0);
    }

    void cardsRemainIndependentUntilDismissed()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Persistent;
        options.message = u"First message"_s;
        QQmlEngine engine;
        QQuickWindow window;
        window.resize(640, 480);
        QScopedPointer<QObject> root(createHost(engine, manager, window));
        QVERIFY(root);
        window.show();
        app::notify::Notification *first = manager.postMessage(options.message);
        auto *cardList = root->findChild<QObject *>(u"cardList"_s);
        QVERIFY(cardList);
        QQuickItem *initial = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(cardList, "itemAtIndex",
                        Q_RETURN_ARG(QQuickItem *, initial), Q_ARG(int, 0))
            && initial);
        options.title = u"Second title"_s;
        options.message = u"Second message"_s;
        app::notify::Notification *second = manager.post(options, this);

        QTRY_COMPARE(cardList->property("count").toInt(), 2);

        const auto titleAt = [cardList](int index) {
            QQuickItem *delegate = nullptr;
            if (!QMetaObject::invokeMethod(cardList, "itemAtIndex",
                    Q_RETURN_ARG(QQuickItem *, delegate), Q_ARG(int, index))
                || !delegate)
                return QString();
            auto *title = delegate->findChild<QObject *>(u"notificationTitle"_s);
            return title ? title->property("text").toString() : QString();
        };
        QTRY_COMPARE(titleAt(0), u"Second title"_s);
        QTRY_COMPARE(titleAt(1), u"First message"_s);

        first->dismiss();
        QTRY_COMPARE(manager.count(), 1);
        QCOMPARE(cardList->property("count").toInt(), 1);
        QVERIFY(!second->isFinished());
    }

    void overflowShowsWaitingRequests()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Persistent;
        for (int index = 0; index < 4; ++index) {
            options.message = u"Notification %1"_s.arg(index);
            QVERIFY(manager.post(options, this));
        }

        QQmlEngine engine;
        QQuickWindow window;
        window.resize(640, 480);
        QScopedPointer<QObject> root(createHost(engine, manager, window));
        QVERIFY(root);
        window.show();

        auto *cardList = root->findChild<QObject *>(u"cardList"_s);
        QVERIFY(cardList);
        QTRY_COMPARE(cardList->property("count").toInt(), 4);
        QVERIFY(!root->findChild<QObject *>(u"overflowDialog"_s));
    }

    void scrollingUpdatesCardPresentation()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Persistent;
        for (int i = 0; i < 20; ++i)
            QVERIFY(manager.post(options, this));
        QQmlEngine engine;
        QQuickWindow window;
        window.resize(640, 480);
        QScopedPointer<QObject> root(createHost(engine, manager, window));
        QVERIFY(root);
        window.show();
        auto *list = root->findChild<QObject *>(u"cardList"_s);
        QVERIFY(list);
        QVERIFY(list->setProperty("cacheBuffer", 2000));
        QQuickItem *delegate = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex",
                        Q_RETURN_ARG(QQuickItem *, delegate), Q_ARG(int, 0))
            && delegate);
        auto *card = delegate;
        QVERIFY(card);
        QTRY_VERIFY(card->property("presented").toBool());
        QVERIFY(list->setProperty("contentY", 250));
        QTRY_VERIFY(!card->property("presented").toBool());
        QVERIFY(list->setProperty("contentY", 0));
        QTRY_VERIFY(card->property("presented").toBool());
    }

    void cardsHaveSubtleBorder()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Persistent;
        QVERIFY(manager.post(options, this));
        QQmlEngine engine;
        QQuickWindow window;
        window.resize(640, 480);
        QScopedPointer<QObject> root(createHost(engine, manager, window));
        QVERIFY(root);
        window.show();
        auto *list = root->findChild<QObject *>(u"cardList"_s);
        QVERIFY(list);
        QQuickItem *delegate = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex",
                        Q_RETURN_ARG(QQuickItem *, delegate), Q_ARG(int, 0))
            && delegate);
        auto *surface = delegate->findChild<QObject *>(u"notificationSurface"_s);
        QVERIFY(surface);
        auto *border = qvariant_cast<QObject *>(surface->property("border"));
        QVERIFY(border);
        QCOMPARE(border->property("width").toInt(), 1);
    }

    void cardsWrapAndActionsRemainVisibleAtNarrowWidth()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Persistent;
        options.title = u"A long notification title that needs two lines"_s;
        options.message = QStringLiteral("This notification message wraps across the card. ")
                              .repeated(5);
        options.actions = {
            { u"retry"_s, u"Try again from here"_s, true },
            { u"keep"_s,  u"Save this draft now"_s, true },
        };
        QVERIFY(manager.post(options, this));

        QQmlEngine engine;
        QQuickWindow window;
        window.resize(360, 360);
        QScopedPointer<QObject> root(createHost(engine, manager, window));
        QVERIFY(root);
        auto *rootItem = qobject_cast<QQuickItem *>(root.data());
        QVERIFY(rootItem);
        window.show();
        QTRY_COMPARE(rootItem->width(), 360);
        QTRY_COMPARE(rootItem->height(), 360);

        auto *cardList = root->findChild<QObject *>(u"cardList"_s);
        QVERIFY(cardList);
        QTRY_COMPARE(cardList->property("count").toInt(), 1);
        QQuickItem *delegate = nullptr;
        QTRY_VERIFY(cardList->property("height").toReal() > 0);
        QTRY_VERIFY(QMetaObject::invokeMethod(cardList, "itemAtIndex",
                        Q_RETURN_ARG(QQuickItem *, delegate), Q_ARG(int, 0))
            && delegate);
        auto *card = delegate;
        auto *message = card ? card->findChild<QObject *>(u"notificationMessage"_s) : nullptr;
        auto *header = card ? card->findChild<QQuickItem *>(u"notificationHeader"_s) : nullptr;
        auto *badge = card ? card->findChild<QQuickItem *>(u"severityBadge"_s) : nullptr;
        auto *title = card ? card->findChild<QQuickItem *>(u"notificationTitle"_s) : nullptr;
        auto *actions = card ? card->findChild<QQuickItem *>(u"actions"_s) : nullptr;
        auto *repeater = card ? card->findChild<QObject *>(u"actionRepeater"_s) : nullptr;
        QQuickItem *button = nullptr;
        QVERIFY(card);
        QVERIFY(message);
        QVERIFY(header);
        QVERIFY(badge);
        QVERIFY(title);
        QVERIFY(title->mapToItem(header, QPointF()).x()
                - (badge->mapToItem(header, QPointF()).x() + badge->width())
            >= 8);
        QVERIFY(actions);
        QVERIFY(repeater);
        QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Q_RETURN_ARG(QQuickItem *, button),
            Q_ARG(int, 0)));
        QVERIFY(button);

        const qreal narrowMessageHeight = message->property("implicitHeight").toReal();
        const qreal narrowActionsHeight = actions->height();
        QVERIFY(card->width() <= 336);
        QVERIFY(narrowMessageHeight > 28);
        QVERIFY(narrowActionsHeight >= 28);
        QVERIFY(button->isVisible());
        QVERIFY(button->width() <= card->width());

        window.resize(640, 480);
        QTRY_VERIFY(cardList->property("width").toReal() >= 320);
        QTRY_VERIFY(message->property("implicitHeight").toReal() <= narrowMessageHeight);
        QTRY_VERIFY(actions->width() <= card->width());
        QTRY_VERIFY(button->isVisible());
    }

    void decisionsSerializeEscapeAndRestoreFocus()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Decision;
        options.actions = {
            { u"accept"_s, u"Accept"_s, true }
        };
        options.message = u"First decision"_s;

        QQmlEngine engine;
        QQuickWindow window;
        window.resize(360, 360);
        QScopedPointer<QObject> root(createHost(engine, manager, window));
        QVERIFY(root);
        auto *rootItem = qobject_cast<QQuickItem *>(root.data());
        QVERIFY(rootItem);
        window.show();
        rootItem->forceActiveFocus();
        QTRY_COMPARE(window.activeFocusItem(), rootItem);
        QObject *originalFocus = window.activeFocusItem();

        app::notify::Notification *first = manager.post(options, this);
        options.message = u"Second decision"_s;
        app::notify::Notification *second = manager.post(options, this);

        auto *dialog = root->findChild<QObject *>(u"decisionDialog"_s);
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->findChild<QObject *>(u"notificationDecision"_s));
        QTRY_VERIFY(dialog->property("height").toReal() > 0);
        QVERIFY(dialog->property("width").toReal() > 0);
        QVERIFY(dialog->property("width").toReal() <= 336);
        QVERIFY(dialog->property("height").toReal() <= 336);
        QCOMPARE(manager.currentDecision(), first);
        QTRY_VERIFY(window.activeFocusItem() != originalFocus);

        QTest::keyClick(&window, Qt::Key_Escape);
        QTRY_COMPARE(manager.currentDecision(), second);
        QTRY_VERIFY(dialog->property("visible").toBool());

        QTest::keyClick(&window, Qt::Key_Escape);
        QTRY_VERIFY(!manager.currentDecision());
        QTRY_COMPARE(window.activeFocusItem(), originalFocus);
    }

    void decisionUsesCardActions()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Decision;
        options.title = u"Choose"_s;
        options.message = u"A long message. "_s.repeated(80);
        bool invoked = false;
        options.actions = {
            { u"disabled"_s, u"Unavailable"_s, false },
            { u"accept"_s,   u"Accept"_s,      true  },
        };
        QQmlEngine engine;
        QQuickWindow window;
        window.resize(360, 360);
        QScopedPointer<QObject> root(createHost(engine, manager, window));
        QVERIFY(root);
        window.show();
        auto *message = manager.post(options, this);
        QVERIFY(message);
        connect(message, &app::notify::Notification::finished, this,
            [&invoked](app::notify::FinishReason reason, const QString &id) {
                invoked = reason == app::notify::FinishReason::Action && id == u"accept"_s;
            });
        auto *dialog = root->findChild<QObject *>(u"decisionDialog"_s);
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTRY_VERIFY(window.activeFocusItem());
        QTRY_COMPARE(window.activeFocusItem()->property("text").toString(), u"Accept"_s);
        QTRY_VERIFY(window.activeFocusItem()->mapToScene(QPointF(0, 0)).y()
                + window.activeFocusItem()->height()
            <= dialog->property("y").toReal() + dialog->property("height").toReal());
        QTest::keyClick(&window, Qt::Key_Space);
        QTRY_VERIFY(invoked);
        QTRY_VERIFY(!manager.currentDecision());
    }

    void cardCanBecomeDecisionAndReturn()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Persistent;
        options.message = u"Recovery"_s;
        auto *message = manager.post(options, this);
        QVERIFY(message);
        QQmlEngine engine;
        QQuickWindow window;
        window.resize(640, 480);
        QScopedPointer<QObject> root(createHost(engine, manager, window));
        QVERIFY(root);
        window.show();
        auto *dialog = root->findChild<QObject *>(u"decisionDialog"_s);
        QVERIFY(dialog);
        message->setPresentation(app::notify::Presentation::Decision);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTRY_VERIFY(dialog->property("height").toReal() > 0);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
        QCOMPARE(manager.currentDecision(), message);
        message->setPresentation(app::notify::Presentation::Persistent);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(manager.count(), 1);
        QVERIFY(!message->isFinished());
    }

    void hostAcceptsCustomViews()
    {
        app::notify::Manager manager;
        QQmlEngine engine;
        QQuickWindow window;
        window.resize(640, 480);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import App.Notify as Notify
            Notify.Host {
                id: testHost
                required property Notify.Manager testManager
                manager: testManager
                width: 640
                height: 480
                notificationDelegate: Component {
                    Rectangle {
                        objectName: "customNotification"
                        required property Notify.Notification notification
                        property string message: notification.message
                        implicitHeight: 30
                    }
                }
                decisionDelegate: Component {
                    Rectangle {
                        objectName: "customDecision"
                        property Notify.Notification notification: testHost.manager.currentDecision
                        property string message: notification ? notification.message : ""
                        implicitHeight: 40
                    }
                }
            }
        )",
            QUrl());
        QScopedPointer<QObject> root(component.createWithInitialProperties({
            { u"testManager"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(root);
        auto *item = qobject_cast<QQuickItem *>(root.data());
        QVERIFY(item);
        item->setParentItem(window.contentItem());
        window.show();
        app::notify::Options options;
        options.message = u"Custom message"_s;
        options.presentation = app::notify::Presentation::Persistent;
        QVERIFY(manager.post(options, this));
        auto *list = root->findChild<QObject *>(u"cardList"_s);
        QVERIFY(list);
        QQuickItem *delegate = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex",
                        Q_RETURN_ARG(QQuickItem *, delegate), Q_ARG(int, 0))
            && delegate);
        auto *custom = static_cast<QObject *>(delegate);
        QVERIFY(custom);
        QCOMPARE(custom->property("message").toString(), u"Custom message"_s);
        options.presentation = app::notify::Presentation::Decision;
        QVERIFY(manager.post(options, this));
        QTRY_VERIFY(root->findChild<QObject *>(u"customDecision"_s));
        QCOMPARE(root->findChild<QObject *>(u"customDecision"_s)->property("message").toString(),
            u"Custom message"_s);
    }

    void dialogProvidesDefaultContent()
    {
        app::notify::Manager manager;
        app::notify::Options options;
        options.presentation = app::notify::Presentation::Decision;
        auto *message = manager.post(options, this);
        QVERIFY(message);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import App.Notify as Notify
            Notify.Dialog {
                required property Notify.Notification message
                notification: message
            }
        )",
            QUrl());
        QScopedPointer<QObject> dialog(component.createWithInitialProperties({
            { u"message"_s, QVariant::fromValue(message) }
        }));
        QVERIFY(dialog);
        QVERIFY(dialog->findChild<QObject *>(u"notificationDecision"_s));
    }

    void playgroundPostsAndReportsCompletion()
    {
        app::notify::Manager manager;
        QQmlEngine engine;
        const QDir source(QString::fromUtf8(QT_TESTCASE_SOURCEDIR));
        QQmlComponent component(&engine,
            QUrl::fromLocalFile(source.filePath(u"../manual/notify/Main.qml"_s)));
        QScopedPointer<QObject> root(component.createWithInitialProperties({
            { u"notifications"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(root);
        QVERIFY(QMetaObject::invokeMethod(root.data(), "post"));
        QCOMPARE(manager.count(), 1);
        auto *message = manager.data(manager.index(0), app::notify::Manager::NotificationRole)
                            .value<app::notify::Notification *>();
        QVERIFY(message);
        QCOMPARE(message->actions().size(), 3);
        message->choose(u"accept"_s);
        QCOMPARE(root->property("result").toString(), u"Finished: 0, action: accept"_s);
    }

    void playgroundCustomDecisionHasPaddingAndActions()
    {
        app::notify::Manager manager;
        QQmlEngine engine;
        const QDir source(QString::fromUtf8(QT_TESTCASE_SOURCEDIR));
        QQmlComponent component(&engine,
            QUrl::fromLocalFile(source.filePath(u"../manual/notify/Main.qml"_s)));
        QScopedPointer<QObject> root(component.createWithInitialProperties({
            { u"notifications"_s, QVariant::fromValue(&manager) }
        }));
        QVERIFY(root);
        auto *customViews = root->findChild<QObject *>(u"customViews"_s);
        QVERIFY(customViews);
        QVERIFY(customViews->setProperty("checked", true));
        app::notify::Options options;
        options.title = u"Decision"_s;
        options.presentation = app::notify::Presentation::Decision;
        options.actions = {
            { u"accept"_s,   u"Accept"_s,      true  },
            { u"disabled"_s, u"Unavailable"_s, false },
            { u"cancel"_s,   u"Cancel"_s,      true  },
        };
        auto *decision = manager.post(options, this);
        QVERIFY(decision);
        QTRY_VERIFY(root->findChild<QQuickItem *>(u"customDecisionView"_s));
        auto *view = root->findChild<QQuickItem *>(u"customDecisionView"_s);
        auto *content = view->findChild<QQuickItem *>(u"customDecisionContent"_s);
        QVERIFY(content);
        QTRY_VERIFY(content->y() >= 8);
        QVERIFY(content->y() + content->height() <= view->height() - 8);
        auto *actions = view->findChild<QObject *>(u"customDecisionActions"_s);
        QVERIFY(actions);
        QCOMPARE(actions->property("count").toInt(), 3);
        QQuickItem *disabled = nullptr;
        QVERIFY(QMetaObject::invokeMethod(actions, "itemAt", Q_RETURN_ARG(QQuickItem *, disabled),
            Q_ARG(int, 1)));
        QVERIFY(disabled);
        QVERIFY(!disabled->isEnabled());
        QQuickItem *cancel = nullptr;
        QVERIFY(QMetaObject::invokeMethod(actions, "itemAt", Q_RETURN_ARG(QQuickItem *, cancel),
            Q_ARG(int, 2)));
        QVERIFY(cancel);
        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        QSignalSpy finished(decision, &app::notify::Notification::finished);
        QTRY_VERIFY(window->activeFocusItem());
        QTRY_COMPARE(window->activeFocusItem()->property("text").toString(), u"Accept"_s);
        cancel->forceActiveFocus();
        QTRY_COMPARE(window->activeFocusItem(), cancel);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(1).toString(), u"cancel"_s);

        QVERIFY(manager.post(options, this));
        QVERIFY(customViews->setProperty("checked", false));
        QTRY_VERIFY(root->findChild<QObject *>(u"notificationDecision"_s));
        QTRY_VERIFY(!root->findChild<QObject *>(u"customDecisionView"_s));
        QTRY_VERIFY(window->activeFocusItem());
        QTRY_COMPARE(window->activeFocusItem()->property("text").toString(), u"Accept"_s);
    }

private:
    QObject *createHost(QQmlEngine &engine, app::notify::Manager &manager, QQuickWindow &window)
    {
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import App.Notify as Notify
            import App.Controls

            Item {
                required property Notify.Manager manager
                width: parent ? parent.width : 0
                height: parent ? parent.height : 0
                Notify.Host {
                    anchors.fill: parent
                    manager: parent.manager
                }
            }
        )",
            QUrl());
        QObject *root = component.createWithInitialProperties({
            { u"manager"_s, QVariant::fromValue(&manager) }
        });
        if (auto *rootItem = qobject_cast<QQuickItem *>(root))
            rootItem->setParentItem(window.contentItem());
        return root;
    }
};

QTEST_MAIN(tst_Host)
#include "tst_Host.moc"
