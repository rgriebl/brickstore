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
#pragma once

#include <QCoreApplication>
#include "bricklink/bricklinkfwd.h"
#include "lot.h"

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QIODevice)

class Document;
class View;


class DocumentIO
{
    Q_DECLARE_TR_FUNCTIONS(DocumentIO)

public:
    static Document *create();
    static View *open();
    static View *open(const QString &name);
    static Document *importBrickLinkInventory(const BrickLink::Item *preselect = nullptr,
                                              const BrickLink::Color *color = nullptr,
                                              int multiply = 1,
                                              BrickLink::Condition condition = BrickLink::Condition::New,
                                              BrickLink::Status extraParts = BrickLink::Status::Extra,
                                              bool includeInstructions = false);
    static Document *importBrickLinkOrder(BrickLink::Order *order, const LotList &lots);
    static Document *importBrickLinkStore();
    static Document *importBrickLinkCart(BrickLink::Cart *cart, const LotList &lots);
    static Document *importBrickLinkXML();
    static Document *importLDrawModel();

    static bool save(View *win);
    static bool saveAs(View *win);
    static void exportBrickLinkXML(const LotList &lots);
    static void exportBrickLinkXMLClipboard(const LotList &itemlist);
    static void exportBrickLinkUpdateClipboard(const Document *doc,
                                               const LotList &lots);
    static void exportBrickLinkInvReqClipboard(const LotList &lots);
    static void exportBrickLinkWantedListClipboard(const LotList &lots);

    struct BsxContents
    {
        LotList lots;
        QString currencyCode;

        QHash<const Lot *, Lot> differenceModeBase;

        QByteArray guiColumnLayout;
        QByteArray guiSortFilterState;

        // parse only
        int invalidLotsCount = 0;
        int fixedLotsCount = 0;
    };

    static QString toBrickLinkXML(const LotList &lots);
    static BsxContents fromBrickLinkXML(const QByteArray &xml);

private:
    static View *loadFrom(const QString &s);
    static bool saveTo(View *win, const QString &s);

    static bool parseLDrawModel(QFile *f, bool isStudio, LotList &lots, int *invalidLots);
    static bool parseLDrawModelInternal(QFile *f, bool isStudio, const QString &modelName,
                                        QVector<Lot *> &lots,
                                        QHash<QString, QVector<Lot *> > &subCache,
                                        QVector<QString> &recursionDetection);

    enum class ResolveResult { Fail, Direct, ChangeLog };
    static ResolveResult resolveIncomplete(Lot *lot);

    static BsxContents parseBsxInventory(QIODevice *in);
    static bool createBsxInventory(QIODevice *out, const BsxContents &bsx);
};
