#include <notify/NotificationManager.hpp>

#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaProperty>
#include <QtCore/QMetaType>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtQml/QJSEngine>
#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <array>
#include <utility>

using namespace Qt::StringLiterals;

namespace {

class DestructionContext : public QObject
{
public:
    DestructionContext() = default;
    Q_DISABLE_COPY_MOVE(DestructionContext)
    ~DestructionContext() override { destroying = true; }
    bool destroying = false;
};

} // namespace

class tst_NotificationManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void postsSimpleMessage();
    void postCallbackFinishesOnce();
    void postCallbackHandlesSynchronousCompletion();
    void postCallbackFollowsContextLifetime();
    void rejectedPostDoesNotCallCallback();
    void javascriptCallbackFollowsEngineLifetime();
    void severityIsConstant();
    void severitySymbols();
    void enumTypesBelongToFeatureNamespace();
    void rejectsReentrantlyFinishedPost();
    void keepsOverlappingMessages();
    void dismissesOnlyPassiveMessages();
    void dismissPassiveHandlesDeletedMessages();
    void newestMessagesFollowPriority();
    void finishesOnceBeforeNotifying();
    void cancelsWhenContextIsDestroyed();
    void defersCancellationUntilContextDestructionCompletes();
    void cancelsAllRequestsOnShutdown();
    void passiveTimeoutStartsWhenPresented();
    void reportsIndependentTimerProgress();
    void prioritizesWaitingCardsWithoutMovingVisibleCards();
    void serializesDecisionRequests();
    void handlesCompletionDuringPresentationChange();
    void modalCompletionResumesCardTimeout();
    void presentedOverflowCardsCanExpire();
    void ignoresUnknownAndDisabledActions();
    void resumesOverdueCardsWithoutSynchronousCompletion();
    void autoIdsActionsAndFinishesOnce();
    void disconnectsReceiverWhenContextDiesDuringFinish();
    void callsConnectedReceiverWithLiveContext();
};

void tst_NotificationManager::init()
{
    QTest::failOnWarning();
}

void tst_NotificationManager::severitySymbols()
{
    app::notify::NotificationManager manager;
    QObject context;
    const std::array<std::pair<app::notify::Severity, QString>, 4> cases = {
        {
         { app::notify::Severity::Information, u"i"_s },
         { app::notify::Severity::Success, u"✓"_s },
         { app::notify::Severity::Warning, u"!"_s },
         { app::notify::Severity::Error, u"×"_s },
         }
    };
    for (const auto &[severity, symbol] : cases) {
        app::notify::Options options;
        options.severity = severity;
        auto *notification = manager.post(options, &context);
        QVERIFY(notification);
        QCOMPARE(notification->property("severitySymbol").toString(), symbol);
    }
}

void tst_NotificationManager::postCallbackFinishesOnce()
{
    app::notify::NotificationManager manager;
    QObject context;
    int calls = 0;
    auto *notification = manager.post(
        { .title = {}, .message = {}, .actions = { { u"save"_s, u"Save"_s, true } } }, &context,
        [&calls](app::notify::Result reason, const QString &actionId) {
            ++calls;
            QCOMPARE(reason, app::notify::Result::Action);
            QCOMPARE(actionId, u"save"_s);
        });
    QVERIFY(notification);
    notification->choose(u"save"_s);
    notification->dismiss();
    QCOMPARE(calls, 1);
}

void tst_NotificationManager::postCallbackHandlesSynchronousCompletion()
{
    app::notify::NotificationManager manager;
    QObject context;
    int calls = 0;
    connect(&manager, &app::notify::NotificationManager::countChanged, &context, [&manager] {
        manager.dismissPassive();
    });
    auto *notification = manager.post({}, &context,
        [&calls](app::notify::Result reason, const QString &actionId) {
            ++calls;
            QCOMPARE(reason, app::notify::Result::Dismissed);
            QVERIFY(actionId.isEmpty());
        });
    QCOMPARE(notification, nullptr);
    QCOMPARE(calls, 1);
}

