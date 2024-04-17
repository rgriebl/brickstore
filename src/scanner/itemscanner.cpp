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
#include "itemscanner.h"

Q_LOGGING_CATEGORY(LogScanner, "scanner")


QIcon ItemScanner::Backend::icon() const
{
    return QIcon(u":/Scanner/service_"_qs + QString::fromLatin1(id));
}

class ItemScannerPrivate
{
public:
    QNetworkAccessManager *nam;
    QByteArray defaultBackendId;
    QVector<ItemScanner::Backend> availableBackends;

    const ItemScanner::Backend *backendFromId(const QByteArray &id) const
    {
        for (const auto &b : availableBackends) {
            if (b.id == id)
                return &b;
        }
        return nullptr;
    }

    QHash<QByteArray, bool> scanning;
    uint nextId = 0;
};


ItemScanner *ItemScanner::s_inst = nullptr;

ItemScanner *ItemScanner::inst()
{
    if (!s_inst)
        s_inst = new ItemScanner();
    return s_inst;
}

ItemScanner::~ItemScanner()
{
    delete d;
    s_inst = nullptr;
}

ItemScanner::ItemScanner()
    : QObject()
    , d(new ItemScannerPrivate)
{
    d->nam = new QNetworkAccessManager(this);

    d->availableBackends = {
        { "brickognize", u"Brickognize.com"_qs, "*PSM", 1024 }
    };
    d->defaultBackendId = d->availableBackends.constFirst().id;
}

const QVector<ItemScanner::Backend> &ItemScanner::availableBackends() const
{
    return d->availableBackends;
}

void ItemScanner::setDefaultBackendId(const QByteArray &id)
{
    if (d->backendFromId(id))
        d->defaultBackendId = id;
}

QByteArray ItemScanner::defaultBackendId() const
{
    return d->defaultBackendId;
}

    int scan(const QImage &image, const QByteArray &backendId = { });


uint ItemScanner::scan(const QImage &image, char itemTypeId, const QByteArray &backendId)
{
    auto *backend = d->backendFromId(backendId.isEmpty() ? defaultBackendId() : backendId);
    if (!backend)
        return 0;

    if (d->scanning.contains(backend->id)) {
        qCCritical(LogScanner) << "Called scan while another scan is still is progress";
        return 0;
    }
    d->scanning[backend->id] = true;

    uint scanId = ++d->nextId;

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
                        qCWarning(LogScanner) << "Brickognize returned an invalid match! type:"
                                              << type << "id:" << itemId;
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

#include "moc_itemscanner.cpp"
