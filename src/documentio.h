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
                                              int quantity = 1,
                                              BrickLink::Condition condition = BrickLink::Condition::New,
                                              BrickLink::Status extraParts = BrickLink::Status::Extra,
                                              bool includeInstructions = false);
    static Document *importBrickLinkOrder(BrickLink::Order *order, const QByteArray &orderXml);
    static Document *importBrickLinkStore();
    static Document *importBrickLinkCart(BrickLink::Cart *cart, const BrickLink::InvItemList &itemlist);
    static Document *importBrickLinkXML();
    static Document *importLDrawModel();

    static bool save(Window *win);
    static bool saveAs(Window *win);
    static void exportBrickLinkXML(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkXMLClipboard(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkUpdateClipboard(const Document *doc,
                                               const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkInvReqClipboard(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkWantedListClipboard(const BrickLink::InvItemList &itemlist);

    struct BsxContents
    {
        BrickLink::InvItemList items;
        QString currencyCode;

        QHash<const BrickLink::InvItem *, BrickLink::InvItem> differenceModeBase;

        QByteArray guiColumnLayout;
        QByteArray guiSortFilterState;

        int invalidItemCount = 0; // parse only
    };

private:
    static Window *loadFrom(const QString &s);
    static bool saveTo(Window *win, const QString &s);

    static QString toBrickLinkXML(const BrickLink::InvItemList &itemlist);
    static BsxContents fromBrickLinkXML(const QByteArray &xml);

    static bool parseLDrawModel(QFile *f, BrickLink::InvItemList &items, int *invalid_items);
    static bool parseLDrawModelInternal(QFile *f, const QString &model_name,
                                        BrickLink::InvItemList &items,
                                        int *invalid_items,
                                        QHash<QPair<QString, uint>, BrickLink::InvItem *> &mergehash,
                                        QStringList &recursion_detection);

    static bool resolveIncomplete(BrickLink::InvItem *item);

    static BsxContents parseBsxInventory(QIODevice *in);
    static bool createBsxInventory(QIODevice *out, const BsxContents &bsx);

    static QString lastDirectory();
    static void setLastDirectory(const QString &dir);

    static QString s_lastDirectory;
};
