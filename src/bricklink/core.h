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

public:
    ~Core() override;

    void openUrl(UrlList u, const void *opt = nullptr, const void *opt2 = nullptr);

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
    Database *database() const  { return m_database; }

    inline const std::vector<Color> &colors() const         { return database()->m_colors; }
    inline const std::vector<Category> &categories() const  { return database()->m_categories; }
    inline const std::vector<ItemType> &itemTypes() const   { return database()->m_itemTypes; }
    inline const std::vector<Item> &items() const           { return database()->m_items; }
    inline const std::vector<PartColorCode> &pccs() const   { return database()->m_pccs; }
    inline const std::vector<ItemChangeLogEntry>  &itemChangelog() const  { return database()->m_itemChangelog; }
    inline const std::vector<ColorChangeLogEntry> &colorChangelog() const { return database()->m_colorChangelog; }

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

    enum class ResolveResult { Fail, Direct, ChangeLog };
    ResolveResult resolveIncomplete(Lot *lot);

public slots:
    void setOnlineStatus(bool on);
    void setUpdateIntervals(const QMap<QByteArray, int> &intervals);

    void cancelTransfers();

signals:
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

    void beginResetDatabase();
    void endResetDatabase();

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

    Transfer *                 m_transfer = nullptr;
    Transfer *                 m_authenticatedTransfer = nullptr;
    bool                       m_authenticated = false;
    QPair<QString, QString>    m_credentials;
    TransferJob *              m_loginJob = nullptr;
    QVector<TransferJob *>     m_jobsWaitingForAuthentication;

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
    Database *m_database = nullptr;
};

inline Core *core() { return Core::inst(); }
inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

} // namespace BrickLink

