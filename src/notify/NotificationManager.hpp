#pragma once

#include "Notification.hpp"

#include <QtCore/QAbstractListModel>
#include <QtCore/QPointer>
#include <QtQml/QJSValue>
#include <QtQmlIntegration/qqmlintegration.h>

#include <functional>

#include <vector>

namespace app::notify {

class NotificationManager final : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NotificationManager)
    QML_UNCREATABLE("Created in main() and passed to the application window")
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(app::notify::Notification *currentDecision READ currentDecision NOTIFY
            currentDecisionChanged FINAL)
    Q_PROPERTY(bool hostActive READ hostActive WRITE setHostActive NOTIFY hostActiveChanged FINAL)

public:
    enum Role { NotificationRole = Qt::UserRole + 1 };
    Q_ENUM(Role)

    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager() override;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const;
    [[nodiscard]] Notification *currentDecision() const;
    [[nodiscard]] bool hostActive() const { return m_hostActive; }

    Q_INVOKABLE app::notify::Notification *post(const app::notify::Options &options,
        QObject *context);
    Notification *post(const Options &options, QObject *context,
        std::function<void(Result, const QString &)> onFinished);
    Q_INVOKABLE app::notify::Notification *post(const app::notify::Options &options,
        QObject *context, const QJSValue &onFinished);
    Q_INVOKABLE app::notify::Notification *postMessage(const QString &message);
    void setHostActive(bool active);
    Q_INVOKABLE void dismissPassive();
    Q_INVOKABLE void shutdown();

Q_SIGNALS:
    void countChanged();
    void currentDecisionChanged();
    void hostActiveChanged();

private:
    friend class Notification;

    void finish(Notification *notification, Result reason, const QString &actionId = {});
    void presentationChanged(Notification *notification, Presentation previous);
    void updateTimerEligibility();
    void promoteDecision();
    void insertCard(Notification *notification);
    void removeNotification(Notification *notification);
    [[nodiscard]] int rowOf(const Notification *notification) const;

    std::vector<Notification *> m_cards;
    std::vector<Notification *> m_decisions;
    QPointer<Notification> m_currentDecision;
    quint64 m_nextId = 0;
    bool m_hostActive = true;
    bool m_shutdown = false;
};

} // namespace app::notify
