#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace App::Notify {
Q_NAMESPACE
QML_NAMED_ELEMENT(Notify)

enum class Severity { Information, Success, Warning, Error };
Q_ENUM_NS(Severity)

enum class Presentation { Passive, Persistent, Decision };
Q_ENUM_NS(Presentation)

enum class Result { Action, Dismissed, Expired, Cancelled };
Q_ENUM_NS(Result)

class Action
{
    Q_GADGET
    QML_VALUE_TYPE(action)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(QString id MEMBER id FINAL)
    Q_PROPERTY(QString text MEMBER text FINAL)
    Q_PROPERTY(bool enabled MEMBER enabled FINAL)

public:
    QString id;
    QString text;
    bool enabled = true;
    friend bool operator==(const Action &left, const Action &right)
    {
        return left.id == right.id && left.text == right.text && left.enabled == right.enabled;
    }
};

struct Options
{
    Q_GADGET
    QML_VALUE_TYPE(options)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(QString title MEMBER title FINAL)
    Q_PROPERTY(QString message MEMBER message FINAL)
    Q_PROPERTY(App::Notify::Severity severity MEMBER severity FINAL)
    Q_PROPERTY(App::Notify::Presentation presentation MEMBER presentation FINAL)
    Q_PROPERTY(QList<App::Notify::Action> actions MEMBER actions FINAL)
    Q_PROPERTY(int timeoutMs MEMBER timeoutMs FINAL)

public:
    QString title;
    QString message;
    Severity severity = Severity::Information;
    Presentation presentation = Presentation::Passive;
    QList<Action> actions;
    int timeoutMs = 5000;
};

} // namespace App::Notify
