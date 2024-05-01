// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QCameraDevice>
#include <QWindow>

#include "bricklink/global.h"


namespace Scanner {

class CapturePrivate;

class Capture : public QObject
{
    Q_OBJECT

public:
    Capture(QObject *parent);
    ~Capture() override;

    QObject *videoOutput() const;
    void setVideoOutput(QObject *videoOutput);
    Q_SIGNAL void videoOutputChanged();

    void trackWindowVisibility(QObject *window);

    Q_INVOKABLE void captureAndScan();

    static void checkSystemPermissions(QObject *context, const std::function<void(bool)> &callback);

    enum class State : int {
        Idle,         // -> Capturing / App to background OR dialog hide -> Inactive
        Capturing,    // -> Scanning | Error
        Scanning,     // -> Idle | NoMatch | Error
        NoMatch,      // 10sec -> Idle
        Error,        // 10sec -> Idle
        Inactive,     // App to foreground or dalog show -> Idle [dim overlay]
    };
    Q_ENUM(State);

    State state() const;
    Q_SIGNAL void stateChanged(Capture::State newState);

    QString lastError() const;

    int progress() const;
    Q_SIGNAL void progressChanged(int newProgress);

    bool isCameraActive() const;
    Q_SIGNAL void cameraActiveChanged(bool newActive);

    QByteArray currentCameraId() const;
    void setCurrentCameraId(const QByteArray &newCameraId);
    Q_SIGNAL void currentCameraIdChanged(const QByteArray &newCameraId);

    QByteArray currentBackendId() const;
    void setCurrentBackendId(const QByteArray &backend);
    Q_SIGNAL void currentBackendIdChanged(const QByteArray &backend);

    const BrickLink::ItemType *currentItemTypeFilter() const;
    void setCurrentItemTypeFilter(const BrickLink::ItemType *filter);
    Q_SIGNAL void currentItemTypeFilterChanged(const BrickLink::ItemType *filter);

    QList<const BrickLink::ItemType *> supportedItemTypeFilters() const;
    Q_SIGNAL void supportedItemTypeFiltersChanged(const QList<const BrickLink::ItemType *> &filters);

signals:
    void captureAndScanFinished(const QVector<const BrickLink::Item *> &items);

private:
    void setState(State newState);

    std::unique_ptr<CapturePrivate> d;
};

} // namespace Scanner
