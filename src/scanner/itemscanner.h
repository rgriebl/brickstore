// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QVector>
#include <QIcon>

#include "bricklink/global.h"


class ItemScannerPrivate;

class ItemScanner : public QObject
{
    Q_OBJECT
public:
    static ItemScanner *inst();
    ItemScanner(const ItemScanner &) = delete;
    ~ItemScanner() override;

    struct Backend {
        QByteArray id;
        QString    name;
        QByteArray itemTypeFilter; // "BCGIMOPS" + '*' == Any
        int        preferredImageSize;
        QIcon icon() const;
    };

    struct Result {
        // shouldn't be needed, but clang still lacks support for aggregate initialization
        Result(const BrickLink::Item *_item, double _score) : item(_item), score(_score) { }

        const BrickLink::Item *item = nullptr;
        double score = 0;
    };

    const QVector<Backend> &availableBackends() const;
    void setDefaultBackendId(const QByteArray &id);
    QByteArray defaultBackendId() const;

    uint scan(const QImage &image, char itemTypeId = '*', const QByteArray &backendId = { });

signals:
    void scanFinished(uint id, const QVector<ItemScanner::Result> &itemsAndScores);
    void scanFailed(uint id, const QString &error);

private:
    ItemScanner();
    static ItemScanner *s_inst;

    ItemScannerPrivate *d;
};
