// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QtCore/QFile>
#include <QtCore/QStringBuilder>
#include <QtCore/QUrlQuery>
#include <QtCore/QBuffer>
#include <QtCore/QLoggingCategory>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
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

PictureCache *Picture::s_cache = nullptr;

Picture::Picture(const Item *item, const Color *color)
    : m_item(item)
    , m_color(color)
{ }

Picture::~Picture()
{
    cancelUpdate();
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

void Picture::update(bool highPriority)
{
    if (s_cache)
        s_cache->updatePicture(this, highPriority);
}

void Picture::cancelUpdate()
{
    if (s_cache)
        s_cache->cancelPictureUpdate(this);
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

    Q_ASSERT(!Picture::s_cache);
    Picture::s_cache = this;

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
            if (auto *pic = job->userData("picture").value<Picture *>())
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
    Picture::s_cache = nullptr;
}

void PictureCache::setUpdateInterval(int interval)
{
    d->m_updateInterval = interval;
}

void PictureCache::clearCache()
{
    int lastLeftOver = 0;
    QElapsedTimer timer;
    QElapsedTimer absoluteTimer;
    absoluteTimer.start();

    // the loader/saver threads might hold references, so we need to wait for their queues to drain
    while (true) {
        int leftOver = d->m_cache.clearRecursive();

        if (!leftOver) {
            break;
        } else if ((leftOver == lastLeftOver) && (timer.elapsed() > 500)) {
            qCCritical(LogCache) << "Pictures:" << leftOver << "active references after"
                                 << absoluteTimer.elapsed() << "ms - giving up, expect a crash soon.";
            break;
        }

        if (lastLeftOver != leftOver) {
            qCWarning(LogCache) << "Pictures:" << leftOver << "active references after"
                                << absoluteTimer.elapsed() << "ms - waiting.";
            lastLeftOver = leftOver;
            timer.start();
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 500);
    }

    AppStatistics::inst()->update(d->m_cacheStatId, d->m_cache.count());
}

QPair<int, int> PictureCache::cacheStats() const
{
    return qMakePair(d->m_cache.totalCost(), d->m_cache.maxCost());
}

Picture *PictureCache::picture(const Item *item, const Color *color, bool highPriority)
{
    if (!item)
        return nullptr;

    if (!color)
        color = item->defaultColor();
    if (!color)
        color = d->m_core->color(0);

    auto key = PictureCachePrivate::cacheKey(item, color);
    Picture *pic = d->m_cache[key];

    bool needToLoad = !pic || (!pic->isValid() && (pic->updateStatus() == UpdateStatus::UpdateFailed));

    if (!pic) {
        pic = new Picture(item, color);
        int cost = pic->cost();
        if (!d->m_cache.insert(key, pic, cost)) {
            qCWarning(LogCache, "Can not add picture to cache (cache max/cur: %d/%d, item cost/id: %d/%s)",
                      int(d->m_cache.maxCost()), int(d->m_cache.totalCost()), int(cost), item->id().constData());
            return nullptr;
        }
        AppStatistics::inst()->update(d->m_cacheStatId, d->m_cache.count());
    }

    if (needToLoad) {
        pic->setUpdateStatus(UpdateStatus::Loading);
        d->load(pic, highPriority);
    } else if (highPriority) {
        // try to re-prioritize
        if (pic->updateStatus() == UpdateStatus::Loading)
            d->reprioritize(pic, true);
        else if ((pic->updateStatus() == UpdateStatus::Updating) && pic->m_transferJob)
            pic->m_transferJob->reprioritize(true);
    }

    return pic;
}

void PictureCache::updatePicture(Picture *pic, bool highPriority)
{
    if (!pic || (pic->m_updateStatus == UpdateStatus::Updating))
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

    pic->addRef();

    uint colorId = pic->color() ? pic->color()->id() : 0;
    QString url = u"https://img.bricklink.com/ItemImage/" + QChar::fromLatin1(pic->item()->itemTypeId())
                  + u"N/" + QString::number(colorId) + u'/' + QString::fromLatin1(pic->item()->id()) + u".png";

    pic->m_transferJob = TransferJob::get(url);
    pic->m_transferJob->setUserData("picture", QVariant::fromValue(pic));
    d->m_core->retrieve(pic->m_transferJob, highPriority);
}

void PictureCache::cancelPictureUpdate(Picture *pic)
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

QString PictureCachePrivate::databaseTag(Picture *pic)
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

bool PictureCachePrivate::isUpdateNeeded(Picture *pic) const
{
    return (m_updateInterval > 0)
            && (!pic->isValid()
                || (pic->lastUpdated().secsTo(QDateTime::currentDateTime()) > m_updateInterval));
}

void PictureCachePrivate::load(Picture *pic, bool highPriority)
{
    if (!pic)
        return;

    pic->addRef();
    m_loadMutex.lock();
    m_loadQueue.insert(highPriority ? 0 : m_loadQueue.size(),
                       { pic, highPriority ? LoadHighPriority : LoadLowPriority });
    m_loadTrigger.wakeOne();
    auto queueSize = m_loadQueue.size();
    m_loadMutex.unlock();

    AppStatistics::inst()->update(m_loadsStatId, queueSize);
}

void PictureCachePrivate::reprioritize(Picture *pic, bool highPriority)
{
    if (!pic)
        return;

    m_loadMutex.lock();
    for (auto i = 0; i < m_loadQueue.size(); ++i) {
        auto &lq = m_loadQueue[i];
        if (lq.first == pic) {
            lq.second = highPriority ? LoadHighPriority : LoadLowPriority;
            m_loadQueue.move(i, highPriority ? 0 : m_loadQueue.size());
            break;
        }
    }
    m_loadMutex.unlock();
}

void PictureCachePrivate::save(Picture *pic)
{
    if (!pic)
        return;

    pic->addRef();
    m_saveMutex.lock();
    m_saveQueue.append({ pic, SaveData });
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
            for (auto [pic, type] : std::as_const(m_loadQueue))
                pic->release();
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
            bool convertedFromOldCache = false;

            if (db.isOpen()) {
                loadQuery.bindValue(u":id"_qs, databaseTag(pic));

                loadQuery.exec();
                if (loadQuery.next()) {
                    lastUpdated = loadQuery.isNull(0) ? QDateTime()
                                                      : QDateTime::fromMSecsSinceEpoch(loadQuery.value(0).toLongLong());
                    auto data = loadQuery.value(1).toByteArray();
                    loaded = imageFromData(img, data);
                }
                loadQuery.finish();
            }
            // try the old file-system based cache
            if (!loaded) {
                bool large = (!pic->color());
                bool hasColors = pic->item()->itemType()->hasColors();
                QFile *f = m_core->dataReadFile(large ? u"large.jpg" : u"normal.png", pic->item(),
                                                (!large && hasColors) ? pic->color() : nullptr);
                if (f && f->isOpen()) {
                    lastUpdated = f->fileTime(QFile::FileModificationTime);
                    if (f->size() > 0)
                        convertedFromOldCache = loaded = imageFromData(img, f->readAll());
                    f->remove();
                }
                delete f;
            }

            pic->addRef(); // the release will happen on the main thread (see the invokeMethod below)
            QMetaObject::invokeMethod(m_core, [=, this, pic=pic]() { // clang bug: P1091R3
                if (loaded) {
                    pic->setLastUpdated(lastUpdated);
                    pic->setImage(img);

                    // update the last accessed time stamp
                    pic->addRef();
                    m_saveMutex.lock();
                    m_saveQueue.append({ pic, convertedFromOldCache ? SaveData : SaveAccessTimeOnly });
                    m_saveTrigger.wakeOne();
                    m_saveMutex.unlock();
                }
                pic->setIsValid(loaded);
                pic->setUpdateStatus(UpdateStatus::Ok);

                if (pic->m_updateAfterLoad || isUpdateNeeded(pic))  {
                    pic->m_updateAfterLoad = false;
                    q->updatePicture(pic, highPriority);
                }
                if (loaded && img.isNull())
                    pic->setIsValid(false);

                m_cache.setObjectCost(cacheKey(pic->item(), pic->color()), pic->cost());

                emit q->pictureUpdated(pic);
                pic->release();
            }, Qt::QueuedConnection);

            pic->release();
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
            for (auto [pic, saveType] : saveQueueCopy) {
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
                    imageDataHash.insert(pic, data);
                }
            }

            if (db.isOpen()) {
                db.transaction();

                qint64 now = QDateTime::currentMSecsSinceEpoch();

                for (auto [pic, saveType] : saveQueueCopy) {
                    auto dbTag = databaseTag(pic);

                    if (saveType == SaveAccessTimeOnly) {
                        accessQuery.bindValue(u":id"_qs, dbTag);
                        accessQuery.bindValue(u":accessed"_qs, now);
                        if (!accessQuery.exec()) {
                            qCWarning(LogSql) << "Failed to update the access time of a picture:"
                                              << accessQuery.lastError().text();
                        }
                        accessQuery.finish();
                    } else {
                        const auto data = imageDataHash.value(pic);
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
                    pic->release();
                }
                db.commit();
            }
        }
    }
    db.close();
}

