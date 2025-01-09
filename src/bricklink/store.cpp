// Copyright (C) 2004-2025 Robert Griebl
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
            bool success = job->isCompleted() && (job->responseCode() == 200);
            QString message;
            if (success) {
                try {
                    auto result = IO::fromBrickLinkXML(job->data(), IO::Hint::Store);
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

    m_job = TransferJob::post(u"https://www.bricklink.com/invExcelFinal.asp"_qs,
                              {
                               { u"itemType"_qs,      { } },
                               { u"catID"_qs,         { } },
                               { u"colorID"_qs,       { } },
                               { u"invNew"_qs,        { } },
                               { u"itemYear"_qs,      { } },
                               { u"viewType"_qs,      u"x"_qs },
                               { u"invStock"_qs,      u"Y"_qs },
                               { u"invStockOnly"_qs,  { } },
                               { u"invQty"_qs,        { } },
                               { u"invQtyMin"_qs,     u"0"_qs },
                               { u"invQtyMax"_qs,     u"0"_qs },
                               { u"invBrikTrak"_qs,   { } },
                               { u"invDesc"_qs,       { } }
                              });
    m_core->retrieveAuthenticated(m_job);
    return true;
}

void BrickLink::Store::cancelUpdate()
{
    if ((m_updateStatus == UpdateStatus::Updating) && m_job)
        m_job->abort();
}

#include "moc_store.cpp"
