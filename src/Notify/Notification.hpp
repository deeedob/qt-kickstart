#pragma once

#include "NotifyTypes.hpp"

#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtQmlIntegration/qqmlintegration.h>


QT_FORWARD_DECLARE_CLASS(QTimer)

namespace App::Notify {

class NotificationManager;

class Notification final : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Notification)
    QML_UNCREATABLE("Created and owned by NotificationManager")
    Q_PROPERTY(quint64 id READ id CONSTANT FINAL)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged FINAL)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged FINAL)
    Q_PROPERTY(App::Notify::Severity severity READ severity CONSTANT FINAL)
    Q_PROPERTY(QString severitySymbol READ severitySymbol CONSTANT FINAL)
    Q_PROPERTY(
        App::Notify::Presentation presentation READ presentation NOTIFY presentationChanged FINAL)
    Q_PROPERTY(QList<App::Notify::Action> actions READ actions WRITE setActions NOTIFY
            actionsChanged FINAL)
    Q_PROPERTY(bool isFinished READ isFinished NOTIFY isFinishedChanged FINAL)

public:
    [[nodiscard]] quint64 id() const { return m_id; }
    [[nodiscard]] QString title() const { return m_title; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] Severity severity() const { return m_severity; }
    [[nodiscard]] QString severitySymbol() const;
    [[nodiscard]] Presentation presentation() const { return m_presentation; }
    [[nodiscard]] QList<Action> actions() const { return m_actions; }
    [[nodiscard]] bool isFinished() const { return m_finished; }

    void setTitle(const QString &title);
    void setMessage(const QString &message);
    void setActions(const QList<Action> &actions);
    Q_INVOKABLE void setPresentation(App::Notify::Presentation presentation);

    Q_INVOKABLE [[nodiscard]] double timeRemaining() const;
    Q_INVOKABLE void choose(const QString &actionId);
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void setPresentationState(bool presented, bool paused);

Q_SIGNALS:
    void titleChanged();
    void messageChanged();
    void presentationChanged();
    void actionsChanged();
    void isFinishedChanged();
    void finished(App::Notify::Result reason, const QString &actionId);

private:
    friend class NotificationManager;

    Notification(NotificationManager *manager, quint64 id, const Options &options,
        QObject *context);
    void updateTimerEligibility();
    void setManagerActive(bool active);
    void assignActionIds(QList<Action> &actions) const;

    NotificationManager *m_manager;
    quint64 m_id;
    QString m_title;
    QString m_message;
    Severity m_severity;
    Presentation m_presentation;
    QList<Action> m_actions;
    QPointer<QObject> m_context;
    QTimer *m_timer;
    QElapsedTimer m_elapsed;
    int m_remainingMs;
    int m_timeoutMs;
    bool m_presented = false;
    bool m_paused = false;
    bool m_managerActive = false;
    bool m_finished = false;
};


} // namespace App::Notify

Q_DECLARE_METATYPE(App::Notify::Notification *)