void tst_NotificationManager::postCallbackFollowsContextLifetime()
{
    app::notify::NotificationManager manager;
    auto *context = new QObject;
    int calls = 0;
    auto *notification = manager.post({}, context, [&calls](app::notify::Result, const QString &) {
        ++calls;
    });
    QVERIFY(notification);
    QSignalSpy finished(notification, &app::notify::Notification::finished);
    delete context;
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(calls, 0);
}

void tst_NotificationManager::rejectedPostDoesNotCallCallback()
{
    app::notify::NotificationManager manager;
    QObject context;
    int calls = 0;
    const auto callback = [&calls](app::notify::Result, const QString &) {
        ++calls;
    };
    QVERIFY(manager.post({}, nullptr, callback) == nullptr);
    manager.shutdown();
    QVERIFY(manager.post({}, &context, callback) == nullptr);
    QCOMPARE(calls, 0);
}

void tst_NotificationManager::javascriptCallbackFollowsEngineLifetime()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Notification *pending = nullptr;
    {
        QJSEngine engine;
        QJSEngine::setObjectOwnership(&context, QJSEngine::CppOwnership);
        engine.globalObject().setProperty(u"context"_s, engine.newQObject(&context));
        const auto callback = engine.evaluate(
            u"(function(reason, actionId) { context.objectName = actionId; })"_s);
        auto *notification = manager.post(
            { .title = {}, .message = {}, .actions = { { u"save"_s, u"Save"_s, true } } }, &context,
            callback);
        QVERIFY(notification);
        notification->choose(u"save"_s);
        QCOMPARE(context.objectName(), u"save"_s);
        pending = manager.post({}, &context, callback);
        QVERIFY(pending);
    }
    pending->dismiss();
    QCOMPARE(context.objectName(), u"save"_s);
    QCOMPARE(manager.count(), 0);
}

void tst_NotificationManager::postsSimpleMessage()
{
    app::notify::NotificationManager manager;
    manager.postMessage(u"Hello"_s);
    QCOMPARE(manager.count(), 1);
    auto *message = manager
                        .data(manager.index(0, 0),
                            app::notify::NotificationManager::NotificationRole)
                        .value<app::notify::Notification *>();
    QVERIFY(message);
    QCOMPARE(message->message(), u"Hello"_s);
    message->dismiss();
    QCOMPARE(manager.count(), 0);
}

void tst_NotificationManager::enumTypesBelongToFeatureNamespace()
{
    QCOMPARE(QMetaType::fromType<app::notify::Severity>().name(), "app::notify::Severity");
    QCOMPARE(QMetaType::fromType<app::notify::Presentation>().name(), "app::notify::Presentation");
    QCOMPARE(QMetaType::fromType<app::notify::Result>().name(), "app::notify::Result");
}

void tst_NotificationManager::severityIsConstant()
{
    const auto &metaObject = app::notify::Notification::staticMetaObject;
    const int index = metaObject.indexOfProperty("severity");
    QVERIFY(index >= 0);
    QVERIFY(metaObject.property(index).isConstant());
}

void tst_NotificationManager::rejectsReentrantlyFinishedPost()
{
    app::notify::NotificationManager manager;
    QObject context;
    connect(&manager, &app::notify::NotificationManager::countChanged, &manager, [&manager] {
        if (manager.count() == 0)
            return;
        auto *message = manager
                            .data(manager.index(0),
                                app::notify::NotificationManager::NotificationRole)
                            .value<app::notify::Notification *>();
        message->dismiss();
    });
    QVERIFY(manager.post({}, &context) == nullptr);
    QCOMPARE(manager.count(), 0);
}

