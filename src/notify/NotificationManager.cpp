#include "NotificationManager.hpp"

#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtQml/QJSEngine>
#include <QtQml/QJSManagedValue>

#include <algorithm>
#include <utility>

namespace app::notify {

Q_STATIC_LOGGING_CATEGORY(lcNotify, "app.notify")

NotificationManager::NotificationManager(QObject *parent)
    : QAbstractListModel(parent)
{
}

NotificationManager::~NotificationManager()
{
    shutdown();
}

int NotificationManager::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_cards.size());
}

QVariant NotificationManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    if (role != NotificationRole)
        return {};
    return QVariant::fromValue(m_cards[static_cast<std::size_t>(index.row())]);
}

QHash<int, QByteArray> NotificationManager::roleNames() const
{
    return {
        { NotificationRole, QByteArrayLiteral("notification") }
    };
}

int NotificationManager::count() const
{
    return rowCount();
}

Notification *NotificationManager::currentDecision() const
{
    return m_currentDecision;
}

Notification *NotificationManager::post(const Options &options, QObject *context)
{
    return post(options, context, std::function<void(Result, const QString &)>{});
}

Notification *NotificationManager::post(const Options &options, QObject *context,
    const QJSValue &onFinished)
{
    if (onFinished.isUndefined() || onFinished.isNull())
        return post(options, context);
    if (!onFinished.isCallable()) {
        qCWarning(lcNotify, "Notification completion callback must be callable");
        return nullptr;
    }
    if (!context)
        return nullptr;
    const QPointer<QJSEngine> engine(qjsEngine(context));
    if (!engine) {
        qCWarning(lcNotify, "Notification callback context must belong to a JavaScript engine");
        return nullptr;
    }
    return post(options, context, [onFinished, engine](Result reason, const QString &actionId) {
        if (!engine)
            return;
        const QJSManagedValue callback(onFinished, engine);
        callback.call({ QJSValue(static_cast<int>(reason)), QJSValue(actionId) });
        if (engine && engine->hasError())
            qCWarning(lcNotify).noquote()
                << "Notification completion callback failed:" << engine->catchError().toString();
    });
}

Notification *NotificationManager::post(const Options &options, QObject *context,
    std::function<void(Result, const QString &)> onFinished)
{
    if (m_shutdown || !context || QThread::currentThread() != thread()
        || context->thread() != thread())
        return nullptr;

    auto *notification = new Notification(this, ++m_nextId, options, context);
    const QPointer<Notification> guardedNotification(notification);
    QJSEngine::setObjectOwnership(notification, QJSEngine::CppOwnership);
    if (onFinished)
        connect(notification, &Notification::finished, context, std::move(onFinished));
    if (options.presentation == Presentation::Decision) {
        if (!m_currentDecision) {
            m_currentDecision = notification;
            Q_EMIT currentDecisionChanged();
        } else {
            m_decisions.push_back(notification);
        }
        updateTimerEligibility();
    } else {
        insertCard(notification);
    }
    auto *current = guardedNotification.data();
    return current && !current->isFinished() ? current : nullptr;
}

Notification *NotificationManager::postMessage(const QString &message)
{
    Options options;
    options.message = message;
    return post(options, this);
}

void NotificationManager::setHostActive(bool active)
{
    if (m_hostActive == active)
        return;
    m_hostActive = active;
    Q_EMIT hostActiveChanged();
    updateTimerEligibility();
}

void NotificationManager::dismissPassive()
{
    std::vector<QPointer<Notification>> passive;
    for (auto *message : m_cards)
        if (message->presentation() == Presentation::Passive)
            passive.emplace_back(message);
    for (const auto &message : passive) {
        auto *current = message.data();
        if (current && current->presentation() == Presentation::Passive)
            current->dismiss();
    }
}

void NotificationManager::shutdown()
{
    if (m_shutdown)
        return;
    m_shutdown = true;
    std::vector<QPointer<Notification>> requests;
    requests.reserve(m_cards.size() + m_decisions.size() + (m_currentDecision ? 1U : 0U));
    for (auto *notification : m_cards)
        requests.emplace_back(notification);
    for (auto *notification : m_decisions)
        requests.emplace_back(notification);
    if (m_currentDecision)
        requests.emplace_back(m_currentDecision);
    for (const auto &notification : requests)
        if (notification)
            finish(notification, Result::Cancelled);
}

