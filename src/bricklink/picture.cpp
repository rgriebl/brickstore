// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QtCore/QFile>
#include <QtCore/QStringBuilder>
#include <QtCore/QUrlQuery>
#include <QtCore/QBuffer>
#include <QtCore/QLoggingCategory>
#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtNetwork/QNetworkInformation>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "bricklink/picture.h"
#include "bricklink/picture_p.h"
#include "bricklink/item.h"
#include "bricklink/core.h"
#include "utility/appstatistics.h"
#include "utility/transfer.h"

Q_DECLARE_LOGGING_CATEGORY(LogCache)
Q_DECLARE_LOGGING_CATEGORY(LogSql)


namespace BrickLink {

Picture::Picture(Private, const Item *item, const Color *color)
    : m_item(item)
    , m_color(color)
    , m_generation(Database::generation())
{ }

bool Picture::isStale() const
{
    return (m_generation != Database::generation());
}

const Item *Picture::item() const
{
    return isStale() ? nullptr : m_item;
}

const Color *Picture::color() const
{
    return isStale() ? nullptr : m_color;
}

Picture::~Picture()
{
    // No cancelUpdate() here: while a transfer job is running, its user data owns a reference to
    // this picture, so we could not be destroyed at all.
    Q_ASSERT(!m_transferJob);
}

// A picture can be handed out to a worker thread, which may end up dropping the last reference to
// it. Destroying a QObject outside of its own thread is not allowed, so hop over if needed.
static void deletePicture(Picture *pic)
{
    if (pic->thread() == QThread::currentThread())
        delete pic;
    else
        pic->deleteLater();
}

const QImage Picture::image() const
{
    return m_image;
}

int Picture::cost() const
{
    if (m_image.isNull())
        return 1;
    else
        return int(m_image.sizeInBytes() / 1024);
}

void Picture::setIsValid(bool valid)
{
    if (valid != m_valid) {
        m_valid = valid;
        emit isValidChanged(valid);
    }
}

void Picture::setUpdateStatus(UpdateStatus status)
{
    if (status != m_updateStatus) {
        m_updateStatus = status;
        emit updateStatusChanged(status);
    }
}

void Picture::setLastUpdated(const QDateTime &dt)
{
    if (dt != m_lastUpdated) {
        m_lastUpdated = dt;
        emit lastUpdatedChanged(dt);
    }
}

void Picture::setImage(const QImage &newImage)
{
    if (newImage != m_image) {
        m_image = newImage;
        emit imageChanged(m_image);
    }
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


PictureCache::PictureCache(Core *core, quint64 physicalMem)
    : QObject(core)
    , d(new PictureCachePrivate)
{
    d->q = this;
    d->m_core = core;

    d->m_cacheStatId = AppStatistics::inst()->addSource(u"Pictures in memory cache"_qs);
    d->m_loadsStatId = AppStatistics::inst()->addSource(u"Pictures queued for disk load"_qs);
    d->m_savesStatId = AppStatistics::inst()->addSource(u"Pictures queued for disk save"_qs);

    // The max. pic cache size is at least 500MB. On 64bit systems, this gets expanded to a quarter
    // of the physical memory, but it is capped at 4GB
    quint64 picCacheMem = 500'000'000ULL; // more than that and Win32 runs out of memory

    if (physicalMem && (Q_PROCESSOR_WORDSIZE >= 8))
        picCacheMem = std::clamp(physicalMem / 4, picCacheMem, picCacheMem * 8);
    d->m_cache.setMaxCost(int(picCacheMem / 1024)); // each pic has the cost of memory used in KB

    qInfo().noquote() << "Picture cache:"
                      << QByteArray::number(double(picCacheMem) / 1'000'000'000ULL, 'f', 1) << "GB";

    connect(core, &Core::transferFinished,
            this, [this](TransferJob *job) {
        if (job) {
            if (auto pic = job->userData("picture").value<PictureRef>())
                d->transferJobFinished(job, pic);
        }
    });

    d->m_dbName = core->dataPath() + u"picture_cache.sqlite"_qs;
    d->m_db = QSqlDatabase::addDatabase(u"QSQLITE"_qs, u"PictureCache"_qs);
    d->m_db.setDatabaseName(d->m_dbName);

    // try to start from scratch, if the DB open fails
    if (!d->m_db.open() &&
            !(QFile::exists(d->m_dbName) && QFile::remove(d->m_dbName) && d->m_db.open())) {
        qCWarning(LogSql) << "Failed to open the picture database:" << d->m_db.lastError().text();
    }

    if (d->m_db.isOpen()) {
        QSqlQuery createQuery(d->m_db);
        if (!createQuery.exec(
                    u"CREATE TABLE IF NOT EXISTS pic ("
                    "id TEXT NOT NULL PRIMARY KEY, "
                    "updated INTEGER, "             // msecsSinceEpoch
                    "accessed INTEGER NOT NULL, "   // msecsSinceEpoch
                    "data BLOB) WITHOUT ROWID;"_qs)) {
            qCWarning(LogSql) << "Failed to create the 'pic' table in the picture database:"
                              << createQuery.lastError().text();
            d->m_db.close();
        }
    }

    if (d->m_db.isOpen()) {
        static constexpr int DBVersion = 1;

        {
            QSqlQuery jnlQuery(u"PRAGMA journal_mode = wal;"_qs, d->m_db);
            if (jnlQuery.lastError().isValid())
                qCWarning(LogSql) << "Failed to set journaling mode to 'wal' on the picture database:"
                                  << jnlQuery.lastError();
        }
        {
            QSqlQuery uvQuery(u"PRAGMA user_version;"_qs, d->m_db);
            uvQuery.next();
            auto userVersion = uvQuery.value(0).toInt();
            if (userVersion == 0) // brand new file, bump version
                QSqlQuery(u"PRAGMA user_version=%1;"_qs.arg(DBVersion), d->m_db);
        }

        // DB schema upgrade code goes here...
    }

#if 0 // DB conversion helper
    {
        auto dbold = QSqlDatabase::addDatabase(u"QSQLITE"_qs, u"PictureCacheOld"_qs);
        dbold.setDatabaseName(d->m_dbName + u".old");

        if (dbold.open()) {
            QSqlQuery oldQuery(u"SELECT * from pic;"_qs, dbold);
            d->m_db.transaction();
            QSqlQuery newQuery(d->m_db);
            newQuery.prepare(u"INSERT INTO pic(id,updated,accessed,data) VALUES(:id,:updated,:accessed,:data);"_qs);

            int cnt = 0;

            while (oldQuery.next()) {
                QString oldId = oldQuery.value(0).toString();
                QVariant oldUpdated = oldQuery.value(1);
                QVariant oldAccessed = oldQuery.value(2);
                QByteArray oldData = oldQuery.value(3).toByteArray();

                QByteArray webpData;
                QBuffer buffer(&webpData);
                QImage::fromData(oldData).save(&buffer, "WEBP", 80);
                oldData = webpData;

                if (oldData.size() && !oldData.startsWith("RIFF")) {
                    qWarning() << "Image for" << oldId << "is not WEBP!";
                    continue;
                }

                newQuery.bindValue(u":id"_qs, oldId);
                newQuery.bindValue(u":updated"_qs, oldUpdated);
                newQuery.bindValue(u":accessed"_qs, oldAccessed);
                newQuery.bindValue(u":data"_qs, oldData);
                if (!newQuery.exec()) {
                    qWarning() << "could not add to new db" << newQuery.lastError().text();
                }
                newQuery.finish();

                if ((++cnt % 100) == 0) {
                    qWarning() << "Images converted..." << cnt;
                }
            }
            d->m_db.commit();
            dbold.close();
        }

        exit(0);
    }
#endif

    //TODO: on mobile: if DB size > maxSize, remove old entries until size <= maxSize

    for (int i = 0; i < 1 /*qMax(2, QThread::idealThreadCount() / 4)*/; ++i) {
        auto t = QThread::create(&PictureCachePrivate::saveThread, d, d->m_db.connectionName(), i);
        t->setObjectName(u"Pic Saver %1"_qs.arg(i));
        d->m_threads.append(t);
    }
    for (int i = 0; i < std::clamp(QThread::idealThreadCount(), 2, 8); ++i) {
        auto t = QThread::create(&PictureCachePrivate::loadThread, d, d->m_db.connectionName(), i);
        t->setObjectName(u"Pic Loader %1"_qs.arg(i));
        d->m_threads.append(t);
    }

    for (auto *thread : std::as_const(d->m_threads))
        thread->start(QThread::LowPriority);
}

PictureCache::~PictureCache()
{
    d->m_stop = true;
    d->m_loadMutex.lock();
    d->m_loadTrigger.wakeAll();
    d->m_loadMutex.unlock();
    d->m_saveMutex.lock();
    d->m_saveTrigger.wakeAll();
    d->m_saveMutex.unlock();
    for (auto *thread : std::as_const(d->m_threads)) {
        thread->wait();
        delete thread;
    }
    d->m_db.close();
    delete d;
}

void PictureCache::setUpdateInterval(int interval)
{
    d->m_updateInterval = interval;
}

void PictureCache::clearCache()
{
    // Pictures that are still in use - by a widget, by the loader/saver queues - simply outlive the
    // cache entry now, so there is nothing to wait for anymore.
    d->m_cache.clear();

    AppStatistics::inst()->update(d->m_cacheStatId, d->m_cache.size());
}

QPair<int, int> PictureCache::cacheStats() const
{
    return qMakePair(d->m_cache.totalCost(), d->m_cache.maxCost());
}

PictureRef PictureCache::picture(const Item *item, const Color *color, bool highPriority)
{
    if (!item)
        return { };

    if (!color)
        color = item->defaultColor();
    if (!color)
        color = d->m_core->color(0);

    auto key = PictureCachePrivate::cacheKey(item, color);
    PictureRef pic = d->m_cache[key];

    // A picture that survived a database update is inert (see Picture::isStale()) and would keep its
    // key occupied forever, as insert() never replaces an existing entry.
    if (pic && pic->isStale()) {
        d->m_cache.remove(key);
        pic.reset();
    }

    bool needToLoad = !pic || (!pic->isValid() && (pic->updateStatus() == UpdateStatus::UpdateFailed));

    if (!pic) {
        PictureRef newPic { new Picture(Picture::Private { }, item, color), &deletePicture };
        pic = d->m_cache.insert(key, std::move(newPic), 1 /* start with roughly 1KB cost */);
        AppStatistics::inst()->update(d->m_cacheStatId, d->m_cache.size());
    }

    if (needToLoad) {
        pic->setUpdateStatus(UpdateStatus::Loading);
        d->load(pic, highPriority);
    } else if (highPriority) {
        // try to re-prioritize
        if (pic->updateStatus() == UpdateStatus::Loading)
            d->reprioritize(pic.get(), true);
        else if ((pic->updateStatus() == UpdateStatus::Updating) && pic->m_transferJob)
            pic->m_transferJob->reprioritize(true);
    }

    return pic;
}

void PictureCache::updatePicture(const PictureRef &pic, bool highPriority)
{
    // a stale picture has no item anymore, so there is nothing left to download
    if (!pic || !pic->item() || (pic->m_updateStatus == UpdateStatus::Updating))
        return;

    if (QNetworkInformation::instance()
        && QNetworkInformation::instance()->supports(QNetworkInformation::Feature::Reachability)
        && (QNetworkInformation::instance()->reachability() != QNetworkInformation::Reachability::Online)) {
        pic->setUpdateStatus(UpdateStatus::UpdateFailed);
        emit pictureUpdated(pic);
        return;
    }

    if (pic->m_updateStatus == UpdateStatus::Loading) {
        pic->m_updateAfterLoad = true;
        return;
    }

    pic->setUpdateStatus(UpdateStatus::Updating);

    uint colorId = pic->color() ? pic->color()->id() : 0;
    QString url = u"https://img.bricklink.com/ItemImage/" + QChar::fromLatin1(pic->item()->itemTypeId())
                  + u"N/" + QString::number(colorId) + u'/' + QString::fromLatin1(pic->item()->id()) + u".png";

    pic->m_transferJob = TransferJob::get(url);
    // the job owns a reference, so the picture cannot go away while it is being downloaded
    pic->m_transferJob->setUserData("picture", QVariant::fromValue(pic));
    d->m_core->retrieve(pic->m_transferJob, highPriority);
}

void PictureCache::cancelPictureUpdate(const PictureRef &pic)
{
    if (pic && pic->m_transferJob)
        pic->m_transferJob->abort();
}

void PictureCache::cancelAllPictureUpdates()
{
    const auto keys = d->m_cache.keys();
    for (const auto &key : keys)
        cancelPictureUpdate(d->m_cache.object(key));
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


quint32 PictureCachePrivate::cacheKey(const Item *item, const Color *color)
{
    // 11 bit color-index | 21 bit item-index
    return (quint32(color ? (color->index() + 1) : 0) << 21)
            | (quint32(item ? (item->index() + 1) : 0));
}

QString PictureCachePrivate::databaseTag(const Picture *pic)
{
    if (!pic || !pic->item())
        return { };

    return QChar::fromLatin1(pic->item()->itemTypeId()) + QString::fromLatin1(pic->item()->id())
            + u'@' + QString::number(pic->color() ? pic->color()->id() : 0);
}

bool PictureCachePrivate::imageFromData(QImage &img, const QByteArray &data)
{
    bool valid = false;
    try {
        // optimize loading when a lot of QImageIO plugins are available
        // (e.g. when building against Qt from a Linux distro)
        if (data.isEmpty()) // there is no image
            valid = true;
        else if (data.startsWith("RIFF") && (data.mid(8, 4) == "WEBP"))
            valid = img.loadFromData(data, "WEBP");
        else if (data.startsWith("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"))
            valid = img.loadFromData(data, "PNG");
        else if (data.startsWith("GIF8"))
            valid = img.loadFromData(data, "GIF");
        else if (data.startsWith("\xFF\xD8\xFF"))
            valid = img.loadFromData(data, "JPG");
        if (!valid)
            valid = img.loadFromData(data);
    } catch (const std::bad_alloc &) {
        img = { };
        valid = false;
    }
    return valid;
}

bool PictureCachePrivate::isUpdateNeeded(const Picture *pic) const
{
    return (m_updateInterval > 0)
            && (!pic->isValid()
                || (pic->lastUpdated().secsTo(QDateTime::currentDateTime()) > m_updateInterval));
}

void PictureCachePrivate::load(PictureRef pic, bool highPriority)
{
    if (!pic)
        return;

    m_loadMutex.lock();
    m_loadQueue.insert(highPriority ? 0 : m_loadQueue.size(),
                       { std::move(pic), highPriority ? LoadHighPriority : LoadLowPriority });
    m_loadTrigger.wakeOne();
    auto queueSize = m_loadQueue.size();
    m_loadMutex.unlock();

    AppStatistics::inst()->update(m_loadsStatId, queueSize);
}

void PictureCachePrivate::reprioritize(const Picture *pic, bool highPriority)
{
    if (!pic)
        return;

    m_loadMutex.lock();
    for (auto i = 0; i < m_loadQueue.size(); ++i) {
        auto &lq = m_loadQueue[i];
        if (lq.first.get() == pic) {
            lq.second = highPriority ? LoadHighPriority : LoadLowPriority;
            m_loadQueue.move(i, highPriority ? 0 : m_loadQueue.size());
            break;
        }
    }
    m_loadMutex.unlock();
}

void PictureCachePrivate::save(PictureRef pic)
{
    if (!pic)
        return;

    m_saveMutex.lock();
    m_saveQueue.append({ std::move(pic), SaveData });
    m_saveTrigger.wakeOne();
    auto queueSize = m_saveQueue.size();
    m_saveMutex.unlock();

    AppStatistics::inst()->update(m_savesStatId, queueSize);
}

void PictureCachePrivate::loadThread(QString dbName, int index)
{
    auto db = QSqlDatabase::cloneDatabase(dbName, dbName + u"_Reader_" + QString::number(index));
    db.open();

    QSqlQuery loadQuery(db);
    loadQuery.prepare(u"SELECT updated,data FROM pic WHERE id=:id;"_qs);

    while (!m_stop) {
        QMutexLocker locker(&m_loadMutex);
        if (m_loadQueue.isEmpty())
            m_loadTrigger.wait(&m_loadMutex);

        if (m_stop) {
            m_loadQueue.clear();
            continue;
        }

        if (!m_loadQueue.isEmpty()) {
            auto [pic, loadType] = m_loadQueue.takeFirst();
            auto queueSize = m_loadQueue.size();
            locker.unlock();

            AppStatistics::inst()->update(m_loadsStatId, queueSize);

            bool loaded = false;
            QDateTime lastUpdated;
            QImage img;
            bool highPriority = (loadType == LoadHighPriority);

            if (db.isOpen()) {
                loadQuery.bindValue(u":id"_qs, databaseTag(pic.get()));

                loadQuery.exec();
                if (loadQuery.next()) {
                    lastUpdated = loadQuery.isNull(0) ? QDateTime()
                                                      : QDateTime::fromMSecsSinceEpoch(loadQuery.value(0).toLongLong());
                    auto data = loadQuery.value(1).toByteArray();
                    loaded = imageFromData(img, data);
                }
                loadQuery.finish();
            }

            // the captured reference keeps the picture alive until this has run - or until it is
            // discarded, if the core object goes away first
            QMetaObject::invokeMethod(m_core, [this, loaded, lastUpdated, img, highPriority, pic=pic]() { // clang bug: P1091R3
                if (loaded) {
                    pic->setLastUpdated(lastUpdated);
                    pic->setImage(img);

                    // update the last accessed time stamp
                    m_saveMutex.lock();
                    m_saveQueue.append({ pic, SaveAccessTimeOnly });
                    m_saveTrigger.wakeOne();
                    m_saveMutex.unlock();
                }
                pic->setIsValid(loaded);
                pic->setUpdateStatus(UpdateStatus::Ok);

                if (pic->m_updateAfterLoad || isUpdateNeeded(pic.get()))  {
                    pic->m_updateAfterLoad = false;
                    q->updatePicture(pic, highPriority);
                }
                if (loaded && img.isNull())
                    pic->setIsValid(false);

                m_cache.setObjectCost(cacheKey(pic->item(), pic->color()), pic->cost());

                emit q->pictureUpdated(pic);
            }, Qt::QueuedConnection);
        }
    }
    db.close();
}

void PictureCachePrivate::saveThread(QString dbName, int index)
{
    auto db = QSqlDatabase::cloneDatabase(dbName, dbName + u"_Writer_" + QString::number(index));
    db.open();

    QSqlQuery saveQuery(db);
    saveQuery.prepare(u"INSERT INTO pic(id,updated,accessed,data) VALUES(:id,:updated,:accessed,:data) "
                      "ON CONFLICT(id) DO UPDATE "
                      "SET updated=excluded.updated,accessed=excluded.accessed,data=excluded.data;"_qs);

    QSqlQuery accessQuery(db);
    accessQuery.prepare(u"UPDATE pic SET accessed=:accessed WHERE id=:id;"_qs);

    while (!m_stop) {
        QMutexLocker locker(&m_saveMutex);
        if (m_saveQueue.isEmpty())
            m_saveTrigger.wait(&m_saveMutex);

        if (!m_saveQueue.isEmpty()) {
            // we might have multiple saver threads, so don't grab the full queue at once
            const auto saveQueueCopy = m_saveQueue.mid(0, 20);
            m_saveQueue.remove(0, saveQueueCopy.size());
            auto queueSize = m_saveQueue.size();
            locker.unlock();

            AppStatistics::inst()->update(m_savesStatId, queueSize);

            QHash<Picture *, QByteArray> imageDataHash;

            // do all this before starting a DB transaction, to keep lock times to a minimum
            for (const auto &[pic, saveType] : saveQueueCopy) {
                if (saveType == SaveData) {
                    QByteArray data;
                    if (!pic->m_image.isNull()) {
                        // WebP lossy at 80% compresses to ~10-20% of the original PNG size
                        // with next to no visible artifacts
                        QByteArray webpData;
                        QBuffer buffer(&webpData);
                        pic->m_image.save(&buffer, "WEBP", 80);
                        //if (webpData.size() < data.size())
                        //    qWarning() << "Saving image as WEBP compresses to" << (100 * webpData.size() / data.size()) << "%";
                        data = webpData;
                    }
                    imageDataHash.insert(pic.get(), data);
                }
            }

            if (db.isOpen()) {
                db.transaction();

                qint64 now = QDateTime::currentMSecsSinceEpoch();

                for (const auto &[pic, saveType] : saveQueueCopy) {
                    auto dbTag = databaseTag(pic.get());

                    if (saveType == SaveAccessTimeOnly) {
                        accessQuery.bindValue(u":id"_qs, dbTag);
                        accessQuery.bindValue(u":accessed"_qs, now);
                        if (!accessQuery.exec()) {
                            qCWarning(LogSql) << "Failed to update the access time of a picture:"
                                              << accessQuery.lastError().text();
                        }
                        accessQuery.finish();
                    } else {
                        const auto data = imageDataHash.value(pic.get());
                        auto lastUpdated = QVariant(QMetaType::fromType<qint64>());
                        if (pic->lastUpdated().isValid())
                            lastUpdated = QVariant::fromValue(pic->lastUpdated().toMSecsSinceEpoch());

                        saveQuery.bindValue(u":id"_qs, dbTag);
                        saveQuery.bindValue(u":updated"_qs, lastUpdated);
                        saveQuery.bindValue(u":accessed"_qs, now);
                        saveQuery.bindValue(u":data"_qs, data);

                        if (!saveQuery.exec()) {
                            qCWarning(LogSql) << "Failed to save picture data:"
                                              << saveQuery.lastError().text();
                        }
                        saveQuery.finish();
                    }
                }
                db.commit();
            }
        }
    }
    db.close();
}

void PictureCachePrivate::transferJobFinished(TransferJob *j, const PictureRef &pic)
{
    Q_ASSERT(pic && (j == pic->m_transferJob));
    pic->m_transferJob = nullptr;

    if (j->isCompleted()) {
        QImage img;
        if (imageFromData(img, j->data())) {
            pic->setLastUpdated(QDateTime::currentDateTime());
            pic->setImage(img);
            pic->setIsValid(true);
            pic->setUpdateStatus(UpdateStatus::Ok);
            m_cache.setObjectCost(cacheKey(pic->item(), pic->color()), pic->cost());

            save(pic);
        }
    } else {
        if (j->responseCode() == 404)
            save(pic);

        pic->setUpdateStatus(UpdateStatus::UpdateFailed);
    }

    emit q->pictureUpdated(pic);
    // no release needed: the job's user data owned the reference
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

} // namespace BrickLink