void tst_NotificationManager::keepsOverlappingMessages()
{
    app::notify::NotificationManager manager;
    QObject context;

    auto *first = manager.post({ .title = {}, .message = u"first"_s, .actions = {} }, &context);
    auto *second = manager.post({ .title = {}, .message = u"second"_s, .actions = {} }, &context);

    QVERIFY(first);
    QVERIFY(second);
    QVERIFY(first->id() != second->id());
    QCOMPARE(manager.rowCount(), 2);
    QCOMPARE(manager.data(manager.index(0), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        second);
    QCOMPARE(manager.data(manager.index(1), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        first);
}

void tst_NotificationManager::dismissesOnlyPassiveMessages()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    auto *passive = manager.post(options, &context);
    options.presentation = app::notify::Presentation::Persistent;
    auto *persistent = manager.post(options, &context);
    options.presentation = app::notify::Presentation::Decision;
    auto *decision = manager.post(options, &context);
    manager.dismissPassive();
    QVERIFY(passive->isFinished());
    QVERIFY(!persistent->isFinished());
    QVERIFY(!decision->isFinished());
    QCOMPARE(manager.count(), 1);
}

void tst_NotificationManager::dismissPassiveHandlesDeletedMessages()
{
    app::notify::NotificationManager manager;
    QObject context;
    auto *later = manager.post({ .title = {}, .message = u"later"_s, .actions = {} }, &context);
    auto *first = manager.post({ .title = {}, .message = u"first"_s, .actions = {} }, &context);
    QPointer<app::notify::Notification> guardedLater(later);
    connect(first, &app::notify::Notification::finished, &manager,
        [later](app::notify::Result, const QString &) {
            later->dismiss();
            delete later;
        });

    manager.dismissPassive();

    QVERIFY(guardedLater.isNull());
    QCOMPARE(manager.count(), 0);
}

void tst_NotificationManager::newestMessagesFollowPriority()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    auto *first = manager.post(options, &context);
    auto *second = manager.post(options, &context);
    options.severity = app::notify::Severity::Error;
    auto *urgent = manager.post(options, &context);
    QCOMPARE(manager.data(manager.index(0), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        urgent);
    QCOMPARE(manager.data(manager.index(1), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        second);
    QCOMPARE(manager.data(manager.index(2), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        first);
}

void tst_NotificationManager::finishesOnceBeforeNotifying()
{
    app::notify::NotificationManager manager;
    QObject context;
    auto *notification = manager.post({ .title = {}, .message = u"finish"_s, .actions = {} },
        &context);
    QVERIFY(notification);
    QPointer<app::notify::Notification> guardedNotification(notification);

    QSignalSpy finished(notification, &app::notify::Notification::finished);
    connect(notification, &app::notify::Notification::finished, &manager,
        [&manager](app::notify::Result, const QString &) { QCOMPARE(manager.rowCount(), 0); });

    notification->dismiss();
    notification->dismiss();

    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(notification->isFinished());
    QCOMPARE(manager.rowCount(), 0);
    QTRY_VERIFY(guardedNotification.isNull());
}

void tst_NotificationManager::cancelsWhenContextIsDestroyed()
{
    app::notify::NotificationManager manager;
    auto *context = new QObject;
    auto *notification = manager.post({ .title = {}, .message = u"context"_s, .actions = {} },
        context);
    QVERIFY(notification);
    QSignalSpy finished(notification, &app::notify::Notification::finished);

    delete context;

    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.first().at(0).value<app::notify::Result>(), app::notify::Result::Cancelled);
    QTRY_COMPARE(manager.rowCount(), 0);
}

void tst_NotificationManager::defersCancellationUntilContextDestructionCompletes()
{
    app::notify::NotificationManager manager;
    auto *context = new DestructionContext;
    auto *notification = manager.post({ .title = {}, .message = u"context"_s, .actions = {} },
        context);
    QVERIFY(notification);
    QSignalSpy finished(notification, &app::notify::Notification::finished);
    bool calledDuringDestruction = false;
    connect(notification, &app::notify::Notification::finished, context,
        [context, &calledDuringDestruction] { calledDuringDestruction = context->destroying; });

    delete context;

    QVERIFY(!calledDuringDestruction);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(manager.count(), 0);
}

void tst_NotificationManager::cancelsAllRequestsOnShutdown()
{
    app::notify::NotificationManager manager;
    QObject context;
    auto *first = manager.post({ .title = {}, .message = u"first"_s, .actions = {} }, &context);
    auto *second = manager.post({ .title = {}, .message = u"second"_s, .actions = {} }, &context);
    QSignalSpy firstFinished(first, &app::notify::Notification::finished);
    QSignalSpy secondFinished(second, &app::notify::Notification::finished);

    manager.shutdown();
    manager.shutdown();

    QCOMPARE(firstFinished.size(), 1);
    QCOMPARE(secondFinished.size(), 1);
    QCOMPARE(manager.rowCount(), 0);
    QVERIFY(manager.post({ .title = {}, .message = u"late"_s, .actions = {} }, &context)
        == nullptr);
}

void tst_NotificationManager::passiveTimeoutStartsWhenPresented()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    options.message = u"timed"_s;
    options.timeoutMs = 40;
    auto *notification = manager.post(options, &context);
    QVERIFY(notification);
    QSignalSpy finished(notification, &app::notify::Notification::finished);

    bool elapsed = false;
    QTimer::singleShot(70, &manager, [&elapsed] { elapsed = true; });
    QTRY_VERIFY_WITH_TIMEOUT(elapsed, 1000);
    QCOMPARE(finished.size(), 0);

    notification->setPresentationState(true, false);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 1000);
    QCOMPARE(finished.first().at(0).value<app::notify::Result>(), app::notify::Result::Expired);
}

