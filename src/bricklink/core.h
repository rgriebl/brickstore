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
#include "bricklink/changelogentry.h"
#include "utility/q3cache.h"

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QSaveFile)

class Transfer;
class TransferJob;

namespace BrickLink {

class Incomplete;


class Core : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QDateTime databaseDate READ databaseDate NOTIFY databaseDateChanged)

public:
    ~Core() override;

    void openUrl(UrlList u, const void *opt = nullptr, const void *opt2 = nullptr);

    enum class DatabaseVersion {
        Invalid,
        Version_1, // deprecated
        Version_2, // deprecated
        Version_3,
        Version_4,
        Version_5,

        Latest = Version_5
    };

    QString defaultDatabaseName(DatabaseVersion version = DatabaseVersion::Latest) const;
    QDateTime databaseDate() const;
    bool isDatabaseUpdateNeeded() const;

    QString dataPath() const;
    QFile *dataReadFile(QStringView fileName, const Item *item,
                        const Color *color = nullptr) const;
    QSaveFile *dataSaveFile(QStringView fileName, const Item *item,
                            const Color *color = nullptr) const;
    void setCredentials(const QPair<QString, QString> &credentials);
    QString userId() const;

    bool isAuthenticated() const;
    void retrieveAuthenticated(TransferJob *job);

    Store *store() const  { return m_store; }
    Orders *orders() const  { return m_orders; }
    Carts *carts() const  { return m_carts; }

    inline const std::vector<Color> &colors() const         { return m_colors; }
    inline const std::vector<Category> &categories() const  { return m_categories; }
    inline const std::vector<ItemType> &itemTypes() const   { return m_item_types; }
    inline const std::vector<Item> &items() const           { return m_items; }

    const QImage noImage(const QSize &s) const;

    const QImage colorImage(const Color *col, int w, int h) const;

    const Color *color(uint id) const;
    const Color *colorFromName(const QString &name) const;
    const Color *colorFromLDrawId(int ldrawId) const;
    const Category *category(uint id) const;
    const ItemType *itemType(char id) const;
    const Item *item(char tid, const QByteArray &id) const;
    const Item *item(const std::string &tids, const QByteArray &id) const;

    const PartColorCode *partColorCode(uint id);

    PriceGuide *priceGuide(const Item *item, const Color *color, bool highPriority = false);

    QSize standardPictureSize() const;
    Picture *picture(const Item *item, const Color *color, bool highPriority = false);
    Picture *largePicture(const Item *item, bool highPriority = false);

    qreal itemImageScaleFactor() const;
    void setItemImageScaleFactor(qreal f);

    bool isLDrawEnabled() const;
    QString ldrawDataPath() const;
    void setLDrawDataPath(const QString &ldrawDataPath);

    bool applyChangeLog(const Item *&item, const Color *&color, Incomplete *inc);

    bool onlineStatus() const;

    QString countryIdFromName(const QString &name) const;

    TransferJob *downloadDatabase(const QUrl &url = { }, const QString &filename = { });

    bool isDatabaseValid() const;
    bool readDatabase(const QString &filename = QString());
    bool writeDatabase(const QString &filename, BrickLink::Core::DatabaseVersion version) const;

    enum class ResolveResult { Fail, Direct, ChangeLog };
    ResolveResult resolveIncomplete(Lot *lot);

public slots:
    void setOnlineStatus(bool on);
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

    void cancelTransfers();

signals:
    void databaseDateChanged(const QDateTime &date);
    void priceGuideUpdated(BrickLink::PriceGuide *pg);
    void pictureUpdated(BrickLink::Picture *pic);
    void itemImageScaleFactorChanged(qreal f);

    void transferProgress(int progress, int total);
    void authenticatedTransferOverallProgress(int progress, int total);
    void authenticatedTransferStarted(TransferJob *job);
    void authenticatedTransferProgress(TransferJob *job, int progress, int total);
    void authenticatedTransferFinished(TransferJob *job);

    void authenticationChanged(bool auth);
    void authenticationFailed(const QString &userName, const QString &error);

    void userIdChanged(const QString &userId);

private:
    Core(const QString &datadir);

    static Core *create(const QString &datadir, QString *errstring);
    static inline Core *inst() { return s_inst; }
    static Core *s_inst;

    friend Core *core();
    friend Core *create(const QString &, QString *);

