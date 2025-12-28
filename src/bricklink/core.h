// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <vector>
#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QUrl>
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
    void setAccessToken(const QString &accessToken);
    bool hasAccessToken() const;

    bool isAuthenticated() const;
    void retrieveAuthenticated(TransferJob *job);
    void authenticate();

    void retrieve(TransferJob *job, bool highPriority = false);

    Database *database() const  { return m_database.get(); }
#if !defined(BS_BACKEND)
    Store *store() const  { return m_store.get(); }
    Orders *orders() const  { return m_orders.get(); }
    Carts *carts() const  { return m_carts.get(); }
    WantedLists *wantedLists() const  { return m_wantedLists.get(); }
    PriceGuideCache *priceGuideCache() const { return m_priceGuideCache.get(); }
    PictureCache *pictureCache() const { return m_pictureCache.get(); }
#endif
    inline const std::vector<Color> &colors() const         { return database()->m_colors; }
    inline const std::vector<Category> &categories() const  { return database()->m_categories; }
    inline const std::vector<ItemType> &itemTypes() const   { return database()->m_itemTypes; }
    inline const std::vector<Item> &items() const           { return database()->m_items; }
    inline const std::vector<ItemChangeLogEntry>  &itemChangelog() const     { return database()->m_itemChangelog; }
    inline const std::vector<ColorChangeLogEntry> &colorChangelog() const    { return database()->m_colorChangelog; }
    inline const std::vector<Relationship> &relationships() const            { return database()->m_relationships; }
    inline const std::vector<RelationshipMatch> &relationshipMatches() const { return database()->m_relationshipMatches; }

    inline uint latestChangelogId() const { return database()->m_latestChangelogId; }

    inline QString apiKey(const QByteArray &id) const { return database()->m_apiKeys.value(id); }

    const QImage noImage(const QSize &s) const;

    const Color *color(uint id) const;
    const Color *colorFromName(const QString &name) const;
    const Color *colorFromLDrawId(int ldrawId) const;
    const Category *category(uint id) const;
    const ItemType *itemType(char id) const;
    const Item *item(char tid, const QByteArray &id) const;
    const Item *item(const std::string &tids, const QByteArray &id) const;

    std::tuple<const Item *, const Color *> partColorCode(uint id) const;

    const Relationship *relationship(uint id) const;
    const RelationshipMatch *relationshipMatch(uint id) const;

    QSize standardPictureSize() const;

    QByteArray applyItemChangeLog(QByteArray itemTypeAndId, uint startAtChangelogId,
                                  const QDate &creationDate);
    uint applyColorChangeLog(uint colorId, uint startAtChangelogId, const QDate &creationDate);

    QString countryIdFromName(const QString &name) const;

    QString itemHtmlDescription(const Item *item, const Color *color, const QColor &highlight) const;

    enum class ResolveResult { Fail, Direct, ChangeLog };
    ResolveResult resolveIncomplete(Lot *lot, uint startAtChangelogId, const QDateTime &creationTime);

    static const QSet<ApiQuirk> knownApiQuirks();
    static QString apiQuirkDescription(ApiQuirk apiQuirk);
    bool isApiQuirkActive(ApiQuirk apiQuirk);
    QSet<ApiQuirk> activeApiQuirks() const;
    void setActiveApiQuirks(const QSet<ApiQuirk> &apiQuirks); // for debuging only

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
    void authenticationFinished(const QString &accessToken, const QString &error);

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
    QString                    m_accessToken;
    QByteArray                 m_sessionToken;
    TransferJob *              m_loginJob = nullptr;
    QVector<TransferJob *>     m_jobsWaitingForAuthentication;
    QHash<TransferJob *, bool> m_authenticatedJobFollowRedirect;
    int                        m_transferStatId = -1;

    std::unique_ptr<Database> m_database;
#if !defined(BS_BACKEND)
    std::unique_ptr<Store> m_store;
    std::unique_ptr<Orders> m_orders;
    std::unique_ptr<Carts> m_carts;
    std::unique_ptr<WantedLists> m_wantedLists;
    std::unique_ptr<PriceGuideCache> m_priceGuideCache;
    std::unique_ptr<PictureCache> m_pictureCache;
#endif

private:
    friend class QmlBrickLink;
};

inline Core *core() { return Core::inst(); }
inline Core *create(const QString &dataDir, const QString &updateUrl = { }, quint64 physicalMem = 0ULL)
{ return Core::create(dataDir, updateUrl, physicalMem); }

} // namespace BrickLink