void tst_NotificationManager::reportsIndependentTimerProgress()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    options.timeoutMs = 1000;
    auto *first = manager.post(options, &context);
    auto *second = manager.post(options, &context);
    first->setPresentationState(true, false);
    QTRY_VERIFY_WITH_TIMEOUT(first->timeRemaining() < 0.95, 500);
    QCOMPARE(second->timeRemaining(), 1.0);
    first->setPresentationState(true, true);
    const auto paused = first->timeRemaining();
    QTRY_VERIFY_WITH_TIMEOUT(first->timeRemaining() == paused, 500);
    second->setPresentationState(true, false);
    QTRY_VERIFY_WITH_TIMEOUT(second->timeRemaining() < 0.95, 500);
    QCOMPARE(first->timeRemaining(), paused);
}

void tst_NotificationManager::prioritizesWaitingCardsWithoutMovingVisibleCards()
{
    app::notify::NotificationManager manager;
    QAbstractItemModelTester modelTester(&manager,
        QAbstractItemModelTester::FailureReportingMode::QtTest);
    QObject context;
    app::notify::Options options;
    options.presentation = app::notify::Presentation::Persistent;
    auto *visible = manager.post(options, &context);
    auto *waiting = manager.post(options, &context);
    options.severity = app::notify::Severity::Error;
    auto *urgent = manager.post(options, &context);

    QCOMPARE(manager.data(manager.index(0), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        urgent);
    QCOMPARE(manager.data(manager.index(1), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        waiting);
    QCOMPARE(manager.data(manager.index(2), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        visible);
}

void tst_NotificationManager::serializesDecisionRequests()
{
    app::notify::NotificationManager manager;
    QAbstractItemModelTester modelTester(&manager,
        QAbstractItemModelTester::FailureReportingMode::QtTest);
    QObject context;
    app::notify::Options options;
    options.presentation = app::notify::Presentation::Decision;
    auto *first = manager.post(options, &context);
    auto *second = manager.post(options, &context);
    auto *card = manager.post({ .title = {}, .message = u"card"_s, .actions = {} }, &context);

    QCOMPARE(manager.currentDecision(), first);
    QCOMPARE(manager.rowCount(), 1);
    QSignalSpy decisionChanges(&manager, &app::notify::NotificationManager::currentDecisionChanged);

    first->dismiss();

    QCOMPARE(decisionChanges.size(), 1);
    QCOMPARE(manager.currentDecision(), second);
    QCOMPARE(manager.rowCount(), 1);
    QCOMPARE(manager.data(manager.index(0), app::notify::NotificationManager::NotificationRole)
                 .value<app::notify::Notification *>(),
        card);
}

void tst_NotificationManager::handlesCompletionDuringPresentationChange()
{
    using Notification = app::notify::Notification;
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    options.presentation = app::notify::Presentation::Persistent;
    auto *card = manager.post(options, &context);
    QVERIFY(card);
    QSignalSpy cardPresentation(card, &Notification::presentationChanged);
    auto cardConnection = connect(&manager, &app::notify::NotificationManager::countChanged,
        &context, [&manager, card] {
            if (manager.count() == 0)
                card->cancel();
        });
    card->setPresentation(app::notify::Presentation::Decision);
    QVERIFY(card->isFinished());
    QCOMPARE(manager.currentDecision(), nullptr);
    QCOMPARE(manager.count(), 0);
    QCOMPARE(cardPresentation.size(), 0);
    disconnect(cardConnection);

    options.presentation = app::notify::Presentation::Decision;
    auto *decision = manager.post(options, &context);
    QVERIFY(decision);
    QSignalSpy decisionPresentation(decision, &Notification::presentationChanged);
    connect(&manager, &app::notify::NotificationManager::currentDecisionChanged, &context,
        [&manager, decision] {
            if (!manager.currentDecision())
                decision->cancel();
        });
    decision->setPresentation(app::notify::Presentation::Persistent);
    QVERIFY(decision->isFinished());
    QCOMPARE(manager.currentDecision(), nullptr);
    QCOMPARE(manager.count(), 0);
    QCOMPARE(decisionPresentation.size(), 0);
}

void tst_NotificationManager::modalCompletionResumesCardTimeout()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    options.timeoutMs = 100;
    auto *card = manager.post(options, &context);
    card->setPresentationState(true, false);
    QSignalSpy cardFinished(card, &app::notify::Notification::finished);

    options.presentation = app::notify::Presentation::Decision;
    auto *decision = manager.post(options, &context);
    bool pausedIntervalElapsed = false;
    QTimer::singleShot(150, &manager, [&pausedIntervalElapsed] { pausedIntervalElapsed = true; });
    QTRY_VERIFY_WITH_TIMEOUT(pausedIntervalElapsed, 1000);
    QCOMPARE(cardFinished.size(), 0);

    decision->dismiss();

    QTRY_COMPARE_WITH_TIMEOUT(cardFinished.size(), 1, 1000);
    QCOMPARE(cardFinished.first().at(0).value<app::notify::Result>(), app::notify::Result::Expired);
}

void tst_NotificationManager::presentedOverflowCardsCanExpire()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    options.timeoutMs = 30;
    auto *first = manager.post(options, &context);
    auto *overflow = manager.post(options, &context);
    overflow->setPresentationState(true, false);
    QSignalSpy overflowFinished(overflow, &app::notify::Notification::finished);

    QTRY_COMPARE_WITH_TIMEOUT(overflowFinished.size(), 1, 1000);
    QVERIFY(first->isFinished() == false);
}

void tst_NotificationManager::ignoresUnknownAndDisabledActions()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    options.presentation = app::notify::Presentation::Persistent;
    options.actions = {
        { u"disabled"_s, u"Disabled"_s, false },
        { u"save"_s,     u"Save"_s,     true  },
    };
    auto *notification = manager.post(options, &context);
    QVERIFY(notification);
    QSignalSpy finished(notification, &app::notify::Notification::finished);

    notification->choose(u"missing"_s);
    notification->choose(u"disabled"_s);
    QCOMPARE(finished.size(), 0);

    notification->choose(u"save"_s);

    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.first().at(0).value<app::notify::Result>(), app::notify::Result::Action);
    QCOMPARE(finished.first().at(1).toString(), u"save"_s);
}