private:
    QString dataFileName(QStringView fileName, const Item *item, const Color *color) const;

    void updatePriceGuide(BrickLink::PriceGuide *pg, bool highPriority = false);
    void updatePicture(BrickLink::Picture *pic, bool highPriority = false);
//    friend void PriceGuide::update(bool);
//    friend void Picture::update(bool);
    friend class PriceGuide;
    friend class Picture;

    void cancelPriceGuideUpdate(BrickLink::PriceGuide *pg);
    void cancelPictureUpdate(BrickLink::Picture *pic);
//    friend void PriceGuide::cancelUpdate();
//    friend void Picture::cancelUpdate();

    static bool updateNeeded(bool valid, const QDateTime &last, int iv);

    static void readColorFromDatabase(Color &col, QDataStream &dataStream, DatabaseVersion v);
    static void writeColorToDatabase(const Color &color, QDataStream &dataStream, DatabaseVersion v);

    static void readCategoryFromDatabase(Category &cat, QDataStream &dataStream, DatabaseVersion v);
    static void writeCategoryToDatabase(const Category &category, QDataStream &dataStream, DatabaseVersion v);

    static void readItemTypeFromDatabase(ItemType &itt, QDataStream &dataStream, DatabaseVersion v);
    static void writeItemTypeToDatabase(const ItemType &itemType, QDataStream &dataStream, DatabaseVersion v);

    static void readItemFromDatabase(Item &item, QDataStream &dataStream, DatabaseVersion v);
    static void writeItemToDatabase(const Item &item, QDataStream &dataStream, DatabaseVersion v);

    void readPCCFromDatabase(PartColorCode &pcc, QDataStream &dataStream, Core::DatabaseVersion v) const;
    static void writePCCToDatabase(const PartColorCode &pcc, QDataStream &dataStream, DatabaseVersion v);

    void readItemChangeLogFromDatabase(ItemChangeLogEntry &e, QDataStream &dataStream, Core::DatabaseVersion v) const;
    static void writeItemChangeLogToDatabase(const ItemChangeLogEntry &e, QDataStream &dataStream, DatabaseVersion v);
    void readColorChangeLogFromDatabase(ColorChangeLogEntry &e, QDataStream &dataStream, Core::DatabaseVersion v) const;
    static void writeColorChangeLogToDatabase(const ColorChangeLogEntry &e, QDataStream &dataStream, DatabaseVersion v);

private slots:
    void pictureJobFinished(TransferJob *j, BrickLink::Picture *pic);
    void priceGuideJobFinished(TransferJob *j, BrickLink::PriceGuide *pg);

    void priceGuideLoaded(BrickLink::PriceGuide *pg);
    void pictureLoaded(BrickLink::Picture *pic);

    friend class PriceGuideLoaderJob;
    friend class PictureLoaderJob;

public: // semi-public for the QML wrapper
    QPair<int, int> pictureCacheStats() const;
    QPair<int, int> priceGuideCacheStats() const;

private:
    void clear();

    QString  m_datadir;
    bool     m_online = false;

    QIcon                           m_noImageIcon;
    mutable QHash<uint, QImage>     m_noImageCache;
    mutable QHash<uint, QImage>     m_colorImageCache;

    std::vector<Color>         m_colors;
    std::vector<Category>      m_categories;
    std::vector<ItemType>      m_item_types;
    std::vector<Item>          m_items;
    std::vector<ItemChangeLogEntry>  m_itemChangelog;
    std::vector<ColorChangeLogEntry> m_colorChangelog;
    std::vector<PartColorCode> m_pccs;

    Transfer *                 m_transfer = nullptr;
    Transfer *                 m_authenticatedTransfer = nullptr;
    bool                       m_authenticated = false;
    QPair<QString, QString>    m_credentials;
    TransferJob *              m_loginJob = nullptr;
    QVector<TransferJob *>     m_jobsWaitingForAuthentication;

    int                          m_db_update_iv = 0;
    QDateTime                    m_databaseDate;

    int                          m_pg_update_iv = 0;
    Q3Cache<quint64, PriceGuide> m_pg_cache;

    int                          m_pic_update_iv = 0;
    QThreadPool                  m_diskloadPool;
    Q3Cache<quint64, Picture>    m_pic_cache;

    qreal m_item_image_scale_factor = 1.;

    QString m_ldraw_datadir;

    Store *m_store = nullptr;
    Orders *m_orders = nullptr;
    Carts *m_carts = nullptr;

    friend class TextImport;
};

inline Core *core() { return Core::inst(); }
inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

} // namespace BrickLink

