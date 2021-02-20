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
#include <QDomDocument>
#include <QDomElement>
#include "bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QFile)

class Document;


class DocumentIO
{
    Q_DECLARE_TR_FUNCTIONS(DocumentIO)

public:
    static Document *create();
    static Document *open();
    static Document *open(const QString &name);
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

    static bool save(Document *doc);
    static bool saveAs(Document *doc);
    static void exportBrickLinkXML(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkXMLClipboard(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkUpdateClipboard(const Document *doc,
                                               const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkInvReqClipboard(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkWantedListClipboard(const BrickLink::InvItemList &itemlist);

private:
    static Document *loadFrom(const QString &s);
    static bool saveTo(Document *doc, const QString &s, bool export_only);

    static QString toBrickLinkXML(const BrickLink::InvItemList &itemlist);
    static QPair<BrickLink::InvItemList, QString> fromBrickLinkXML(const QByteArray &xml);

    static bool parseLDrawModel(QFile &f, BrickLink::InvItemList &items, int *invalid_items);
    static bool parseLDrawModelInternal(QFile &f, const QString &model_name,
                                        BrickLink::InvItemList &items,
                                        int *invalid_items,
                                        QHash<QString, BrickLink::InvItem *> &mergehash,
                                        QStringList &recursion_detection);

    struct ParseItemListResult
    {
        BrickLink::InvItemList items;
        QString currencyCode;

        QHash<const BrickLink::InvItem *, BrickLink::InvItem> differenceModeBase;
        bool xmlParseError = false;

        int invalidItemCount = 0;

        QDomElement domGuiState;
    };

    static ParseItemListResult parseBsxInventory(const QDomDocument &domDoc);
    static QDomDocument createBsxInventory(const Document *doc);

    static QString s_lastDirectory;
};
