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

#include <vector>

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QUrl>
#include <QtCore/QThreadPool>
#include <QtGui/QImage>
#include <QtGui/QIcon>

#include "bricklink/global.h"
#include "bricklink/database.h"
#include "bricklink/priceguide.h"
#include "bricklink/picture.h"
#include "bricklink/lot.h"

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QSaveFile)

class Transfer;
class TransferJob;

namespace BrickLink {

class QmlBrickLink;


class Core : public QObject
{
    Q_OBJECT

public:
    ~Core() override;

    static QUrl urlForInventoryRequest();
    static QUrl urlForWantedListUpload();
    static QUrl urlForInventoryUpload();
    static QUrl urlForInventoryUpdate();
    static QUrl urlForCatalogInfo(const Item *item, const Color *color = nullptr);
    static QUrl urlForPriceGuideInfo(const Item *item, const Color *color = nullptr);
    static QUrl urlForLotsForSale(const Item *item, const Color *color = nullptr);
    static QUrl urlForAppearsInSets(const Item *item, const Color *color = nullptr);
    static QUrl urlForStoreItemDetail(uint lotId);
    static QUrl urlForStoreItemSearch(const Item *item, const Color *color = nullptr);
    static QUrl urlForOrderDetails(const QString &orderId);
    static QUrl urlForShoppingCart(int shopId);
    static QUrl urlForWantedList(uint wantedListId);

    QString dataPath() const;
    QFile *dataReadFile(QStringView fileName, const Item *item,
                        const Color *color = nullptr) const;
    QSaveFile *dataSaveFile(QStringView fileName, const Item *item,
                            const Color *color = nullptr) const;
    void setCredentials(const QPair<QString, QString> &credentials);
    QString userId() const;

    bool isAuthenticated() const;
    void retrieveAuthenticated(TransferJob *job);

    void retrieve(TransferJob *job, bool highPriority = false);

    Store *store() const  { return m_store; }
    Orders *orders() const  { return m_orders; }
    Carts *carts() const  { return m_carts; }
    WantedLists *wantedLists() const  { return m_wantedLists; }
    Database *database() const  { return m_database; }
    PriceGuideCache *priceGuideCache() const { return m_priceGuideCache; }
    PictureCache *pictureCache() const { return m_pictureCache; }

    inline const std::vector<Color> &colors() const         { return database()->m_colors; }
    inline const std::vector<Category> &categories() const  { return database()->m_categories; }
    inline const std::vector<ItemType> &itemTypes() const   { return database()->m_itemTypes; }
    inline const std::vector<Item> &items() const           { return database()->m_items; }
    inline const std::vector<PartColorCode> &pccs() const   { return database()->m_pccs; }
    inline const std::vector<ItemChangeLogEntry>  &itemChangelog() const  { return database()->m_itemChangelog; }
    inline const std::vector<ColorChangeLogEntry> &colorChangelog() const { return database()->m_colorChangelog; }

    const QImage noImage(const QSize &s) const;

    const Color *color(uint id) const;
    const Color *colorFromName(const QString &name) const;
    const Color *colorFromLDrawId(int ldrawId) const;
    const Category *category(uint id) const;
    const ItemType *itemType(char id) const;
    const Item *item(char tid, const QByteArray &id) const;
    const Item *item(const std::string &tids, const QByteArray &id) const;

    const PartColorCode *partColorCode(uint id);

    QSize standardPictureSize() const;

    bool applyChangeLog(const Item *&item, const Color *&color, const Incomplete *inc);

    QString countryIdFromName(const QString &name) const;

    static QString itemHtmlDescription(const Item *item, const Color *color, const QColor &highlight);

    enum class ResolveResult { Fail, Direct, ChangeLog };
    ResolveResult resolveIncomplete(Lot *lot);

public slots:
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

    void cancelTransfers();

signals:
    void transferFinished(TransferJob *job);
    void transferProgress(int progress, int total);
    void authenticatedTransferOverallProgress(int progress, int total);
    void authenticatedTransferStarted(TransferJob *job);
    void authenticatedTransferProgress(TransferJob *job, int progress, int total);
    void authenticatedTransferFinished(TransferJob *job);

    void authenticationChanged(bool auth);
    void authenticationFailed(const QString &userName, const QString &error);

    void userIdChanged(const QString &userId);

private:
    Core(const QString &dataDir, const QString &updateUrl, quint64 physicalMem);

    static Core *create(const QString &dataDir, const QString &updateUrl, quint64 physicalMem);
    static inline Core *inst() { return s_inst; }
    static Core *s_inst;

    friend Core *core();
    friend Core *create(const QString &, const QString &, quint64);

private:
    QString dataFileName(QStringView fileName, const Item *item, const Color *color) const;

private:
    QString  m_datadir;

    QIcon                           m_noImageIcon;
    mutable QHash<uint, QImage>     m_noImageCache;

    Transfer *                 m_transfer = nullptr;
    Transfer *                 m_authenticatedTransfer = nullptr;
    bool                       m_authenticated = false;
    QPair<QString, QString>    m_credentials;
    TransferJob *              m_loginJob = nullptr;
    QVector<TransferJob *>     m_jobsWaitingForAuthentication;
    int                        m_transferStatId = -1;

    Database *m_database = nullptr;
    Store *m_store = nullptr;
    Orders *m_orders = nullptr;
    Carts *m_carts = nullptr;
    WantedLists *m_wantedLists = nullptr;
    PriceGuideCache *m_priceGuideCache = nullptr;
    PictureCache *m_pictureCache = nullptr;

    friend class QmlBrickLink;
};

inline Core *core() { return Core::inst(); }
inline Core *create(const QString &dataDir, const QString &updateUrl = { }, quint64 physicalMem = 0ULL)
{ return Core::create(dataDir, updateUrl, physicalMem); }

} // namespace BrickLink
