// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// #include <functional>

// #include <QtCore/QMetaType>
// #include <QtCore/QString>
#include <QtCore/QObject>
// #include <QtCore/QIdentityProxyModel>
#include <QtQml/QQmlEngine>
#include "bricklink/qmlapi.h"
#include "capture.h"


namespace Scanner {

class QmlCore : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Core)
    QML_SINGLETON
    Q_PROPERTY(QVariantList availableBackends READ availableBackends CONSTANT FINAL)
    Q_PROPERTY(QByteArray defaultBackendId READ defaultBackendId WRITE setDefaultBackendId NOTIFY defaultBackendIdChanged)

public:
    QmlCore();

    Q_INVOKABLE QVariantMap backendFromId(const QByteArray &id) const;

    QVariantList availableBackends() const;

    QByteArray defaultBackendId() const;
    void setDefaultBackendId(const QByteArray &newDefaultBackendId);
    Q_SIGNAL void defaultBackendIdChanged(const QByteArray &newDefaultBackendId);

    Q_INVOKABLE void checkSystemPermissions(QObject *context, QJSValue callback);
};

class QmlCapture : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Capture)
    QML_EXTENDED_NAMESPACE(Scanner::Capture)
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    Q_PRIVATE_PROPERTY(d, Scanner::Capture::State state READ state NOTIFY stateChanged)
    Q_PRIVATE_PROPERTY(d, QString lastError READ lastError NOTIFY stateChanged)
    Q_PRIVATE_PROPERTY(d, bool cameraActive READ isCameraActive NOTIFY cameraActiveChanged)
    Q_PRIVATE_PROPERTY(d, QObject *videoOutput READ videoOutput WRITE setVideoOutput NOTIFY videoOutputChanged)
    Q_PRIVATE_PROPERTY(d, QByteArray currentBackendId READ currentBackendId WRITE setCurrentBackendId NOTIFY currentBackendIdChanged)
    Q_PRIVATE_PROPERTY(d, QByteArray currentCameraId READ currentCameraId WRITE setCurrentCameraId NOTIFY currentCameraIdChanged)
    Q_PROPERTY(BrickLink::QmlItemType currentItemTypeFilter READ currentItemTypeFilter WRITE setCurrentItemTypeFilter NOTIFY currentItemTypeFilterChanged)
    Q_PROPERTY(QVariantList supportedItemTypeFilters READ supportedItemTypeFilters NOTIFY supportedItemTypeFiltersChanged)
    Q_PRIVATE_PROPERTY(d, int progress READ progress NOTIFY progressChanged)

public:
    QmlCapture(QObject *parent = nullptr);

    Q_SIGNAL void stateChanged();
    Q_SIGNAL void cameraActiveChanged();
    Q_SIGNAL void progressChanged();

    QObject *videoOutput() const;
    Q_SIGNAL void videoOutputChanged();

    Q_SIGNAL void currentCameraIdChanged();
    Q_SIGNAL void currentBackendIdChanged();

    BrickLink::QmlItemType currentItemTypeFilter() const;
    void setCurrentItemTypeFilter(const BrickLink::QmlItemType &newCurrentItemTypeFilter);
    Q_SIGNAL void currentItemTypeFilterChanged();

    QVariantList supportedItemTypeFilters() const;
    Q_SIGNAL void supportedItemTypeFiltersChanged();

    Q_INVOKABLE void captureAndScan();

signals:
    void captureAndScanFinished(const QVariantList &items);

private:
    Capture *d;
    double m_progress = .0;
};

} // namespace Scanner
