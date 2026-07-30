// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QByteArray>
#include <QtCore/QAtomicInt>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QVector>
#include <QtSql/QSqlDatabase>

#include "utility/cache.h"
#include "global.h"
#include "priceguide.h"

QT_FORWARD_DECLARE_CLASS(QTimer)

class TransferJob;


namespace BrickLink {

class PriceGuideRetrieverInterface : public QObject
{
    Q_OBJECT

public:
    PriceGuideRetrieverInterface(QObject *parent = nullptr);

    virtual QString name() const = 0;
    virtual QString id() const = 0;

    virtual QVector<VatType> supportedVatTypes() const = 0;

    virtual void fetch(const PriceGuideRef &pg, bool highPriority) = 0;
    virtual void cancel(const PriceGuideRef &pg) = 0;
    virtual void cancelAll() = 0;

signals:
    void finished(const BrickLink::PriceGuideRef &pg, const BrickLink::PriceGuide::Data &data);
    void failed(const BrickLink::PriceGuideRef &pg, const QString &errorString);
};

class SingleHTMLScrapePGRetriever : public PriceGuideRetrieverInterface
{
    Q_OBJECT

public:
    SingleHTMLScrapePGRetriever(Core *core);

    QString name() const override { return u"Single HTML Scrape"_qs; }
    QString id() const override { return u"S"_qs; }

    QVector<VatType> supportedVatTypes() const override;

    void fetch(const PriceGuideRef &pg, bool highPriority) override;
    void cancel(const PriceGuideRef &pg) override;
    void cancelAll() override;

private:
    void transferJobFinished(TransferJob *j, const PriceGuideRef &pg);
    static bool parseHtml(const QByteArray &ba, PriceGuide::Data &result);

    Core *m_core = nullptr;
    QHash<PriceGuide *, TransferJob *> m_jobs;
};


class BatchedAffiliateAPIPGRetriever : public PriceGuideRetrieverInterface
{
    Q_OBJECT

public:
    BatchedAffiliateAPIPGRetriever(Core *core, const QString &apiKey);

    QString name() const override { return u"Batched Affiliate API"_qs; }
    QString id() const override { return u"B"_qs; }

    QVector<VatType> supportedVatTypes() const override;

    void fetch(const PriceGuideRef &pg, bool highPriority) override;
    void cancel(const PriceGuideRef &pg) override;
    void cancelAll() override;

    void setApiKey(const QString &key);

    static constexpr qsizetype MaxBatchSize = 500;
    static constexpr qint64 MaxBatchAgeMSec = 100;

private:
    void check();
    void transferJobFinished(TransferJob *j);
    static QString itemTypeApiId(const ItemType *itt);

    Core *m_core = nullptr;
    QVector<PriceGuideRef> m_currentBatch;
    TransferJob *m_currentJob = nullptr;
    QVector<std::pair<PriceGuideRef, QElapsedTimer>> m_nextBatch;
    VatType m_nextBatchVatType = VatType::Excluded;
    qsizetype m_nextBatchPrioritySize = 0;
    QTimer *m_batchTimer;
    QVector<std::pair<PriceGuideRef, QElapsedTimer>> m_wrongVatTypeQueue;
    qsizetype m_wrongVatTypeQueuePrioritySize = 0;
    QString m_apiKey;
};


class PriceGuideCachePrivate
{
public:
    PriceGuideRetrieverInterface *m_retriever;
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

    QVector<std::pair<PriceGuideRef, LoadType>> m_loadQueue;
    QVector<std::pair<PriceGuideRef, SaveType>> m_saveQueue;
    QString m_dbName;
    QSqlDatabase m_db;
    QVector<QThread *> m_threads;

    int m_updateInterval = 0;
    QMap<QString, VatType> m_vatType;  // key: retriever->id()
    Cache<quint64, PriceGuide> m_cache;
    Core *m_core;
    PriceGuideCache *q;
    int m_cacheStatId = -1;
    int m_loadsStatId = -1;
    int m_savesStatId = -1;

    static quint64 cacheKey(const Item *item, const Color *color, VatType vatType);
    static QString databaseTag(const PriceGuide *pg, PriceGuideRetrieverInterface *retriever);
    bool isUpdateNeeded(const PriceGuide *pg) const;

    void load(PriceGuideRef pg, bool highPriority);
    void save(PriceGuideRef pg);
    void loadThread(QString dbName, int index);
    void saveThread(QString dbName, int index);

    void retrieveFinished(const PriceGuideRef &pg, const PriceGuide::Data &data);
    void retrieveFailed(const PriceGuideRef &pg, const QString &errorString);
};

} // namespace BrickLink
