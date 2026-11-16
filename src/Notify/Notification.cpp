#include "Notification.hpp"

#include "NotificationManager.hpp"

#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <algorithm>
#include <utility>

namespace App::Notify {

QString Notification::severitySymbol() const
{
    switch (m_severity) {
    case Severity::Success:
        return QStringLiteral("✓");
    case Severity::Warning:
        return QStringLiteral("!");
    case Severity::Error:
        return QStringLiteral("×");
    case Severity::Information:
        return QStringLiteral("i");
    }
    return QStringLiteral("i");
}

Notification::Notification(NotificationManager *manager, quint64 id, const Options &options,
    QObject *context)
    : QObject(manager)
    , m_manager(manager)
    , m_id(id)
    , m_title(options.title)
    , m_message(options.message)
    , m_severity(options.severity)
    , m_presentation(options.presentation)
    , m_actions(options.actions)
    , m_context(context)
    , m_timer(new QTimer(this))
    , m_remainingMs(options.timeoutMs)
    , m_timeoutMs(options.timeoutMs)
{
    assignActionIds(m_actions);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this] { m_manager->finish(this, Result::Expired); });
    connect(context, &QObject::destroyed, m_manager,
        [manager = m_manager, notification = QPointer<Notification>(this)] {
            QTimer::singleShot(0, manager, [notification] {
                if (notification)
                    notification->cancel();
            });
        });
}

void Notification::setTitle(const QString &title)
{
    if (m_title == title || m_finished)
        return;
    m_title = title;
    Q_EMIT titleChanged();
}

void Notification::setMessage(const QString &message)
{
    if (m_message == message || m_finished)
        return;
    m_message = message;
    Q_EMIT messageChanged();
}

void Notification::setActions(const QList<Action> &actions)
{
    if (m_finished)
        return;
    m_actions = actions;
    assignActionIds(m_actions);
    Q_EMIT actionsChanged();
}

void Notification::setPresentation(Presentation presentation)
{
    if (m_presentation == presentation || m_finished)
        return;
    const auto previous = m_presentation;
    m_presentation = presentation;
    QPointer<Notification> guardedNotification(this);
    m_manager->presentationChanged(this, previous);
    auto *currentNotification = guardedNotification.data();
    if (!currentNotification || currentNotification->isFinished())
        return;
    updateTimerEligibility();
    Q_EMIT presentationChanged();
}

double Notification::timeRemaining() const
{
    if (m_timeoutMs <= 0 || m_presentation != Presentation::Passive)
        return -1.0;
    const auto remaining = m_remainingMs - (m_timer->isActive() ? m_elapsed.elapsed() : 0);
    return qBound(0.0, static_cast<double>(remaining) / m_timeoutMs, 1.0);
}

void Notification::choose(const QString &actionId)
{
    if (m_finished || !m_context)
        return;
    const auto action = std::ranges::find_if(std::as_const(m_actions),
        [&actionId](const Action &item) { return item.id == actionId && item.enabled; });
    if (action == m_actions.cend())
        return;

    const auto selectedId = action->id;
    m_manager->finish(this, Result::Action, selectedId);
}

void Notification::dismiss()
{
    if (!m_finished && m_context)
        m_manager->finish(this, Result::Dismissed);
}

void Notification::cancel()
{
    if (!m_finished)
        m_manager->finish(this, Result::Cancelled);
}

void Notification::setPresentationState(bool presented, bool paused)
{
    if (m_presented == presented && m_paused == paused)
        return;
    m_presented = presented;
    m_paused = paused;
    updateTimerEligibility();
}

void Notification::setManagerActive(bool active)
{
    if (m_managerActive == active)
        return;
    m_managerActive = active;
    updateTimerEligibility();
}

void Notification::updateTimerEligibility()
{
    const bool shouldRun = !m_finished && m_presentation == Presentation::Passive && m_presented
        && !m_paused && m_managerActive && m_timeoutMs > 0;
    if (shouldRun == m_timer->isActive())
        return;
    if (shouldRun) {
        m_elapsed.start();
        m_timer->start(m_remainingMs);
    } else if (m_timer->isActive()) {
        m_timer->stop();
        m_remainingMs = qMax(0, m_remainingMs - static_cast<int>(m_elapsed.elapsed()));
    }
}

void Notification::assignActionIds(QList<Action> &actions) const
{
    QSet<QString> used;
    for (const auto &action : actions)
        if (!action.id.isEmpty())
            used.insert(action.id);

    for (qsizetype i = 0; i < actions.size(); ++i) {
        auto &action = actions[i];
        if (!action.id.isEmpty())
            continue;
        const QString baseId = QStringLiteral("notification-%1-action-%2")
                                   .arg(m_id)
                                   .arg(static_cast<qulonglong>(i));
        QString id = baseId;
        quint64 suffix = 1;
        while (used.contains(id))
            id = baseId + QLatin1Char('-') + QString::number(suffix++);
        action.id = id;
        used.insert(id);
    }
}

} // namespace App::Notify
