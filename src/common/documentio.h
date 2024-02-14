// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QCoreApplication>
#include "bricklink/global.h"
#include "bricklink/io.h"
#include "bricklink/lot.h"

#include <QCoro/QCoroTask>

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QIODevice)

class Document;
class DocumentModel;
class View;

using BrickLink::Lot;
using BrickLink::LotList;

class DocumentIO
{
    Q_DECLARE_TR_FUNCTIONS(DocumentIO)

public:
    static QList<QPair<QString, QStringList>> nameFiltersForBrickLinkXML();
    static QList<QPair<QString, QStringList>> nameFiltersForBrickStoreXML();
    static QList<QPair<QString, QStringList>> nameFiltersForLDraw();

    class BsxContents : public BrickLink::IO::ParseResult
    {
    public:
        BsxContents() = default;
        BsxContents(const LotList &lots) : ParseResult(lots) { }
        BsxContents(const BsxContents &);  // no copy, but allow RVO

        QByteArray guiColumnLayout;
        QByteArray guiSortFilterState;
    };

    static Document *importBrickLinkStore(BrickLink::Store *store);
    static Document *importBrickLinkOrder(BrickLink::Order *order);
    static Document *importBrickLinkCart(BrickLink::Cart *cart);
    static Document *importBrickLinkWantedList(BrickLink::WantedList *wantedList);

    static QCoro::Task<Document *> importBrickLinkXML(QString fileName = { });
    static QCoro::Task<Document *> importLDrawModel(QString fileName = { });

    static QString exportBrickLinkUpdateClipboard(const DocumentModel *doc,
                                                  const LotList &lots);

    static Document *parseBsxInventory(QFile *in);
    static bool createBsxInventory(QIODevice *out, const Document *doc);

private:
    static bool parseLDrawModel(QFile *f, bool isStudio, BrickLink::IO::ParseResult &pr);
    static bool parseLDrawModelInternal(QFile *f, bool isStudio, const QString &modelName,
                                        QVector<Lot *> &lots,
                                        QHash<QString, QVector<Lot *> > &subCache,
                                        QVector<QString> &recursionDetection);


};
