#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace app::notify {
Q_NAMESPACE

enum class Severity { Information, Success, Warning, Error };
Q_ENUM_NS(Severity)

enum class Presentation { Passive, Persistent, Decision };
Q_ENUM_NS(Presentation)

enum class FinishReason { Action, Dismissed, Expired, Cancelled };
Q_ENUM_NS(FinishReason)

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
    Q_PROPERTY(app::notify::Severity severity MEMBER severity FINAL)
    Q_PROPERTY(app::notify::Presentation presentation MEMBER presentation FINAL)
    Q_PROPERTY(QList<app::notify::Action> actions MEMBER actions FINAL)
    Q_PROPERTY(int timeoutMs MEMBER timeoutMs FINAL)

public:
    QString title;
    QString message;
    Severity severity = Severity::Information;
    Presentation presentation = Presentation::Passive;
    QList<Action> actions;
    int timeoutMs = 5000;
};

} // namespace app::notify

namespace app::notify::qml::severity {
Q_NAMESPACE
QML_NAMED_ELEMENT(Severity)

enum class Value {
    Information = static_cast<int>(app::notify::Severity::Information),
    Success = static_cast<int>(app::notify::Severity::Success),
    Warning = static_cast<int>(app::notify::Severity::Warning),
    Error = static_cast<int>(app::notify::Severity::Error),
};
Q_ENUM_NS(Value)
}

namespace app::notify::qml::presentation {
Q_NAMESPACE
QML_NAMED_ELEMENT(Presentation)

enum class Value {
    Passive = static_cast<int>(app::notify::Presentation::Passive),
    Persistent = static_cast<int>(app::notify::Presentation::Persistent),
    Decision = static_cast<int>(app::notify::Presentation::Decision),
};
Q_ENUM_NS(Value)
}

namespace app::notify::qml::finishReason {
Q_NAMESPACE
QML_NAMED_ELEMENT(FinishReason)

enum class Value {
    Action = static_cast<int>(app::notify::FinishReason::Action),
    Dismissed = static_cast<int>(app::notify::FinishReason::Dismissed),
    Expired = static_cast<int>(app::notify::FinishReason::Expired),
    Cancelled = static_cast<int>(app::notify::FinishReason::Cancelled),
};
Q_ENUM_NS(Value)
}
