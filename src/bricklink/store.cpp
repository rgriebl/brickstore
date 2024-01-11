// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QUrl>
#include <QUrlQuery>

#include "utility/transfer.h"
#include "utility/exception.h"
#include "bricklink/core.h"
#include "bricklink/io.h"
#include "bricklink/store.h"


BrickLink::Store::Store(Core *core)
    : QObject(core)
    , m_core(core)
{
    connect(core, &Core::authenticatedTransferStarted,
            this, [this](TransferJob *job) {
        if ((m_updateStatus == UpdateStatus::Updating) && (m_job == job))
            emit updateStarted();
    });
    connect(core, &Core::authenticatedTransferProgress,
            this, [this](TransferJob *job, int progress, int total) {
        if ((m_updateStatus == UpdateStatus::Updating) && (m_job == job))
            emit updateProgress(progress, total);
    });
    connect(core, &Core::authenticatedTransferFinished,
            this, [this](TransferJob *job) {
        if ((m_updateStatus == UpdateStatus::Updating) && (m_job == job)) {
            bool success = job->isCompleted() && (job->responseCode() == 200) && job->data();
            QString message;
            if (success) {
                try {
                    auto result = IO::fromBrickLinkXML(*job->data(), IO::Hint::Store);
                    m_lots = result.takeLots();
                    if (result.currencyCode() != m_currencyCode) {
                        m_currencyCode = result.currencyCode();
                        emit currencyCodeChanged(m_currencyCode);
                    }
                } catch (const Exception &e) {
                    success = false;
                    message = tr("Failed to import the store inventory") + u":<br><br>" + e.errorString();
                }
                if (success != m_valid) {
                    m_valid = success;
                    emit isValidChanged(success);
                }
            } else {
                message = tr("Failed to download the store inventory") + u": " + job->errorString();
            }
            setUpdateStatus(success ? UpdateStatus::Ok : UpdateStatus::UpdateFailed);
            setLastUpdated(QDateTime::currentDateTime());
            emit updateFinished(success, message);
            m_job = nullptr;
        }
    });
}

BrickLink::Store::~Store()
{
    qDeleteAll(m_lots);
}

void BrickLink::Store::setUpdateStatus(UpdateStatus updateStatus)
{
    if (updateStatus != m_updateStatus) {
        m_updateStatus = updateStatus;
        emit updateStatusChanged(updateStatus);
    }
}

void BrickLink::Store::setLastUpdated(const QDateTime &lastUpdated)
{
    if (lastUpdated != m_lastUpdated) {
        m_lastUpdated = lastUpdated;
        emit lastUpdatedChanged(lastUpdated);
    }
}

bool BrickLink::Store::startUpdate()
{
    if (updateStatus() == UpdateStatus::Updating)
        return false;
    Q_ASSERT(!m_job);
    setUpdateStatus(UpdateStatus::Updating);

    QUrl url(u"https://www.bricklink.com/invExcelFinal.asp"_qs);
    QUrlQuery query;
    query.addQueryItem(u"itemType"_qs,      { });
    query.addQueryItem(u"catID"_qs,         { });
    query.addQueryItem(u"colorID"_qs,       { });
    query.addQueryItem(u"invNew"_qs,        { });
    query.addQueryItem(u"itemYear"_qs,      { });
    query.addQueryItem(u"viewType"_qs,      u"x"_qs);    // XML
    query.addQueryItem(u"invStock"_qs,      u"Y"_qs);
    query.addQueryItem(u"invStockOnly"_qs,  { });
    query.addQueryItem(u"invQty"_qs,        { });
    query.addQueryItem(u"invQtyMin"_qs,     u"0"_qs);
    query.addQueryItem(u"invQtyMax"_qs,     u"0"_qs);
    query.addQueryItem(u"invBrikTrak"_qs,   { });
    query.addQueryItem(u"invDesc"_qs,       { });
    url.setQuery(query);

    m_job = TransferJob::post(url);
    m_core->retrieveAuthenticated(m_job);
    return true;
}

void BrickLink::Store::cancelUpdate()
{
    if ((m_updateStatus == UpdateStatus::Updating) && m_job)
        m_job->abort();
}

#include "moc_store.cpp"
