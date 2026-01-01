// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "core.h"
#include "qmlapi.h"


namespace Scanner {

QmlCapture::QmlCapture(QObject *parent)
    : QObject(parent)
    , d(new Capture(this))
{
    connect(d, &Capture::captureAndScanFinished,
            this, [this](const QVector<const BrickLink::Item*> &items) {
        QVariantList qmlItems;
        qmlItems.reserve(items.size());
        for (const auto *item : items)
            qmlItems << QVariant::fromValue(BrickLink::QmlItem(item));
        emit captureAndScanFinished(qmlItems);
    });
    connect(d, &Capture::progressChanged,
            this, &QmlCapture::progressChanged);
    connect(d, &Capture::stateChanged,
            this, &QmlCapture::stateChanged);
    connect(d, &Capture::cameraActiveChanged,
            this, &QmlCapture::cameraActiveChanged);
    connect(d, &Capture::currentBackendIdChanged,
            this, &QmlCapture::currentBackendIdChanged);
    connect(d, &Capture::currentCameraIdChanged,
            this, &QmlCapture::currentCameraIdChanged);
    connect(d, &Capture::supportedItemTypeFiltersChanged,
            this, &QmlCapture::supportedItemTypeFiltersChanged);
    connect(d, &Capture::currentItemTypeFilterChanged,
            this, &QmlCapture::currentItemTypeFilterChanged);
}

void QmlCapture::captureAndScan()
{
    d->captureAndScan();
}

QVariantList QmlCapture::supportedItemTypeFilters() const
{
    const auto supported = d->supportedItemTypeFilters();
    QVariantList result;
    result.reserve(supported.size());
    for (const auto *filter : supported) {
        result << QVariant::fromValue(BrickLink::QmlItemType(filter));
    }
    return result;
}

BrickLink::QmlItemType QmlCapture::currentItemTypeFilter() const
{
    return BrickLink::QmlItemType(d->currentItemTypeFilter());
}

void QmlCapture::setCurrentItemTypeFilter(const BrickLink::QmlItemType &newCurrentItemTypeFilter)
{
    d->setCurrentItemTypeFilter(newCurrentItemTypeFilter.wrappedObject());
}

QmlCore::QmlCore()
{ }

QVariantMap QmlCore::backendFromId(const QByteArray &id) const
{
    if (const auto *backend = Scanner::core()->backendFromId(id)) {
        QVariantMap map;
        map[u"id"_qs] = id;
        map[u"name"_qs] = backend->name;
        map[u"icon"_qs] = QString(u"qrc" + backend->icon());
        map[u"itemTypeFiler"_qs] = backend->itemTypeFilter;
        return map;
    }
    return { };
}

QVariantList QmlCore::availableBackends() const
{
    const auto backendIds = Scanner::core()->availableBackendIds();
    QVariantList result;
    result.reserve(backendIds.size());
    for (const auto &backendId : backendIds)
        result << backendFromId(backendId);
    return result;
}

QByteArray QmlCore::defaultBackendId() const
{
    return Scanner::core()->defaultBackendId();
}

void QmlCore::setDefaultBackendId(const QByteArray &newDefaultBackendId)
{
    Scanner::core()->setDefaultBackendId(newDefaultBackendId);
}

void QmlCore::checkSystemPermissions(QObject *context, QJSValue callback)
{
    if (!callback.isCallable())
        return;

    Scanner::Capture::checkSystemPermissions(context, [callback](bool granted) {
        callback.call({ granted });
    });
}

} // namespace Scanner

#include "moc_qmlapi.cpp"