void PictureCachePrivate::transferJobFinished(TransferJob *j, Picture *pic)
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
    pic->release();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype Picture
    \inqmlmodule BrickLink
    \ingroup qml-api
    \brief This value type represents a picture of a BrickLink item.

    Each picture of an item in the BrickLink catalog is available as a Picture object.

    You cannot create Picture objects yourself, but you can retrieve a Picture object given the
    item and color id via BrickLink::picture().

    \note Pictures aren't readily available, but need to be asynchronously loaded (or even
          downloaded) at runtime. You need to connect to the signal BrickLink::pictureUpdated()
          to know when the data has been loaded.
*/
/*! \qmlproperty bool Picture::isNull
    \readonly
    Returns whether this Picture is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty ItemPointer Picture::item
    \readonly
    The BrickLink item reference this picture is requested for as a raw
    C++ pointer. You can convert it to a QML \l Item object like this:
    \code
    let item = BrickLink.item(pic.item)
    \endcode
*/
/*! \qmlproperty ColorPointer Picture::color
    \readonly
    The BrickLink color reference this picture is requested for as a raw
    C++ pointer. You can convert it to a QML \l Color object like this:
    \code
    let color = BrickLink.color(pic.color)
    \endcode
*/
/*! \qmlproperty date Picture::lastUpdated
    \readonly
    Holds the time stamp of the last successful update of this picture.
*/
/*! \qmlproperty UpdateStatus Picture::updateStatus
    \readonly
    Returns the current update status. The available values are:
    \value BrickLink.UpdateStatus.Ok            The last picture load (or download) was successful.
    \value BrickLink.UpdateStatus.Loading       BrickStore is currently loading the picture from the local cache.
    \value BrickLink.UpdateStatus.Updating      BrickStore is currently downloading the picture from BrickLink.
    \value BrickLink.UpdateStatus.UpdateFailed  The last download from BrickLink failed. isValid might still be
                                                \c true, if there was a valid picture available before the
                                                failed update!
*/
/*! \qmlproperty bool Picture::isValid
    \readonly
    Returns whether the image property currently holds a valid image.
*/
/*! \qmlproperty image Picture::image
    \readonly
    Returns the image if the Picture object isValid, or a null image otherwise.
*/
/*! \qmlmethod Picture::update(bool highPriority = false)
    Tries to re-download the picture from the BrickLink server. If you set \a highPriority to \c
    true the load/download request will be prepended to the work queue instead of appended.
*/

} // namespace BrickLink