void tst_NotificationManager::resumesOverdueCardsWithoutSynchronousCompletion()
{
    app::notify::NotificationManager manager;
    QObject context;
    app::notify::Options options;
    options.timeoutMs = 1;
    auto *first = manager.post(options, &context);
    auto *second = manager.post(options, &context);
    first->setPresentationState(true, false);
    second->setPresentationState(true, false);
    QElapsedTimer undeliveredTimeout;
    undeliveredTimeout.start();
    while (undeliveredTimeout.elapsed() < 5)
        QThread::yieldCurrentThread();
    first->setPresentationState(true, true);
    second->setPresentationState(true, true);
    manager.setHostActive(false);
    first->setPresentationState(true, false);
    second->setPresentationState(true, false);
    QSignalSpy firstFinished(first, &app::notify::Notification::finished);
    QSignalSpy secondFinished(second, &app::notify::Notification::finished);

    manager.setHostActive(true);

    QCOMPARE(firstFinished.size(), 0);
    QCOMPARE(secondFinished.size(), 0);
    QTRY_COMPARE(firstFinished.size(), 1);
    QTRY_COMPARE(secondFinished.size(), 1);
    QCOMPARE(manager.count(), 0);
}

void tst_NotificationManager::autoIdsActionsAndFinishesOnce()
{
    app::notify::NotificationManager manager;
    QObject context;
    int triggeredCount = 0;
    app::notify::Options options;
    options.presentation = app::notify::Presentation::Persistent;
    options.actions = {
        { .id = {}, .text = u"Retry"_s },
        { .id = {}, .text = u"Again"_s },
    };
    auto *notification = manager.post(options, &context);
    QVERIFY(notification);
    connect(notification, &app::notify::Notification::finished, &context, [&triggeredCount] {
        ++triggeredCount;
    });
    const auto actions = notification->actions();
    QVERIFY(!actions.at(0).id.isEmpty());
    QVERIFY(!actions.at(1).id.isEmpty());
    QVERIFY(actions.at(0).id != actions.at(1).id);

    notification->choose(actions.at(0).id);
    notification->choose(actions.at(0).id);

    QCOMPARE(triggeredCount, 1);

    auto *updated = manager.post({}, &context);
    QVERIFY(updated);
    updated->setActions(options.actions);
    const auto updatedActions = updated->actions();
    QVERIFY(!updatedActions.at(0).id.isEmpty());
    QVERIFY(!updatedActions.at(1).id.isEmpty());
    QVERIFY(updatedActions.at(0).id != updatedActions.at(1).id);
}

