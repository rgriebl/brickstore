// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QDebug>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "bricklink/core.h"
#include "bricklink/item.h"
#include "core.h"

Q_LOGGING_CATEGORY(LogScanner, "scanner")

namespace Scanner {

class CorePrivate
{
public:
    QNetworkAccessManager *nam;
    QByteArray defaultBackendId;
    QVector<Core::Backend> availableBackends;

    QHash<QByteArray, bool> scanning;
    uint nextId = 0;
};


Core *Core::s_inst = nullptr;

Core *Core::inst()
{
    if (!s_inst)
        s_inst = new Core();
    return s_inst;
}

Core::~Core()
{
    s_inst = nullptr;
}

Core::Core()
    : QObject()
    , d(new CorePrivate)
{
    d->nam = new QNetworkAccessManager(this);

    d->availableBackends = {
        { "brickognize", u"Brickognize.com"_qs, "PSM", 1024 }
    };
    d->defaultBackendId = d->availableBackends.constFirst().id;
}

QByteArrayList Core::availableBackendIds() const
{
    QByteArrayList result;
    result.reserve(d->availableBackends.size());
    for (const auto &b : d->availableBackends)
        result << b.id;
    return result;
}

void Core::setDefaultBackendId(const QByteArray &id)
{
    if (backendFromId(id))
        d->defaultBackendId = id;
}

QByteArray Core::defaultBackendId() const
{
    return d->defaultBackendId;
}

const Core::Backend *Core::backendFromId(const QByteArray &id) const
{
    for (const auto &b : d->availableBackends) {
        if (b.id == id)
            return &b;
    }
    return nullptr;
}

uint Core::scan(const QImage &image, const BrickLink::ItemType *filter, const QByteArray &backendId)
{
    auto *backend = backendFromId(backendId.isEmpty() ? defaultBackendId() : backendId);
    if (!backend)
        return 0;

    if (d->scanning.contains(backend->id)) {
        qCCritical(LogScanner) << "Called scan while another scan is still is progress";
        return 0;
    }

    char itemTypeId = filter ? filter->id() : 0;
    if (itemTypeId && !backend->itemTypeFilter.contains(itemTypeId)) {
        qCWarning(LogScanner) << "Backend can not filter on the request item-type" << itemTypeId;
        itemTypeId = 0;
    }

    d->scanning[backend->id] = true;

    uint scanId = ++d->nextId;
    if (!scanId) // overflow
        scanId = ++d->nextId;

    if (backend->id == "brickognize") {
        QString path = u"/predict/"_qs;

        if (itemTypeId == 'P')
            path.append(u"parts/");
        else if (itemTypeId == 'M')
            path.append(u"figs/");
        else if (itemTypeId == 'S')
            path.append(u"sets/");

        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        image.scaled({ backend->preferredImageSize, backend->preferredImageSize }, Qt::KeepAspectRatio)
            .save(&buffer, "JPG");
        buffer.close();

        QNetworkRequest req(QUrl(u"https://api.brickognize.com" + path));
        QString ua = QCoreApplication::applicationName() + u'/' + QCoreApplication::applicationVersion();
        req.setHeader(QNetworkRequest::UserAgentHeader, ua);
        req.setRawHeader("Accept", "application/json");

        auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart imagePart;
        imagePart.setHeader(QNetworkRequest::ContentTypeHeader, u"image/jpg"_qs);
        imagePart.setHeader(QNetworkRequest::ContentLengthHeader, data.size());
        imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            u"form-data; name=\"query_image\"; filename=\"capture.jpg\""_qs);
        imagePart.setBody(data);
        multiPart->append(imagePart);

        auto reply = d->nam->post(req, multiPart);

        multiPart->setParent(reply);

        connect(reply, &QNetworkReply::finished, this, [this, reply, scanId, backendId = backend->id]() {
            auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt();

            if (!statusCode) {
                emit scanFailed(scanId, reply->errorString());

            } else if (statusCode == 422) {
                const auto json = QJsonDocument::fromJson(reply->readAll());
                const auto jsonDetails = json[u"detail"_qs].toArray();
                QStringList errorStrings;
                errorStrings.reserve(jsonDetails.size());
                for (const auto jsonDetail : jsonDetails) {
                    QString str = jsonDetail[u"msg"_qs].toString() + u" (type: "
                                  + jsonDetail[u"type"_qs].toString() + u")";
                    const auto locs = jsonDetail[u"loc"_qs].toArray();
                    if (!locs.isEmpty()) {
                        QStringList locList;
                        locList.reserve(locs.size());
                        for (const auto loc : locs)
                            locList << loc.toVariant().toString();
                        str = str + u" at [" + locList.join(u", ") + u']';
                    }
                    errorStrings << str;
                }
                emit scanFailed(scanId, u"Brickognize failed: " + errorStrings.join(u", "));

            } else if (statusCode != 200) {
                emit scanFailed(scanId, u"Brickognize returned an invalid status code: "
                                            + QString::number(statusCode));
            } else {
                const auto json = QJsonDocument::fromJson(reply->readAll());
                const auto jsonItems = json[u"items"_qs].toArray();

                QVector<Result> itemsAndScores;
                itemsAndScores.reserve(jsonItems.size());

                for (const auto jsonItem : jsonItems) {
                    const auto itemId = jsonItem[u"id"_qs].toString();
                    const auto type   = jsonItem[u"type"_qs].toString();
                    const auto score  = jsonItem[u"score"_qs].toDouble();
                    char itemTypeId = 0;

                    if (type == u"part")
                        itemTypeId = 'P';
                    else if (type == u"set")
                        itemTypeId = 'S';
                    else if (type == u"fig")
                        itemTypeId = 'M';

                    if (auto item = BrickLink::core()->item(itemTypeId, itemId.toLatin1())) {
                        itemsAndScores.emplace_back(item, score);
                        // qCDebug(LogScanner) << "Match:" << item->itemTypeId() << itemId
                        //                     << " Score:" << score;
                    } else {
                        auto inc = new BrickLink::Incomplete;
                        inc->m_item_id = itemId.toLatin1();
                        inc->m_itemtype_id = itemTypeId;
                        BrickLink::Lot lot;
                        lot.setIncomplete(inc);
                        const auto lastMonth = QDateTime::currentDateTime().addMonths(-3);

                        if (BrickLink::core()->resolveIncomplete(&lot, 0, lastMonth)
                                != BrickLink::Core::ResolveResult::Fail) {
                            itemsAndScores.emplace_back(lot.item(), score);
                        } else {
                            qCWarning(LogScanner) << "Brickognize returned an invalid match! type:"
                                                  << type << "id:" << itemId;
                        }
                    }
                }
                std::sort(itemsAndScores.begin(), itemsAndScores.end(), [](const auto &is1, const auto &is2) {
                    return is1.score > is2.score;
                });

                emit scanFinished(scanId, itemsAndScores);
            }
            d->scanning.remove(backendId);
            reply->deleteLater();
        });
    }

    return scanId;
}

QString Core::Backend::icon() const
{
    return u":/Scanner/service_"_qs + QString::fromLatin1(id);
}

} // namespace Scanner

#include "moc_core.cpp"
