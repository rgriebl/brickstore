/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#pragma once

#include <QCoreApplication>
#include "bricklink/global.h"
#include "bricklink/io.h"
#include "bricklink/lot.h"
#include "qcoro/qcoro.h"

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
    static QStringList nameFiltersForBrickLinkXML(bool includeAll = false);
    static QStringList nameFiltersForBrickStoreXML(bool includeAll = false);
    static QStringList nameFiltersForLDraw(bool includeAll = false);

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

    static Document *parseBsxInventory(QIODevice *in);
    static bool createBsxInventory(QIODevice *out, const Document *doc);

private:
    static bool parseLDrawModel(QFile *f, bool isStudio, BrickLink::IO::ParseResult &pr);
    static bool parseLDrawModelInternal(QFile *f, bool isStudio, const QString &modelName,
                                        QVector<Lot *> &lots,
                                        QHash<QString, QVector<Lot *> > &subCache,
                                        QVector<QString> &recursionDetection);


};
