/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/

#include <QUrl>
#include <QUrlQuery>

#include "utility/utility.h"
#include "utility/transfer.h"
#include "utility/exception.h"
#include "bricklink/core.h"
#include "bricklink/io.h"
#include "bricklink/store.h"


BrickLink::Store::Store(QObject *parent)
    : QObject(parent)
{
    connect(core(), &Core::authenticatedTransferStarted,
            this, [this](TransferJob *job) {
        if ((m_updateStatus == UpdateStatus::Updating) && (m_job == job))
            emit updateStarted();
    });
    connect(core(), &Core::authenticatedTransferProgress,
            this, [this](TransferJob *job, int progress, int total) {
        if ((m_updateStatus == UpdateStatus::Updating) && (m_job == job))
            emit updateProgress(progress, total);
    });
    connect(core(), &Core::authenticatedTransferFinished,
            this, [this](TransferJob *job) {
        if ((m_updateStatus == UpdateStatus::Updating) && (m_job == job)) {
            bool success = job->isCompleted() && (job->responseCode() == 200) && job->data();
            QString message;
            if (success) {
                try {
                    auto result = IO::fromBrickLinkXML(*job->data(), IO::Hint::Store);
                    m_lots = result.takeLots();
                    m_currencyCode = result.currencyCode();
                    m_valid = true;
                } catch (const Exception &e) {
                    success = false;
                    message = tr("Failed to import the store inventory") % u": " % e.error();
                }
            } else {
                message = tr("Failed to download the store inventory") % u": " % job->errorString();
            }
            m_updateStatus = success ? UpdateStatus::Ok : UpdateStatus::UpdateFailed;
            emit updateFinished(success, message);
            m_job = nullptr;
        }
    });
}

BrickLink::Store::~Store()
{
    qDeleteAll(m_lots);
}

bool BrickLink::Store::startUpdate()
{
    if (updateStatus() == UpdateStatus::Updating)
        return false;
    Q_ASSERT(!m_job);
    m_updateStatus = UpdateStatus::Updating;

    QUrl url("https://www.bricklink.com/invExcelFinal.asp"_l1);
    QUrlQuery query;
    query.addQueryItem("itemType"_l1,      ""_l1);
    query.addQueryItem("catID"_l1,         ""_l1);
    query.addQueryItem("colorID"_l1,       ""_l1);
    query.addQueryItem("invNew"_l1,        ""_l1);
    query.addQueryItem("itemYear"_l1,      ""_l1);
    query.addQueryItem("viewType"_l1,      "x"_l1);    // XML
    query.addQueryItem("invStock"_l1,      "Y"_l1);
    query.addQueryItem("invStockOnly"_l1,  ""_l1);
    query.addQueryItem("invQty"_l1,        ""_l1);
    query.addQueryItem("invQtyMin"_l1,     "0"_l1);
    query.addQueryItem("invQtyMax"_l1,     "0"_l1);
    query.addQueryItem("invBrikTrak"_l1,   ""_l1);
    query.addQueryItem("invDesc"_l1,       ""_l1);
    url.setQuery(query);

    m_job = TransferJob::post(url);
    core()->retrieveAuthenticated(m_job);
    return true;
}

void BrickLink::Store::cancelUpdate()
{
    if ((m_updateStatus == UpdateStatus::Updating) && m_job)
        m_job->abort();
}
