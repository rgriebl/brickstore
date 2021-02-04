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
                                              BrickLink::Condition condition = BrickLink::Condition::New);
    static Document *importBrickLinkOrder(BrickLink::Order *order, const QByteArray &orderXml);
    static Document *importBrickLinkStore();
    static Document *importBrickLinkCart();
    static Document *importBrickLinkXML();
    static Document *importLDrawModel();

    static bool save(Document *doc);
    static bool saveAs(Document *doc);
    static void exportBrickLinkXML(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkXMLClipboard(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkUpdateClipboard(const Document *doc,
                                               const Document::ItemList &itemlist);
    static void exportBrickLinkInvReqClipboard(const BrickLink::InvItemList &itemlist);
    static void exportBrickLinkWantedListClipboard(const BrickLink::InvItemList &itemlist);

private:
    static Document *loadFrom(const QString &s);
    static bool saveTo(Document *doc, const QString &s, bool export_only);

    static QString toBrickLinkXML(const BrickLink::InvItemList &itemlist);
    static QPair<BrickLink::InvItemList, QString> fromBrickLinkXML(const QByteArray &xml);

    static bool parseLDrawModel(QFile &f, BrickLink::InvItemList &items, uint *invalid_items);
    static bool parseLDrawModelInternal(QFile &f, const QString &model_name,
                                        BrickLink::InvItemList &items,
                                        uint *invalid_items,
                                        QHash<QString, BrickLink::InvItem *> &mergehash,
                                        QStringList &recursion_detection);

    struct ParseItemListResult
    {
        BrickLink::InvItemList items;
        QString currencyCode;

        QHash<const BrickLink::InvItem *, BrickLink::InvItem> differenceModeBase;
        bool differenceModeActive = false;
        bool differenceModeBroken = false;
        bool differenceModeMigrated = false;

        int invalidItemCount = 0;

        QDomElement domGuiState;
    };

    static ParseItemListResult parseBsxInventory(const QDomDocument &domDoc);
    static QDomDocument createBsxInventory(const Document *doc);
};
