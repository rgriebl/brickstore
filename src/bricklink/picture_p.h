// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QAtomicInt>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QVector>
#include <QtSql/QSqlDatabase>

#include "utility/q3cache.h"
#include "global.h"

QT_FORWARD_DECLARE_CLASS(QThread)

class TransferJob;


namespace BrickLink {

class PictureCachePrivate
{
public:
    QAtomicInt m_stop = false;
    QMutex m_loadMutex;
    QMutex m_saveMutex;
    QWaitCondition m_loadTrigger;
    QWaitCondition m_saveTrigger;

    enum LoadType {
        LoadHighPriority,
        LoadLowPriority,
    };

    enum SaveType {
        SaveData,
        SaveAccessTimeOnly,
    };

    QVector<std::pair<Picture *, LoadType>> m_loadQueue;
    QVector<std::pair<Picture *, SaveType>> m_saveQueue;
    QString m_dbName;
    QSqlDatabase m_db;
    QVector<QThread *> m_threads;

    int m_updateInterval = 0;
    Q3Cache<quint32, Picture> m_cache;
    Core *m_core;
    PictureCache *q;
    int m_cacheStatId = -1;
    int m_loadsStatId = -1;
    int m_savesStatId = -1;

    static quint32 cacheKey(const Item *item, const Color *color);
    static QString databaseTag(Picture *pic);
    static bool imageFromData(QImage &img, const QByteArray &data);
    bool isUpdateNeeded(Picture *pic) const;

    void load(Picture *pic, bool highPriority);
    void reprioritize(Picture *pic, bool highPriority);
    void save(Picture *pic);
    void loadThread(QString dbName, int index);
    void saveThread(QString dbName, int index);
    void transferJobFinished(TransferJob *j, Picture *pic);
};

} // namespace BrickLink