void NotificationManager::finish(Notification *notification, Result reason, const QString &actionId)
{
    if (!notification || notification->m_finished)
        return;

    notification->m_finished = true;
    notification->m_actions.clear();
    notification->m_timer->stop();
    QPointer<Notification> guardedNotification(notification);
    removeNotification(notification);
    if (!guardedNotification)
        return;
    Q_EMIT guardedNotification->actionsChanged();
    if (!guardedNotification)
        return;
    Q_EMIT guardedNotification->isFinishedChanged();
    if (!guardedNotification)
        return;
    Q_EMIT guardedNotification->finished(reason, actionId);
    if (guardedNotification)
        guardedNotification->deleteLater();
}

void NotificationManager::presentationChanged(Notification *notification, Presentation previous)
{
    const QPointer<Notification> guardedNotification(notification);
    const auto current = notification->presentation();
    if (previous != Presentation::Decision && current != Presentation::Decision) {
        updateTimerEligibility();
        return;
    }

    if (previous == Presentation::Decision) {
        if (m_currentDecision == notification) {
            m_currentDecision = nullptr;
            promoteDecision();
        } else {
            std::erase(m_decisions, notification);
        }
        auto *currentNotification = guardedNotification.data();
        if (!currentNotification || currentNotification->isFinished())
            return;
        insertCard(notification);
    } else if (notification->presentation() == Presentation::Decision) {
        const int row = rowOf(notification);
        if (row >= 0) {
            beginRemoveRows({}, row, row);
            m_cards.erase(m_cards.begin() + row);
            endRemoveRows();
            Q_EMIT countChanged();
        }
        auto *currentNotification = guardedNotification.data();
        if (!currentNotification || currentNotification->isFinished()) {
            updateTimerEligibility();
            return;
        }
        if (!m_currentDecision) {
            m_currentDecision = notification;
            Q_EMIT currentDecisionChanged();
        } else {
            m_decisions.push_back(notification);
        }
        updateTimerEligibility();
    }
}

void NotificationManager::updateTimerEligibility()
{
    const bool active = m_hostActive && !m_currentDecision;
    for (auto *notification : m_cards)
        notification->setManagerActive(active);
}

void NotificationManager::promoteDecision()
{
    if (m_currentDecision)
        return;
    if (!m_shutdown && !m_decisions.empty()) {
        m_currentDecision = m_decisions.front();
        m_decisions.erase(m_decisions.begin());
    }
    Q_EMIT currentDecisionChanged();
    updateTimerEligibility();
}

void NotificationManager::insertCard(Notification *notification)
{
    const auto severity = notification->severity();
    const auto row = std::ranges::find_if(m_cards.begin(), m_cards.end(),
        [severity](const Notification *existing) {
            return static_cast<int>(existing->severity()) <= static_cast<int>(severity);
        });
    const int position = static_cast<int>(std::distance(m_cards.begin(), row));
    beginInsertRows({}, position, position);
    m_cards.insert(row, notification);
    endInsertRows();
    Q_EMIT countChanged();
    updateTimerEligibility();
}

void NotificationManager::removeNotification(Notification *notification)
{
    const int row = rowOf(notification);
    if (row >= 0) {
        beginRemoveRows({}, row, row);
        m_cards.erase(m_cards.begin() + row);
        endRemoveRows();
        Q_EMIT countChanged();
        updateTimerEligibility();
        return;
    }

    if (m_currentDecision == notification) {
        m_currentDecision = nullptr;
        promoteDecision();
        return;
    }
    std::erase(m_decisions, notification);
}

int NotificationManager::rowOf(const Notification *notification) const
{
    const auto found = std::ranges::find(m_cards, notification);
    return found == m_cards.end() ? -1 : static_cast<int>(std::distance(m_cards.begin(), found));
}

} // namespace app::notify