void tst_NotificationManager::disconnectsReceiverWhenContextDiesDuringFinish()
{
    app::notify::NotificationManager manager;
    auto *context = new QObject;
    int triggeredCount = 0;
    app::notify::Options options;
    options.presentation = app::notify::Presentation::Persistent;
    options.actions = {
        { .id = {}, .text = u"Retry"_s },
    };
    auto *notification = manager.post(options, context);
    QVERIFY(notification);
    const QString actionId = notification->actions().first().id;
    QObject observer;
    connect(notification, &app::notify::Notification::finished, &observer, [&context] {
        delete context;
        context = nullptr;
    });

    connect(notification, &app::notify::Notification::finished, context, [&triggeredCount] {
        ++triggeredCount;
    });
    notification->choose(actionId);

    QCOMPARE(triggeredCount, 0);
    QVERIFY(context == nullptr);
}

void tst_NotificationManager::callsConnectedReceiverWithLiveContext()
{
    app::notify::NotificationManager manager;
    QObject context;
    int calls = 0;
    app::notify::Options options;
    auto *message = manager.post(options, &context);
    QVERIFY(message);
    connect(message, &app::notify::Notification::finished, &context, [&calls] { ++calls; });
    message->dismiss();
    message->dismiss();
    QCOMPARE(calls, 1);

    auto *dyingContext = new QObject;
    message = manager.post(options, dyingContext);
    QVERIFY(message);
    QObject observer;
    connect(message, &app::notify::Notification::finished, &observer, [&dyingContext] {
        delete dyingContext;
        dyingContext = nullptr;
    });
    connect(message, &app::notify::Notification::finished, dyingContext, [&calls] { ++calls; });
    message->dismiss();
    QCOMPARE(calls, 1);
}

QTEST_MAIN(tst_NotificationManager)

#include "tst_NotificationManager.moc"
