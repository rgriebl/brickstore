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
#include "bricklinkfwd.h"
#include "lot.h"

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QIODevice)

class Document;
class Window;


class DocumentIO
{
    Q_DECLARE_TR_FUNCTIONS(DocumentIO)

public:
    static Document *create();
    static Window *open();
    static Window *open(const QString &name);
    static Document *importBrickLinkInventory(const BrickLink::Item *preselect = nullptr,
                                              int multiply = 1,
                                              BrickLink::Condition condition = BrickLink::Condition::New,
                                              BrickLink::Status extraParts = BrickLink::Status::Extra,
                                              bool includeInstructions = false);
    static Document *importBrickLinkOrder(BrickLink::Order *order, const LotList &lots);
    static Document *importBrickLinkStore();
    static Document *importBrickLinkCart(BrickLink::Cart *cart, const LotList &lots);
    static Document *importBrickLinkXML();
    static Document *importLDrawModel();

    static bool save(Window *win);
    static bool saveAs(Window *win);
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

        int invalidLotsCount = 0; // parse only
    };

    static QString toBrickLinkXML(const LotList &lots);
    static BsxContents fromBrickLinkXML(const QByteArray &xml);

private:
    static Window *loadFrom(const QString &s);
    static bool saveTo(Window *win, const QString &s);

    static bool parseLDrawModel(QFile *f, LotList &lots, int *invalidLots);
    static bool parseLDrawModelInternal(QFile *f, const QString &modelName,
                                        QVector<Lot *> &lots,
                                        QHash<QString, QVector<Lot *> > &subCache,
                                        QVector<QString> &recursionDetection);

    static bool resolveIncomplete(Lot *lot);

    static BsxContents parseBsxInventory(QIODevice *in);
    static bool createBsxInventory(QIODevice *out, const BsxContents &bsx);

    static QString lastDirectory();
    static void setLastDirectory(const QString &dir);

    static QString s_lastDirectory;
};
