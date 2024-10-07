// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QString>
#include <QByteArray>
#include <QLoggingCategory>
#include <QVector>
#include <QColor>
#include <QQmlEngine>
#include <QFuture>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QAtomicInt>

#include <QCoro/QCoroTask>

#include "utility/q3cache.h"

Q_DECLARE_LOGGING_CATEGORY(LogLDraw)

class Transfer;
class TransferJob;
class MiniZip;


namespace LDraw {

class Part;
class PartElement;
class PartLoaderJob;

enum class UpdateStatus  { Ok, Loading, Updating, UpdateFailed };

class Library : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
    Q_PROPERTY(LDraw::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)

public:
    ~Library() override;

    QString path() const;
    QCoro::Task<bool> setPath(const QString &path, bool forceReload = false);

    bool isValid() const               { return m_valid; }
    QDateTime lastUpdated() const      { return m_lastUpdated; }
    UpdateStatus updateStatus() const  { return m_updateStatus; }

    bool startUpdate();
    bool startUpdate(bool force);
    void cancelUpdate();

    QFuture<Part *> partFromId(const QByteArray &id);
    QFuture<Part *> partFromBrickLinkId(const QByteArray &brickLinkId);
    QFuture<Part *> partFromFile(const QString &filename);

    static QStringList potentialLDrawDirs();
    static bool checkLDrawDir(const QString &dir);

    QPair<int, int> partCacheStats() const;

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);
    void updateStatusChanged(LDraw::UpdateStatus updateStatus);
    void lastUpdatedChanged(const QDateTime &lastUpdated);
    void validChanged(bool valid);
    void libraryAboutToBeReset();
    void libraryReset();

private:
    Library(const QString &updateUrl, QObject *parent = nullptr);

    static inline Library *inst() { return s_inst; }
    static Library *create(const QString &updateUrl);
    static Library *s_inst;
    friend Library *library();
    friend Library *create(const QString &);

    void partLoaderThread();
    Part *findPart(const QString &_filename, const QString &_parentdir);
    QByteArray readLDrawFile(const QString &filename);
    void setUpdateStatus(UpdateStatus updateStatus);
    void emitUpdateStartedIfNecessary();

    void startPartLoaderThread();
    void shutdownPartLoaderThread();

    QString m_updateUrl;
    bool m_valid = false;
    UpdateStatus m_updateStatus = UpdateStatus::UpdateFailed;
    int m_updateInterval = 0;
    QDateTime m_lastUpdated;
    QString m_etag;
    Transfer *m_transfer;
    TransferJob *m_job = nullptr;

    QString m_path;
    bool m_isZip = false;
    bool m_locked = false; // during updates/loading
    std::unique_ptr<MiniZip> m_zip;
    QStringList m_searchpath;
    QHash<QString, QString> m_partIdMapping;
    Q3Cache<QString, Part> m_cache;  // path -> part
    // (filename, parentdir) -> (resolved filename, resolved parentdir, inZip)
    QHash<std::pair<QString, QString>, std::tuple<QString, QString, bool>> m_lookupCache;

    QVector<PartLoaderJob *> m_partLoaderJobs;
    QMutex m_partLoaderMutex;
    QWaitCondition m_partLoaderCondition;
    std::unique_ptr<QThread> m_partLoaderThread;
    QAtomicInt m_partLoaderShutdown = 0;
    QAtomicInt m_partLoaderClear = 0;
    int m_lookupStatId = -1;
    int m_partsStatId = -1;

    friend class PartElement;
};

inline Library *library() { return Library::inst(); }
inline Library *create(const QString &updateUrl) { return Library::create(updateUrl); }

} // namespace LDraw
