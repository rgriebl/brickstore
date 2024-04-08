// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QtCore/QLocale>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringBuilder>
#include <QtCore/QUrlQuery>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QTimer>
#include <QtCore/QLoggingCategory>
#include <QtCore/QCoreApplication>
#include <QtNetwork/QNetworkInformation>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "utility/appstatistics.h"
#include "utility/exception.h"
#include "utility/transfer.h"
#include "utility/utility.h"
#include "bricklink/priceguide.h"
#include "bricklink/priceguide_p.h"
#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/color.h"

Q_DECLARE_LOGGING_CATEGORY(LogCache)
Q_DECLARE_LOGGING_CATEGORY(LogSql)


namespace BrickLink {

PriceGuideCache *PriceGuide::s_cache = nullptr;

PriceGuide::PriceGuide(const Item *item, const Color *color, VatType vatType)
    : m_item(item)
    , m_color(color)
    , m_vatType(vatType)
{ }

PriceGuide::~PriceGuide()
{
    cancelUpdate();
}

void PriceGuide::setIsValid(bool valid)
{
    if (valid != m_valid) {
        m_valid = valid;
        emit isValidChanged(valid);
    }
}

void PriceGuide::setUpdateStatus(UpdateStatus status)
{
    if (status != m_updateStatus) {
        m_updateStatus = status;
        emit updateStatusChanged(status);
    }
}

void PriceGuide::setLastUpdated(const QDateTime &dt)
{
    if (dt != m_lastUpdated) {
        m_lastUpdated = dt;
        emit lastUpdatedChanged(dt);
    }
}

void PriceGuide::update(bool highPriority)
{
    if (s_cache)
        s_cache->updatePriceGuide(this, highPriority);
}

void PriceGuide::cancelUpdate()
{
    if (s_cache)
        s_cache->cancelPriceGuideUpdate(this);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


PriceGuideRetrieverInterface::PriceGuideRetrieverInterface(QObject *parent)
    : QObject(parent)
{ }


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SingleHTMLScrapePGRetriever::SingleHTMLScrapePGRetriever(Core *core)
    : PriceGuideRetrieverInterface(core)
    , m_core(core)
{
    connect(m_core, &Core::transferFinished,
            this, [this](TransferJob *job) {
        if (job) {
            if (auto *pg = job->userData("htmlPriceGuide").value<PriceGuide *>())
                transferJobFinished(job, pg);
        }
    });
}

QVector<VatType> SingleHTMLScrapePGRetriever::supportedVatTypes() const
{
    static const QVector<VatType> all = { VatType::Excluded, VatType::Included };
    return all;
}

void SingleHTMLScrapePGRetriever::fetch(PriceGuide *pg, bool highPriority)
{
    if (m_jobs.contains(pg))
        return;

    pg->addRef();

    QUrl url = QUrl(u"https://www.bricklink.com/priceGuideSummary.asp"_qs);
    url.setQuery({
        { u"a"_qs,           QString(QChar::fromLatin1(pg->item()->itemTypeId())) },
        { u"vcID"_qs,        u"1"_qs }, // USD
        { u"vatInc"_qs,      (pg->vatType() == VatType::Included) ? u"Y"_qs : u"N"_qs },
        { u"viewExclude"_qs, u"Y"_qs },
        { u"ajView"_qs,      u"Y"_qs }, // only the AJAX snippet
        { u"colorID"_qs,     QString::number(pg->color()->id()) },
        { u"itemID"_qs,      Utility::urlQueryEscape(pg->item()->id()) },
        { u"uncache"_qs,     QString::number(QDateTime::currentMSecsSinceEpoch()) },
    });

    auto job = TransferJob::get(url, nullptr, 2);
    job->setUserData("htmlPriceGuide", QVariant::fromValue(pg));
    m_jobs.insert(pg, job);

    m_core->retrieve(job, highPriority);
}

void SingleHTMLScrapePGRetriever::cancel(PriceGuide *pg)
{
    if (auto job = m_jobs.value(pg))
        job->abort();
}

void SingleHTMLScrapePGRetriever::cancelAll()
{
    for (auto job : std::as_const(m_jobs))
        job->abort();
}

void SingleHTMLScrapePGRetriever::transferJobFinished(TransferJob *j, PriceGuide *pg)
{
    auto *job = m_jobs.take(pg);
    Q_ASSERT(job == j);

    try {
        if (job->isCompleted()) {
            PriceGuide::Data data;
            if (parseHtml(*job->data(), data))
                emit finished(pg, data);
            else
                throw Exception("invalid price-guide data");
        } else if (job->isAborted()) {
            throw Exception(job->errorString());
        } else {
            throw Exception("%1 (%2)").arg(job->errorString()).arg(job->responseCode());
        }
    } catch (const Exception &e) {
        emit failed(pg, u"PG download for " + QString::fromLatin1(pg->item()->id()) + u" failed: " + e.errorString());
    }
    pg->release();
}

bool SingleHTMLScrapePGRetriever::parseHtml(const QByteArray &ba, PriceGuide::Data &result)
{
    static const QRegularExpression re(uR"(<B>([A-Za-z-]*?): </B><.*?> (\d+) <.*?> (\d+) <.*?> US \$([0-9.,]+) <.*?> US \$([0-9.,]+) <.*?> US \$([0-9.,]+) <.*?> US \$([0-9.,]+) <)"_qs);

    QString s = QString::fromUtf8(ba).replace(u"&nbsp;"_qs, u" "_qs);
    QLocale en_US(u"en_US"_qs);

    result = { };

    int matchCounter = 0;
    int startPos = 0;

    bool noData = (s.indexOf(u">(No Data)<") > 0);
    if (noData) {
        result = { };
        return true;
    }

    int currentPos = int(s.indexOf(u"Current Items for Sale"));
    bool hasCurrent = (currentPos > 0);
    int pastSixPos = int(s.indexOf(u"Past 6 Months Sales"));
    bool hasPastSix = (pastSixPos > 0);

//    qWarning() << s;

    for (int i = 0; i < 4; ++i) {
        auto m = re.match(s, startPos);
        if (m.hasMatch()) {
            int ti = -1;
            int ci = -1;

            int matchPos = int(m.capturedStart(0));
            int matchEnd = int(m.capturedEnd(0));

            // if both pastSix and current are available, pastSix comes first
            if (hasCurrent && (matchPos > currentPos))
                ti = int(Time::Current);
            else if (hasPastSix && (matchPos > pastSixPos))
                ti = int(Time::PastSix);

            const QString condStr = m.captured(1);
            if (condStr == u"Used") {
                ci = int(Condition::Used);
            } else if (condStr == u"New") {
                ci = int(Condition::New);
            } else if (condStr.isEmpty()) {
                ci = int(Condition::New);
                if (result.lots[ti][ci])
                    ci = int(Condition::Used);
            }

//            qWarning() << i << ti << ci << m.capturedTexts().mid(1);
//            qWarning() << "   start:" << startPos << "match start:" << matchPos << "match end:" << matchEnd;

            if ((ti == -1) || (ci == -1))
                continue;

            result.lots[ti][ci]                         = en_US.toInt(m.captured(2));
            result.quantities[ti][ci]                   = en_US.toInt(m.captured(3));
            result.prices[ti][ci][int(Price::Lowest)]   = en_US.toDouble(m.captured(4));
            result.prices[ti][ci][int(Price::Average)]  = en_US.toDouble(m.captured(5));
            result.prices[ti][ci][int(Price::WAverage)] = en_US.toDouble(m.captured(6));
            result.prices[ti][ci][int(Price::Highest)]  = en_US.toDouble(m.captured(7));

            ++matchCounter;
            startPos = matchEnd;
        }
    }
    return (matchCounter > 0);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


BatchedAffiliateAPIPGRetriever::BatchedAffiliateAPIPGRetriever(Core *core, const QString &apiKey)
    : PriceGuideRetrieverInterface(core)
    , m_core(core)
    , m_batchTimer(new QTimer(this))
    , m_apiKey(apiKey)
{
    m_batchTimer->setSingleShot(true);
    m_batchTimer->setInterval(MaxBatchAgeMSec);
    connect(m_batchTimer, &QTimer::timeout, this, &BatchedAffiliateAPIPGRetriever::check);

    m_currentBatch.reserve(MaxBatchSize);

    connect(m_core, &Core::transferFinished,
            this, [this](TransferJob *job) {
        if (job) {
            if (job->userData("batchedPriceGuide").toBool())
                transferJobFinished(job);
        }
    });
}

QVector<VatType> BatchedAffiliateAPIPGRetriever::supportedVatTypes() const
{
    static const QVector<VatType> all { VatType::Excluded, VatType::EU, VatType::Norway, VatType::UK };
    return all;
}

void BatchedAffiliateAPIPGRetriever::fetch(PriceGuide *pg, bool highPriority)
{
    // check if the pg is currently being fetched
    if (m_currentBatch.contains(pg))
        return;

    bool wrongVatType = (pg->vatType() != m_nextBatchVatType);
    auto &queue = wrongVatType ? m_wrongVatTypeQueue : m_nextBatch;
    auto &prioSize = wrongVatType ? m_wrongVatTypeQueuePrioritySize : m_nextBatchPrioritySize;

    // check if the pg is already scheduled for the next batch and if we need to up the priority
    auto it = std::find_if(queue.cbegin(), queue.cend(), [pg](const auto &pair) {
        return (pair.first == pg);
    });
    if (it != queue.cend()) {
        auto index = std::distance(queue.cbegin(), it);
        if (index >= prioSize)
            queue.move(index, prioSize++);
        return;
    }

    pg->addRef();

    QElapsedTimer now;
    now.start();

    if (highPriority)
        queue.emplace(prioSize++, pg, now);
    else
        queue.emplace_back(pg, now);

    check();
}

void BatchedAffiliateAPIPGRetriever::cancel(PriceGuide *pg)
{
    if (m_currentBatch.contains(pg))
        m_currentJob->abort();

    bool wrongVatType = (pg->vatType() != m_nextBatchVatType);
    auto &queue = wrongVatType ? m_wrongVatTypeQueue : m_nextBatch;
    auto &prioSize = wrongVatType ? m_wrongVatTypeQueuePrioritySize : m_nextBatchPrioritySize;

    auto it = std::find_if(queue.cbegin(), queue.cend(), [pg](const auto &pair) {
        return (pair.first == pg);
    });
    if (it != queue.cend()) {
        auto index = std::distance(queue.cbegin(), it);
        if (index < prioSize)
            --prioSize;
        queue.removeAt(index);

        emit failed(pg, u"aborted"_qs);
        pg->release();
    }
}

void BatchedAffiliateAPIPGRetriever::cancelAll()
{
    if (m_currentJob)
        m_currentJob->abort();

    const auto list = m_wrongVatTypeQueue + m_nextBatch;

    m_wrongVatTypeQueue.clear();
    m_nextBatch.clear();
    m_wrongVatTypeQueuePrioritySize = 0;
    m_nextBatchPrioritySize = 0;

    for (const auto &pair : list) {
        emit failed(pair.first, u"aborted"_qs);
        pair.first->release();
    }
}

void BatchedAffiliateAPIPGRetriever::setApiKey(const QString &key)
{
    if ((key != m_apiKey) && !key.isEmpty())
        m_apiKey = key;
}

void BatchedAffiliateAPIPGRetriever::check()
{
    if (m_currentBatch.isEmpty() && !m_currentJob) {
        if (m_nextBatch.isEmpty() && !m_wrongVatTypeQueue.isEmpty()) {
            // switch vatType to the request with the highest priority
            m_nextBatchVatType = m_wrongVatTypeQueue.constFirst().first->vatType();

            // move all requests with the same vatType to nextBatch
            qsizetype highPrioMoved = 0;
            for (auto i = 0; i < m_wrongVatTypeQueue.size(); ++i) {
                auto [pg, age] = m_wrongVatTypeQueue.at(i);
                if (pg->vatType() == m_nextBatchVatType) {
                    if (i < m_wrongVatTypeQueuePrioritySize)
                        ++highPrioMoved;
                    m_nextBatch.emplace_back(pg, age);
                    // just tag now, as we can't remove directly: this would mess up the indices
                    m_wrongVatTypeQueue[i].first = nullptr;
                }
            }
            m_nextBatchPrioritySize = highPrioMoved;
            m_wrongVatTypeQueuePrioritySize -= highPrioMoved;
            // finally remove all the tagged entries
            m_wrongVatTypeQueue.removeIf([](const auto &pair) { return pair.first == nullptr; });
        }

        qsizetype nextSize = m_nextBatch.size();
        qint64 nextAge = (m_nextBatchPrioritySize >= nextSize)
                ? 0 : m_nextBatch.at(m_nextBatchPrioritySize).second.elapsed();
        qint64 nextPriorityAge = (m_nextBatchPrioritySize <= 0)
                ? 0 : m_nextBatch.at(0).second.elapsed();

        if ((nextSize >= MaxBatchSize) || (qMax(nextAge, nextPriorityAge) > MaxBatchAgeMSec)) {
            auto batchSize = qMin(nextSize, MaxBatchSize);
            QJsonArray array;

            for (auto i = 0; i < batchSize; ++i) {
                auto *pg = m_nextBatch.at(i).first;
                const QString itemId = QString::fromLatin1(pg->item()->id());
                const QString typeId = itemTypeApiId(pg->item()->itemType());
                int colorId = int(pg->color()->id());

                m_currentBatch.append(pg);

                array.append(QJsonObject {
                                 { u"color_id"_qs, colorId },
                                 { u"item"_qs, QJsonObject {
                                       { u"no"_qs, itemId },
                                       { u"type"_qs, typeId },
                                   } },
                             });
            }
            m_nextBatch.remove(0, batchSize);
            m_nextBatchPrioritySize -= qMin(m_nextBatchPrioritySize, batchSize);

            const auto json = QJsonDocument(array).toJson(QJsonDocument::Compact);

            // https://api.bricklink.com/api/affiliate/v1/price_guide_batch?currency_code=USD&precision=4&vat_type=1&api_key=...
            QUrl url = u"https://api.bricklink.com/api/affiliate/v1/price_guide_batch"_qs;
            url.setQuery({
                { u"currency_code"_qs, u"USD"_qs },
                { u"precision"_qs,     u"4"_qs },
                { u"vat_type"_qs,      QString::number(int(m_nextBatchVatType)) },
                { u"api_key"_qs,       m_apiKey }
            });

            m_currentJob = TransferJob::postContent(url, u"application/json"_qs, json);
            m_currentJob->setUserData("batchedPriceGuide", true);

            m_core->retrieve(m_currentJob, m_nextBatchPrioritySize > 0);
        } else if (nextSize) {
            auto nextCheck = std::max(0LL, (MaxBatchAgeMSec - std::max(nextAge, nextPriorityAge)));
            m_batchTimer->setInterval(int(nextCheck));
            m_batchTimer->start();
        }
    }
}

void BatchedAffiliateAPIPGRetriever::transferJobFinished(TransferJob *j)
{
    Q_ASSERT(m_currentJob == j);
    m_currentJob = nullptr;

    try {
        if (j->isCompleted() && j->data()) {
            QJsonParseError err;
            const auto doc = QJsonDocument::fromJson(*j->data(), &err);
            if (doc.isNull())
                throw ParseException("invalid JSON: %1 at %2").arg(err.errorString()).arg(err.offset);

            const auto meta = doc[u"meta"];
            const int code = meta[u"code"].toInt();
            const QString description = meta[u"description"].toString();
            const QString message = meta[u"message"].toString();

            if (!meta.isObject() || (code != 200) || (description != u"OK") || (message != u"OK"))
                throw Exception("bad request (%1). %2: %3").arg(code).arg(description).arg(message);

            const auto data = doc[u"data"].toArray();
            if (data.size() != m_currentBatch.size()) {
                throw Exception("JSON data size mismatch: requested %1, got %2")
                    .arg(m_currentBatch.size()).arg(data.size());
            }
            for (const auto &d : data) {
                const auto item = d.toObject();
                const QString itemId = item[u"item"][u"no"].toString();
                const QString typeId = item[u"item"][u"type"].toString();
                const int colorId = item[u"color_id"].toInt();

                auto pit = std::find_if(m_currentBatch.begin(), m_currentBatch.end(),
                                        [&](const auto *pg) {
                    return pg && (QLatin1String(pg->item()->id()) == itemId)
                            && (itemTypeApiId(pg->item()->itemType()) == typeId)
                            && (pg->color()->id() == uint(colorId));
                });

                if (pit == m_currentBatch.end()) {
                    qCWarning(LogCache) << "PG download was not requested, but received for"
                                        << typeId.mid(0, 1) << itemId << "in" << colorId;
                    continue;
                }

                PriceGuide::Data pgdata;
                auto parsePGJson = [&](QStringView key, int time, int cond) {
                    auto obj = item[key].toObject();
                    pgdata.quantities[time][cond] = obj[u"unit_quantity"].toInt();
                    pgdata.lots[time][cond] = obj[u"total_quantity"].toInt();
                    pgdata.prices[time][cond][int(Price::Lowest)] = obj[u"min_price"].toString().toDouble();
                    pgdata.prices[time][cond][int(Price::Highest)] = obj[u"max_price"].toString().toDouble();
                    pgdata.prices[time][cond][int(Price::Average)] = pgdata.lots[time][cond]
                            ? (obj[u"total_price"].toString().toDouble() / pgdata.lots[time][cond]) : 0;
                    pgdata.prices[time][cond][int(Price::WAverage)] =  pgdata.quantities[time][cond]
                            ? (obj[u"total_qty_price"].toString().toDouble() / pgdata.quantities[time][cond]) : 0;
                };

                parsePGJson(u"inventory_new",  int(Time::Current), int(Condition::New));
                parsePGJson(u"inventory_used", int(Time::Current), int(Condition::Used));
                parsePGJson(u"ordered_new",    int(Time::PastSix), int(Condition::New));
                parsePGJson(u"ordered_used",   int(Time::PastSix), int(Condition::Used));

                emit finished(*pit, pgdata);
                (*pit)->release();

                *pit = nullptr;  // mark as "dealt with"
            }

            // Make sure to fail any remaining pg requests that might still be in m_currentBatch.
            // Ideally there are none, but throwing this Exception unconditionally doesn't hurt.
            throw Exception("no reply received for request");

        } else if (j->isAborted()) {
            throw Exception("aborted");
        } else {
            throw Exception(j->errorString() + u'(' + QString::number(j->responseCode()) + u')');
        }
    } catch (const Exception &e) {
        for (auto *pg : std::as_const(m_currentBatch)) {
            if (pg) {
                emit failed(pg, u"PG download for " + QChar::fromLatin1(pg->item()->itemType()->id())
                                    + u' ' + QString::fromLatin1(pg->item()->id()) + u" in "
                            + pg->color()->name() + u" failed: " + e.errorString());
                pg->release();
            }
        }
    }

    m_currentBatch.clear();

    QMetaObject::invokeMethod(this, &BatchedAffiliateAPIPGRetriever::check, Qt::QueuedConnection);
}

QString BatchedAffiliateAPIPGRetriever::itemTypeApiId(const ItemType *itt)
{
    static const QHash<char, QString> mapping = {
        { 'B', u"BOOK"_qs },
        { 'C', u"CATALOG"_qs },
        { 'G', u"GEAR"_qs },
        { 'I', u"INSTRUCTION"_qs },
        { 'M', u"MINIFIG"_qs },
        { 'O', u"ORIGINAL_BOX"_qs },
        { 'P', u"PART"_qs },
        { 'S', u"SET"_qs },
    };
    return mapping.value(itt ? itt->id() : 0);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


PriceGuideCache::PriceGuideCache(Core *core)
    : QObject(core)
    , d(new PriceGuideCachePrivate)
{
    d->q = this;
    d->m_core = core;

    Q_ASSERT(!PriceGuide::s_cache);
    PriceGuide::s_cache = this;

    d->m_cacheStatId = AppStatistics::inst()->addSource(u"Price-guides in memory cache"_qs);
    d->m_loadsStatId = AppStatistics::inst()->addSource(u"Price-guides queued for disk load"_qs);
    d->m_savesStatId = AppStatistics::inst()->addSource(u"Price-guides queued for disk save"_qs);

    d->m_cache.setMaxCost(5000); // each price guide has a cost of 1

    QString batchApiKey = core->apiKey("affiliate");
    auto affiliate = new BatchedAffiliateAPIPGRetriever(core, batchApiKey);
    connect(core->database(), &Database::updateFinished,
            affiliate, [this, affiliate](bool success, const QString &) {
        if (success)
            affiliate->setApiKey(d->m_core->apiKey("affiliate"));
    });
    d->m_retriever = affiliate;

    // this is the old retriever:
    // d->m_retriever = new SingleHTMLScrapePGRetriever(core);

    qInfo() << "Using BrickLink price-guide retriever plugin:" << d->m_retriever->name();

    connect(d->m_retriever, &PriceGuideRetrieverInterface::finished,
            this, [this](PriceGuide *pg, const PriceGuide::Data &data) {
        d->retrieveFinished(pg, data);
    });
    connect(d->m_retriever, &PriceGuideRetrieverInterface::failed,
            this, [this](PriceGuide *pg, const QString &errorString) {
        d->retrieveFailed(pg, errorString);
    });

    d->m_dbName = core->dataPath() + u"priceguide_cache.sqlite"_qs;
    d->m_db = QSqlDatabase::addDatabase(u"QSQLITE"_qs, u"PriceGuideCache"_qs);
    d->m_db.setDatabaseName(d->m_dbName);

    // try to start from scratch, if the DB open fails
    if (!d->m_db.open() &&
            !(QFile::exists(d->m_dbName) && QFile::remove(d->m_dbName) && d->m_db.open())) {
        qCWarning(LogSql) << "Failed to open the price-guide database:" << d->m_db.lastError().text();
    }

    if (d->m_db.isOpen()) {
        QSqlQuery createQuery(d->m_db);
        if (!createQuery.exec(
                    u"CREATE TABLE IF NOT EXISTS pg ("
                    "id TEXT NOT NULL PRIMARY KEY, "
                    "updated INTEGER, "             // msecsSinceEpoch
                    "accessed INTEGER NOT NULL, "   // msecsSinceEpoch
                    "data BLOB) WITHOUT ROWID;"_qs)) {
            qCWarning(LogSql) << "Failed to create the 'pg' table in the price-guide database:"
                       << createQuery.lastError().text();
            d->m_db.close();
        }
    }

    if (d->m_db.isOpen()) {
        static constexpr int DBVersion = 1;

        {
            QSqlQuery jnlQuery(u"PRAGMA journal_mode = wal;"_qs, d->m_db);
            if (jnlQuery.lastError().isValid())
                qCWarning(LogSql) << "Failed to set journaling mode to 'wal' on the price-guide database:"
                                  << jnlQuery.lastError();
        }
        {
            QSqlQuery uvQuery(u"PRAGMA user_version"_qs, d->m_db);
            uvQuery.next();
            auto userVersion = uvQuery.value(0).toInt();
            if (userVersion == 0) // brand new file, bump version
                QSqlQuery(u"PRAGMA user_version=%1"_qs.arg(DBVersion), d->m_db);
        }

        // DB schema upgrade code goes here...
    }

    for (int i = 0; i < 1; ++i) // one writer should be enough
        d->m_threads.append(QThread::create(&PriceGuideCachePrivate::saveThread, d, d->m_db.connectionName(), i));
    for (int i = 0; i < QThread::idealThreadCount(); ++i)
        d->m_threads.append(QThread::create(&PriceGuideCachePrivate::loadThread, d, d->m_db.connectionName(), i));

    for (auto *thread : d->m_threads)
        thread->start(QThread::LowPriority);
}

PriceGuideCache::~PriceGuideCache()
{
    d->m_stop = true;
    d->m_loadMutex.lock();
    d->m_loadTrigger.wakeAll();
    d->m_loadMutex.unlock();
    d->m_saveMutex.lock();
    d->m_saveTrigger.wakeAll();
    d->m_saveMutex.unlock();
    for (auto *thread : d->m_threads) {
        thread->wait();
        delete thread;
    }
    d->m_db.close();
    delete d;
    PriceGuide::s_cache = nullptr;
}

void PriceGuideCache::setUpdateInterval(int interval)
{
    d->m_updateInterval = interval;
}

void PriceGuideCache::clearCache()
{
    int run = 0;
    int lastLeftOver = 0;
    QElapsedTimer timer;

    // the loader/saver threads might hold references, so we need to wait for their queues to drain
    while (true) {
        ++run;
        int leftOver = d->m_cache.clearRecursive();

        if (!leftOver) {
            break;
        } else if ((leftOver == lastLeftOver) && (timer.elapsed() > 500)) {
            qCCritical(LogCache) << "PriceGuide cache: still" << leftOver
                                 << "active references - giving up, expect a crash soon";
            break;
        }

        qCWarning(LogCache) << "PriceGuide cache:" << leftOver
                            << "objects still have a reference after clearing run" << run;
        if (lastLeftOver != leftOver) {
            lastLeftOver = leftOver;
            timer.start();
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 500);
    }

    AppStatistics::inst()->update(d->m_cacheStatId, d->m_cache.count());
}

QPair<int, int> PriceGuideCache::cacheStats() const
{
    return qMakePair(d->m_cache.totalCost(), d->m_cache.maxCost());
}

PriceGuide *PriceGuideCache::priceGuide(const Item *item, const Color *color, bool highPriority)
{
    return priceGuide(item, color, currentVatType(), highPriority);
}

PriceGuide *PriceGuideCache::priceGuide(const Item *item, const Color *color, VatType vatType,
                                        bool highPriority)
{
    if (!item || !color || !supportedVatTypes().contains(vatType))
        return nullptr;

    auto key = PriceGuideCachePrivate::cacheKey(item, color, vatType);
    PriceGuide *pg = d->m_cache[key];

    bool needToLoad = !pg || (!pg->isValid() && (pg->updateStatus() == UpdateStatus::UpdateFailed));

    if (!pg) {
        pg = new PriceGuide(item, color, vatType);
        if (!d->m_cache.insert(key, pg)) {
            qCWarning(LogCache, "Can not add price guide to cache (cache max/cur: %d/%d, cost: %d)",
                      int(d->m_cache.maxCost()), int(d->m_cache.totalCost()), 1);
            return nullptr;
        }
        AppStatistics::inst()->update(d->m_cacheStatId, d->m_cache.count());
    }

    if (needToLoad) {
        pg->setUpdateStatus(UpdateStatus::Loading);
        d->load(pg, highPriority);

        //TODO re-prioritize?
    }

    return pg;
}

void PriceGuideCache::updatePriceGuide(PriceGuide *pg, bool highPriority)
{
    if (!pg || (pg->m_updateStatus == UpdateStatus::Updating))
        return;

    if (QNetworkInformation::instance()
        && (QNetworkInformation::instance()->reachability() != QNetworkInformation::Reachability::Online)) {
        pg->setUpdateStatus(UpdateStatus::UpdateFailed);
        emit priceGuideUpdated(pg);
        return;
    }

    if (pg->m_updateStatus == UpdateStatus::Loading) {
        pg->m_updateAfterLoad = true;
        return;
    }

    pg->setUpdateStatus(UpdateStatus::Updating);

    d->m_retriever->fetch(pg, highPriority);
}

void PriceGuideCache::cancelPriceGuideUpdate(PriceGuide *pg)
{
    d->m_retriever->cancel(pg);
}

void PriceGuideCache::cancelAllPriceGuideUpdates()
{
    d->m_retriever->cancelAll();
}

QString PriceGuideCache::retrieverName() const
{
    return d->m_retriever->name();
}

QString PriceGuideCache::retrieverId() const
{
    return d->m_retriever->id();
}

QVector<VatType> PriceGuideCache::supportedVatTypes() const
{
    return d->m_retriever->supportedVatTypes();
}

bool PriceGuideCache::setCurrentVatType(VatType vatType)
{
    bool changed = supportedVatTypes().contains(vatType);
    if (changed) {
        d->m_vatType[d->m_retriever->id()] = vatType;
        emit currentVatTypeChanged(vatType);
    }
    return changed;
}

VatType PriceGuideCache::currentVatType() const
{
    return d->m_vatType.value(d->m_retriever->id(), VatType::Excluded);
}

QIcon PriceGuideCache::iconForVatType(VatType vatType)
{
    switch (vatType) {
    case VatType::Excluded: return QIcon::fromTheme(u"vat-excluded"_qs);
    case VatType::EU      : return QIcon::fromTheme(u"vat-included-eu"_qs);
    case VatType::UK      : return QIcon::fromTheme(u"vat-included-uk"_qs);
    case VatType::Norway  : return QIcon::fromTheme(u"vat-included-no"_qs);
    case VatType::Included:
    default               : return QIcon::fromTheme(u"vat-included"_qs);
    }
}

QString PriceGuideCache::descriptionForVatType(VatType vatType)
{
    QString inc = tr("VAT is included");
    QString exc = tr("VAT is excluded");

    switch (vatType) {
    case VatType::Excluded: return exc;
    case VatType::EU      : return inc + u" (" + tr("for the EU") + u")";
    case VatType::UK      : return inc + u" (" + tr("for the UK") + u")";
    case VatType::Norway  : return inc + u" (" + tr("for Norway") + u")";
    case VatType::Included:
    default               : return inc;
    }
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


quint64 PriceGuideCachePrivate::cacheKey(const Item *item, const Color *color, VatType vatType)
{
    // 24 bit reserved | 8 bit vat-type | 11 bit color-index | 21 bit item-index
    return (quint64(quint8(vatType)) << 32)
            | (quint64(color ? (color->index() + 1) : 0) << 21)
            | (quint64(item ? (item->index() + 1) : 0));
}

QString PriceGuideCachePrivate::databaseTag(PriceGuide *pg, PriceGuideRetrieverInterface *retriever)
{
    if (!pg || !pg->item() || !pg->color() || !retriever)
        return { };

    return QChar::fromLatin1(pg->item()->itemTypeId()) + QString::fromLatin1(pg->item()->id())
            + u'@' + QString::number(pg->color()->id())
            + u'@' + retriever->id() + QString::number(int(pg->vatType()));
}

bool PriceGuideCachePrivate::isUpdateNeeded(PriceGuide *pg) const
{
    return (m_updateInterval > 0)
            && (!pg->isValid()
                || (pg->lastUpdated().secsTo(QDateTime::currentDateTime()) > m_updateInterval));
}

void PriceGuideCachePrivate::load(PriceGuide *pg, bool highPriority)
{
    if (!pg)
        return;

    pg->addRef();
    m_loadMutex.lock();
    m_loadQueue.insert(highPriority ? 0 : m_loadQueue.size(),
                       { pg, highPriority ? LoadHighPriority : LoadLowPriority });
    m_loadTrigger.wakeOne();
    auto queueSize = m_loadQueue.size();
    m_loadMutex.unlock();

    AppStatistics::inst()->update(m_loadsStatId, queueSize);
}

void PriceGuideCachePrivate::save(PriceGuide *pg)
{
    if (!pg)
        return;

    pg->addRef();
    m_saveMutex.lock();
    m_saveQueue.append({ pg, SaveData });
    m_saveTrigger.wakeOne();
    auto queueSize = m_saveQueue.size();
    m_saveMutex.unlock();

    AppStatistics::inst()->update(m_savesStatId, queueSize);
}

void PriceGuideCachePrivate::loadThread(QString dbName, int index)
{
    auto db = QSqlDatabase::cloneDatabase(dbName, dbName + u"_Reader_" + QString::number(index));
    db.open();

    QSqlQuery loadQuery(db);
    loadQuery.prepare(u"SELECT updated,data FROM pg WHERE id=:id;"_qs);

    while (!m_stop) {
        QMutexLocker locker(&m_loadMutex);
        if (m_loadQueue.isEmpty())
            m_loadTrigger.wait(&m_loadMutex);

        if (m_stop) {
            for (auto [pg, type] : m_loadQueue)
                pg->release();
            continue;
        }

        // alternate between loading and saving in order to not starve one queue
        if (!m_loadQueue.isEmpty()) {
            auto [pg, loadType] = m_loadQueue.takeFirst();
            auto queueSize = m_loadQueue.size();
            locker.unlock();

            AppStatistics::inst()->update(m_loadsStatId, queueSize);

            bool loaded = false;
            QDateTime lastUpdated;
            QByteArray data;
            bool highPriority = (loadType == LoadHighPriority);

            if (db.isOpen()) {
                loadQuery.bindValue(u":id"_qs, databaseTag(pg, m_retriever));

                loadQuery.exec();
                if (loadQuery.next()) {
                    lastUpdated = loadQuery.isNull(0) ? QDateTime()
                                                      : QDateTime::fromMSecsSinceEpoch(loadQuery.value(0).toLongLong());
                    data = loadQuery.value(1).toByteArray();
                    loaded = data.isEmpty() || (data.size() == sizeof(PriceGuide::Data));
                }
                loadQuery.finish();
            }
            pg->addRef(); // the release will happen on the main thread (see the invokeMethod below)
            QMetaObject::invokeMethod(m_core, [=, this, pg=pg]() { // clang bug: P1091R3
                if (loaded) {
                    pg->setLastUpdated(lastUpdated);
                    std::memcpy(&pg->m_data, data, sizeof(PriceGuide::Data));

                    // update the last accessed time stamp
                    pg->addRef();
                    m_saveMutex.lock();
                    m_saveQueue.append({ pg, SaveAccessTimeOnly });
                    m_saveTrigger.wakeOne();
                    m_saveMutex.unlock();
                }
                pg->setIsValid(loaded);
                pg->setUpdateStatus(UpdateStatus::Ok);

                if (pg->m_updateAfterLoad || isUpdateNeeded(pg))  {
                    pg->m_updateAfterLoad = false;
                    q->updatePriceGuide(pg, highPriority);
                }
                if (loaded && data.isEmpty())
                    pg->setIsValid(false);

                emit q->priceGuideUpdated(pg);
                pg->release();
            }, Qt::QueuedConnection);

            pg->release();
        }
    }
    db.close();
}


void PriceGuideCachePrivate::saveThread(QString dbName, int index)
{
    auto db = QSqlDatabase::cloneDatabase(dbName, dbName + u"_Writer_" + QString::number(index));
    db.open();

    QSqlQuery saveQuery(db);
    saveQuery.prepare(u"INSERT INTO pg(id,updated,accessed,data) VALUES(:id,:updated,:accessed,:data) "
                      "ON CONFLICT(id) DO UPDATE "
                      "SET updated=excluded.updated,accessed=excluded.accessed,data=excluded.data;"_qs);

    QSqlQuery accessQuery(db);
    accessQuery.prepare(u"UPDATE pg SET accessed=:accessed WHERE id=:id;"_qs);

    while (!m_stop) {
        QMutexLocker locker(&m_saveMutex);
        if (m_saveQueue.isEmpty())
            m_saveTrigger.wait(&m_saveMutex);

        if (!m_saveQueue.isEmpty()) {
            // we might have multiple saver threads, so don't grab the full queue at once
            const auto saveQueueCopy = m_saveQueue.mid(0, 10);
            m_saveQueue.remove(0, saveQueueCopy.size());
            auto queueSize = m_saveQueue.size();
            locker.unlock();

            AppStatistics::inst()->update(m_savesStatId, queueSize);

            if (db.isOpen()) {
                db.transaction();

                qint64 now = QDateTime::currentMSecsSinceEpoch();

                for (auto [pg, saveType] : saveQueueCopy) {
                    auto dbTag = databaseTag(pg, m_retriever);

                    if (saveType == SaveAccessTimeOnly) {
                        accessQuery.bindValue(u":id"_qs, dbTag);
                        accessQuery.bindValue(u":accessed"_qs, now);
                        if (!accessQuery.exec()) {
                            qCWarning(LogSql) << "Failed to update the access time of a price-guide:"
                                              << accessQuery.lastError().text();
                        }
                        accessQuery.finish();
                    } else {
                        auto lastUpdated = QVariant(QMetaType::fromType<qint64>());
                        if (pg->lastUpdated().isValid())
                            lastUpdated = QVariant::fromValue(pg->lastUpdated().toMSecsSinceEpoch());

                        saveQuery.bindValue(u":id"_qs, dbTag);
                        saveQuery.bindValue(u":updated"_qs, lastUpdated);
                        saveQuery.bindValue(u":accessed"_qs, now);
                        saveQuery.bindValue(u":data"_qs, QByteArray::fromRawData(reinterpret_cast<const char *>(&pg->m_data),
                                                                                 sizeof(PriceGuide::Data)));
                        if (!saveQuery.exec()) {
                            qCWarning(LogSql) << "Failed to save price-guide data:"
                                              << saveQuery.lastError().text();
                        }
                        saveQuery.finish();
                    }
                    pg->release();
                }
                db.commit();
            }
        }
    }
    db.close();
}

void PriceGuideCachePrivate::retrieveFinished(PriceGuide *pg, const PriceGuide::Data &data)
{
    pg->setLastUpdated(QDateTime::currentDateTime());
    pg->m_data = data;

    save(pg);

#if 0
    qInfo().noquote() << "PG for" << pg->item()->itemTypeId() << pg->item()->id() << "in"
                      << pg->color()->name() << " |  VAT:" << pg->m_vatType << " |  retriever:" << m_retriever->name();
    for (int t = 0; t < 2; ++t) {
        for (int c = 0; c < 2; ++c) {
            static const char *cond[2] = { "New: ", "Used:" };
            static const char *time[2] = { "PastSix", "Current" };

            qInfo().noquote() << time[t] << cond[c] << u"%1 (%2) %3 %4 %5 %6"_qs
                                 .arg(data.quantities[t][c], 7).arg(data.lots[t][c], 7)
                                 .arg(data.prices[t][c][0], 7, 'f', 4)
                    .arg(data.prices[t][c][1], 8, 'f', 4)
                    .arg(data.prices[t][c][2], 8, 'f', 4)
                    .arg(data.prices[t][c][3], 8, 'f', 4);
        }
    }
#endif

    pg->setIsValid(true);
    pg->setUpdateStatus(UpdateStatus::Ok);
    emit q->priceGuideUpdated(pg);
}

void PriceGuideCachePrivate::retrieveFailed(PriceGuide *pg, const QString &errorString [[maybe_unused]])
{
    qCWarning(LogCache).noquote() << errorString;
    pg->setUpdateStatus(UpdateStatus::UpdateFailed);
    emit q->priceGuideUpdated(pg);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype PriceGuide
    \inqmlmodule BrickLink
    \ingroup qml-api
    \brief This value type represents the price guide for a BrickLink item.

    Each price guide of an item in the BrickLink catalog is available as a PriceGuide object.

    You cannot create PriceGuide objects yourself, but you can retrieve a PriceGuide object given the
    item and color id via BrickLink::priceGuide().

    \note PriceGuides aren't readily available, but need to be asynchronously loaded (or even
          downloaded) at runtime. You need to connect to the signal BrickLink::priceGuideUpdated()
          to know when the data has been loaded.

    The following three enumerations are used to retrieve the price guide data from this object:

    \b Time
    \value BrickLink.Time.PastSix   The sales in the last six months.
    \value BrickLink.Time.Current   The items currently for sale.

    \b Condition
    \value BrickLink.Condition.New       Only items in new condition.
    \value BrickLink.Condition.Used      Only items in used condition.

    \b Price
    \value BrickLink.Price.Lowest    The lowest price.
    \value BrickLink.Price.Average   The average price.
    \value BrickLink.Price.WAverage  The weighted average price.
    \value BrickLink.Price.Highest   The highest price.

*/
/*! \qmlproperty bool PriceGuide::isNull
    \readonly
    Returns whether this PriceGuide is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty Item PriceGuide::item
    \readonly
    The BrickLink item reference this price guide is requested for.
*/
/*! \qmlproperty Color PriceGuide::color
    \readonly
    The BrickLink color reference this price guide is requested for.
*/
/*! \qmlproperty date PriceGuide::lastUpdated
    \readonly
    Holds the time stamp of the last successful update of this price guide.
*/
/*! \qmlproperty UpdateStatus PriceGuide::updateStatus
    \readonly
    Returns the current update status. The available values are:
    \value BrickLink.UpdateStatus.Ok            The last picture load (or download) was successful.
    \value BrickLink.UpdateStatus.Loading       BrickStore is currently loading the picture from the local cache.
    \value BrickLink.UpdateStatus.Updating      BrickStore is currently downloading the picture from BrickLink.
    \value BrickLink.UpdateStatus.UpdateFailed  The last download from BrickLink failed. isValid might still be
                                                \c true, if there was a valid picture available before the failed
                                                update!
*/
/*! \qmlproperty bool PriceGuide::isValid
    \readonly
    Returns whether this object currently holds valid price guide data.
*/
/*! \qmlmethod PriceGuide::update(bool highPriority = false)
    Tries to re-download the price guide from the BrickLink server. If you set \a highPriority to \c
    true the load/download request will be prepended to the work queue instead of appended.
*/
/*! \qmlmethod int PriceGuide::quantity(Time time, Condition condition)
    Returns the number of items for sale (or item that have been sold) given the \a time frame and
    \a condition. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time and
    Condition enumerations.
*/
/*! \qmlmethod int PriceGuide::lots(Time time, Condition condition)
    Returns the number of lots for sale (or lots that have been sold) given the \a time frame and
    \a condition. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time and
    Condition enumerations.
*/
/*! \qmlmethod real PriceGuide::price(Time time, Condition condition, Price price)
    Returns the price of items for sale (or item that have been sold) given the \a time frame,
    \a condition and \a price type. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time,
    Condition and Price enumerations.
*/

} // namespace BrickLink
