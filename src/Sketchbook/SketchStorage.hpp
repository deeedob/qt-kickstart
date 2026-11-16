#pragma once

#include <Canvas/CanvasTypes.hpp>

#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QtTypes>

#include <string>
#include <vector>

namespace App::Sketchbook {

class SketchStorageWorker;

struct SketchDocument
{
    std::string name;
    std::vector<Canvas::StrokePtr> strokes;
    Canvas::Page page;
};

struct SketchEntry
{
    QString id;
    QString name;
    QDateTime modified;
    qsizetype strokeCount = 0;
};

class SketchStorage : public QObject
{
    Q_OBJECT

public:
    enum class Mutation { Save, Rename, Duplicate };
    Q_ENUM(Mutation)

    explicit SketchStorage(QString directory, QObject *parent = nullptr);
    ~SketchStorage() override;

    void list();
    void load(const QString &id, quint64 requestId);
    void save(const QString &id, SketchDocument document);
    void rename(const QString &id, QString name);
    void duplicate(const QString &sourceId, QString id, QString name);
    void remove(const QString &id);
    void retryFailedSaves();
    [[nodiscard]] bool hasUnsavedChanges() const { return !m_dirty.isEmpty(); }

Q_SIGNALS:
    void listed(std::vector<App::Sketchbook::SketchEntry> entries);
    void loaded(QString id, quint64 requestId, App::Sketchbook::SketchDocument document,
        QString error);
    void mutationFinished(QString id, QString sourceId,
        App::Sketchbook::SketchStorage::Mutation mutation, QString error, bool retryable);
    void removed(QString id, QString error);
    void unsavedChangesChanged();

private Q_SLOTS:
    void onMutationFinished(const QString &id, const QString &sourceId, Mutation mutation,
        quint64 revision, const QString &error, bool retryable);
    void onRemoved(const QString &id, const QString &error);

private:
    friend class SketchStorageWorker;

    [[nodiscard]] quint64 markDirty(const QString &id);

    QThread m_thread;
    SketchStorageWorker *m_worker = nullptr;
    QHash<QString, quint64> m_dirty;
    quint64 m_nextRevision = 0;
};

} // namespace App::Sketchbook

Q_DECLARE_METATYPE(App::Sketchbook::SketchDocument)
Q_DECLARE_METATYPE(App::Sketchbook::SketchEntry)
Q_DECLARE_METATYPE(std::vector<App::Sketchbook::SketchEntry>)
Q_DECLARE_METATYPE(App::Sketchbook::SketchStorage::Mutation)
