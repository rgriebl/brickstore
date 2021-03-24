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
#include <cstdio>
#include <cstdlib>

#include <QFile>
#include <QBuffer>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QThread>
#include <QUrlQuery>
#include <QStringBuilder>
#include <QDesktopServices>

#include "config.h"
#include "utility.h"
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#  include <qhashfunctions.h>
#  define q5Hash qHash
#else
#  include "q5hashfunctions.h"
#endif
#include "stopwatch.h"
#include "bricklink.h"
#include "chunkreader.h"
#include "chunkwriter.h"
#include "exception.h"


namespace BrickLink {

void Core::openUrl(UrlList u, const void *opt, const void *opt2)
{
    QUrl url;

    switch (u) {
    case URL_InventoryRequest:
        url = "https://www.bricklink.com/catalogInvAdd.asp"_l1;
        break;

    case URL_WantedListUpload:
        url = "https://www.bricklink.com/wantedXML.asp"_l1;
        break;

    case URL_InventoryUpload:
        url = "https://www.bricklink.com/invXML.asp"_l1;
        break;

    case URL_InventoryUpdate:
        url = "https://www.bricklink.com/invXML.asp#update"_l1;
        break;

    case URL_CatalogInfo: {
        auto item = static_cast<const Item *>(opt);
        if (item && item->itemType()) {
            url = "https://www.bricklink.com/catalogItem.asp"_l1;
            QUrlQuery query;
            query.addQueryItem(QString(QLatin1Char(item->itemType()->id())), QLatin1String(item->id()));
            if (item->itemType()->hasColors() && opt2)
                query.addQueryItem("C"_l1, QString::number(static_cast<const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_PriceGuideInfo: {
        auto *item = static_cast<const Item *>(opt);
        if (item && item->itemType()) {
            url = "https://www.bricklink.com/catalogPG.asp"_l1;
            QUrlQuery query;
            query.addQueryItem(QString(QLatin1Char(item->itemType()->id())), QLatin1String(item->id()));
            if (item->itemType()->hasColors() && opt2)
                query.addQueryItem("colorID"_l1, QString::number(static_cast<const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_LotsForSale: {
        auto item = static_cast<const Item *>(opt);
        if (item && item->itemType()) {
            url = "https://www.bricklink.com/search.asp"_l1;
            QUrlQuery query;
            query.addQueryItem("viewFrom"_l1, "sa"_l1);
            query.addQueryItem("itemType"_l1, QString(QLatin1Char(item->itemType()->id())));

            // workaround for BL not accepting the -X suffix for sets, instructions and boxes
            QString id = QLatin1String(item->id());
            char itt = item->itemType()->id();

            if (itt == 'S' || itt == 'I' || itt == 'O') {
                int pos = id.lastIndexOf('-'_l1);
                if (pos >= 0)
                    id.truncate(pos);
            }
            query.addQueryItem("q"_l1, id);

            if (item->itemType()->hasColors() && opt2)
                query.addQueryItem("colorID"_l1, QString::number(static_cast<const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_AppearsInSets: {
        auto item = static_cast<const Item *>(opt);
        if (item && item->itemType()) {
            url = "https://www.bricklink.com/catalogItemIn.asp"_l1;
            QUrlQuery query;
            query.addQueryItem(QString(QLatin1Char(item->itemType()->id())), QLatin1String(item->id()));
            query.addQueryItem("in"_l1, "S"_l1);

            if (item->itemType()->hasColors() && opt2)
                query.addQueryItem("colorID"_l1, QString::number(static_cast<const Color *>(opt2)->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_ColorChangeLog:
        url = "https://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=R"_l1;
        break;

    case URL_ItemChangeLog: {
        url = "https://www.bricklink.com/catalogReqList.asp?pg=1&chgUserID=&viewActionType=I"_l1;
        QUrlQuery query;
        if (opt)
            query.addQueryItem("q"_l1, QLatin1String(static_cast<const char *>(opt)));
        url.setQuery(query);
        break;
    }
    case URL_StoreItemDetail: {
        auto lotId = static_cast<const unsigned int *>(opt);
        if (lotId && *lotId) {
            url = "https://www.bricklink.com/inventory_detail.asp"_l1;
            QUrlQuery query;
            query.addQueryItem("invID"_l1, QString::number(*lotId));
            url.setQuery(query);
        }
        break;
    }
    case URL_StoreItemSearch: {
        const Item *item = static_cast<const Item *>(opt);
        const Color *color = static_cast<const Color *>(opt2);
        if (item && item->itemType()) {
            url = "https://www.bricklink.com/inventory_detail.asp?"_l1;
            QUrlQuery query;
            query.addQueryItem("catType"_l1, QString(QLatin1Char(item->itemType()->id())));
            query.addQueryItem("q"_l1, QLatin1String(item->id()));
            if (item->itemType()->hasColors() && color)
                query.addQueryItem("ColorID"_l1, QString::number(color->id()));
            url.setQuery(query);
        }
        break;
    }
    case URL_OrderDetails: {
        auto orderId = static_cast<const char *>(opt);
        if (orderId && *orderId) {
            url = "https://www.bricklink.com/orderDetail.asp"_l1;
            QUrlQuery query;
            query.addQueryItem("ID"_l1, QString::fromLatin1(orderId));
            url.setQuery(query);
        }
        break;
    }
    case URL_ShoppingCart: {
        auto shopId = static_cast<const int *>(opt);
        if (shopId && *shopId) {
            url = "https://www.bricklink.com/v2/globalcart.page"_l1;
            QUrlQuery query;
            query.addQueryItem("sid"_l1, QString::number(*shopId));
            url.setQuery(query);
        }
        break;
    }
    }
    if (url.isValid())
        QDesktopServices::openUrl(url);
}


const QImage Core::noImage(const QSize &s) const
{
    uint key = uint(s.width() << 16) | uint(s.height());
    QImage img = m_noImageCache.value(key);

    if (img.isNull()) {
        img = m_noImageIcon.pixmap(s).toImage();
        m_noImageCache.insert(key, img);
    }
    return img;
}


const QImage Core::colorImage(const Color *col, int w, int h) const
{
    if (!col || w <= 0 || h <= 0)
        return QImage();

    //TODO for Qt6: use more than 10/11/11 bits ... qHash is 64bit wide now
    uint key = uint(col->id() << 22) | uint(w << 11) | uint(h);
    QImage img = m_colorImageCache.value(key);

    if (img.isNull()) {
        QColor c = col->color();

        img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&img);
        p.setRenderHints(QPainter::Antialiasing);
        QRect r = img.rect();

        QBrush brush;

        if (col->isGlitter()) {
            brush = QBrush(Utility::contrastColor(c, 0.25), Qt::Dense6Pattern);
        }
        else if (col->isSpeckle()) {
            // hack for speckled colors
            QColor c2;

            if (!c.isValid()) {
                QString name = col->name();
                int dash = name.indexOf('-'_l1);
                if (dash > 0) {
                    QString basename = name.mid(8, dash - 8);
                    if (basename.startsWith("DB"_l1))
                        basename.replace(0, 2, "Dark Bluish "_l1);
                    QString speckname = name.mid(dash + 1);

                    const Color *basec = colorFromName(basename);
                    const Color *speckc = colorFromName(speckname);

                    if (basec)
                        c = basec->color();
                    if (speckc)
                        c2 = speckc->color();
                }
            }
            if (c.isValid()) {
                if (!c2.isValid()) // fake
                    c2 = Utility::contrastColor(c, 0.2);
                brush = QBrush(c2, Qt::Dense7Pattern);
            }
        }
        else if (col->isMetallic()) {
            QColor c2 = Utility::gradientColor(c, Qt::black);

            QLinearGradient gradient(0, 0, 0, r.height());
            gradient.setColorAt(0,   c2);
            gradient.setColorAt(0.3, c);
            gradient.setColorAt(0.7, c);
            gradient.setColorAt(1,   c2);
            brush = gradient;
        }
        else if (col->isChrome()) {
            QColor c2 = Utility::gradientColor(c, Qt::black);

            QLinearGradient gradient(0, 0, r.width(), 0);
            gradient.setColorAt(0,   c2);
            gradient.setColorAt(0.3, c);
            gradient.setColorAt(0.7, c);
            gradient.setColorAt(1,   c2);
            brush = gradient;
        }

        if (c.isValid()) {
            p.fillRect(r, c);
            p.fillRect(r, brush);
        }
        else {
            p.fillRect(0, 0, w, h, Qt::white);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::darkGray);
            p.setBrush(QColor(255, 255, 255, 128));
            p.drawRect(r);
            p.drawLine(0, 0, w, h);
            p.drawLine(0, h, w, 0);
        }

        if (col->isTransparent()) {
            QLinearGradient gradient(0, 0, r.width(), r.height());
            gradient.setColorAt(0,   Qt::transparent);
            gradient.setColorAt(0.4, Qt::transparent);
            gradient.setColorAt(0.6, QColor(255, 255, 255, 100));
            gradient.setColorAt(1,   QColor(255, 255, 255, 100));

            p.fillRect(r, gradient);
        }
        p.end();

        m_colorImageCache.insert(key, img);
    }
    return img;
}



namespace {

static bool check_and_create_path(const QString &p)
{
    QFileInfo fi(p);

    if (!fi.exists()) {
        QDir(p).mkpath(QStringLiteral("."));
        fi.refresh();
    }
    return (fi.exists() && fi.isDir() && fi.isReadable() && fi.isWritable());
}

} // namespace

QString Core::dataPath() const
{
    return m_datadir;
}

QFile *Core::dataFile(QStringView fileName, QIODevice::OpenMode openMode,
                       const Item *item, const Color *color) const
{
    // Avoid huge directories with 1000s of entries.
    // sse4.2 is only used if a seed value is supplied
    // please note: Qt6's qHash is incompatible
    uchar hash = q5Hash(QString::fromLatin1(item->id()), 42) & 0xff;

    QString p = m_datadir % QLatin1Char(item->itemType()->id()) % u'/' % (hash < 0x10 ? u"0" : u"")
            % QString::number(hash, 16) % u'/' % QLatin1String(item->id()) % u'/'
            % (color ? QString::number(color->id()) : QString()) % (color ? u"/" : u"")
            % fileName;

    if (openMode != QIODevice::ReadOnly) {
        if (!QDir(fileName.isEmpty() ? p : p.left(p.size() - int(fileName.size()))).mkpath("."_l1))
            return nullptr;
    }
    auto f = new QFile(p);
    if (!f->open(openMode)) {
        if (openMode & QIODevice::WriteOnly) {
            qWarning() << "BrickLink::Core::dataFile failed to open" << p << "for writing:"
                       << f->errorString();
        }
    }
    return f;
}



Core *Core::s_inst = nullptr;

Core *Core::create(const QString &datadir, QString *errstring)
{
    if (!s_inst) {
        s_inst = new Core(datadir);
        QString test = s_inst->dataPath();

        if (test.isNull() || !check_and_create_path(test)) {
            delete s_inst;
            s_inst = nullptr;

            if (errstring)
                *errstring = tr("Data directory \'%1\' is not both read- and writable.").arg(datadir);
        }
    }
    return s_inst;
}

Core::Core(const QString &datadir)
    : m_datadir(QDir::cleanPath(QDir(datadir).absolutePath()) + u'/')
    , m_noImageIcon(QIcon::fromTheme("image-missing-large"_l1))
{
    m_diskloadPool.setMaxThreadCount(QThread::idealThreadCount() * 3);
    m_online = true;

    // cache size is physical memory * 0.25, at least 128MB, at most 1GB
    quint64 cachemem = qBound(128ULL *1024*1024, Utility::physicalMemory() / 4, 1024ULL *1024*1024);
    //quint64 cachemem = 1024*1024; // DEBUG

    m_pg_cache.setMaxCost(10000);            // each priceguide has a cost of 1
    m_pic_cache.setMaxCost(int(cachemem));   // each pic has roughly the cost of memory used
}

Core::~Core()
{
    clear();

    delete m_transfer;
    s_inst = nullptr;
}

void Core::setTransfer(Transfer *trans)
{
    Transfer *old = m_transfer;

    m_transfer = trans;

    if (old) { // disconnect
        disconnect(old, &Transfer::finished, this, &Core::priceGuideJobFinished);
        disconnect(old, &Transfer::finished, this, &Core::pictureJobFinished);

        disconnect(old, &Transfer::progress, this, &Core::transferJobProgress);
    }
    if (trans) { // connect
        connect(trans, &Transfer::finished, this, &Core::priceGuideJobFinished);
        connect(trans, &Transfer::finished, this, &Core::pictureJobFinished);

        connect(trans, &Transfer::progress, this, &Core::transferJobProgress);
    }
}

Transfer *Core::transfer() const
{
    return m_transfer;
}

void Core::setUpdateIntervals(const QMap<QByteArray, int> &intervals)
{
    m_db_update_iv = intervals["Database"];
    m_pic_update_iv = intervals["Picture"];
    m_pg_update_iv = intervals["PriceGuide"];
}

bool Core::updateNeeded(bool valid, const QDateTime &last, int iv)
{
    return (iv > 0) && (!valid || (last.secsTo(QDateTime::currentDateTime()) > iv));
}

void Core::setOnlineStatus(bool on)
{
    m_online = on;
}

bool Core::onlineStatus() const
{
    return m_online;
}

QString Core::countryIdFromName(const QString &name) const
{
    // BrickLink doesn't use the standard ISO country names...
    static const char * const brickLinkCountries[] = {
        "AF Afghanistan",
        "AL Albania",
        "DZ Algeria",
        "AD Andorra",
        "AO Angola",
        "AI Anguilla",
        "AG Antigua and Barbuda",
        "AR Argentina",
        "AM Armenia",
        "AW Aruba",
        "AU Australia",
        "AT Austria",
        "AZ Azerbaijan",
        "BS Bahamas",
        "BH Bahrain",
        "BD Bangladesh",
        "BB Barbados",
        "BY Belarus",
        "BE Belgium",
        "BZ Belize",
        "BJ Benin",
        "BM Bermuda",
        "BT Bhutan",
        "BO Bolivia",
        "BA Bosnia and Herzegovina",
        "BW Botswana",
        "BR Brazil",
        "IO British Indian Ocean Territory",
        "BN Brunei",
        "BG Bulgaria",
        "BF Burkina Faso",
        "BI Burundi",
        "KH Cambodia",
        "CM Cameroon",
        "CA Canada",
        "CV Cape Verde",
        "BQ Caribbean Netherlands",
        "KY Cayman Islands",
        "CF Central African Republic",
        "TD Chad",
        "CL Chile",
        "CN China",
        "CO Colombia",
        "KM Comoros",
        "CG Congo",
        "CD Congo (DRC)",
        "CK Cook Islands",
        "CR Costa Rica",
        "CI Cote D\'Ivoire",
        "HR Croatia",
        "CW Curacao",
        "CY Cyprus",
        "CZ Czech Republic",
        "DK Denmark",
        "DJ Djibouti",
        "DM Dominica",
        "DO Dominican Republic",
        "TL East Timor",
        "EC Ecuador",
        "EG Egypt",
        "SV El Salvador",
        "GQ Equatorial Guinea",
        "ER Eritrea",
        "EE Estonia",
        "ET Ethiopia",
        "FK Falkland Islands (Islas Malvinas)",
        "FO Faroe Islands",
        "FJ Fiji",
        "FI Finland",
        "FR France",
        "PF French Polynesia",
        "GA Gabon",
        "GM Gambia",
        "GE Georgia",
        "DE Germany",
        "GH Ghana",
        "GI Gibraltar",
        "GR Greece",
        "GL Greenland",
        "GD Grenada",
        "GT Guatemala",
        "GN Guinea",
        "GW Guinea-Bissau",
        "GY Guyana",
        "HT Haiti",
        "HN Honduras",
        "HK Hong Kong",
        "HU Hungary",
        "IS Iceland",
        "IN India",
        "ID Indonesia",
        "IQ Iraq",
        "IE Ireland",
        "IL Israel",
        "IT Italy",
        "JM Jamaica",
        "JP Japan",
        "JO Jordan",
        "KZ Kazakhstan",
        "KE Kenya",
        "KI Kiribati",
        "KW Kuwait",
        "KG Kyrgyzstan",
        "LA Laos",
        "LV Latvia",
        "LB Lebanon",
        "LS Lesotho",
        "LR Liberia",
        "LI Liechtenstein",
        "LT Lithuania",
        "LU Luxembourg",
        "LY Lybia",
        "MO Macau",
        "MK Macedonia",
        "MG Madagascar",
        "MW Malawi",
        "MY Malaysia",
        "MV Maldives",
        "ML Mali",
        "MT Malta",
        "MH Marshall Islands",
        "MR Mauritania",
        "MU Mauritius",
        "YT Mayotte",
        "MX Mexico",
        "FM Micronesia",
        "MD Moldova",
        "MC Monaco",
        "MN Mongolia",
        "ME Montenegro",
        "MS Montserrat",
        "MA Morocco",
        "MZ Mozambique",
        "MM Myanmar",
        "NA Namibia",
        "NR Nauru",
        "NP Nepal",
        "NL Netherlands",
        "NC New Caledonia",
        "NZ New Zealand",
        "NI Nicaragua",
        "NE Niger",
        "NU Niue",
        "NF Norfolk Island",
        "NO Norway",
        "OM Oman",
        "PK Pakistan",
        "PW Palau",
        "PA Panama",
        "PG Papua new Guinea",
        "PY Paraguay",
        "PE Peru",
        "PH Philippines",
        "PN Pitcairn Islands",
        "PL Poland",
        "PT Portugal",
        "QA Qatar",
        "RO Romania",
        "RU Russia",
        "RW Rwanda",
        "WS Samoa",
        "SM San Marino",
        "ST Sao Tome and Principe",
        "SA Saudi Arabia",
        "SN Senegal",
        "RS Serbia",
        "SC Seychelles",
        "SL Sierra Leone",
        "SG Singapore",
        "SX Sint Maarten",
        "SK Slovakia",
        "SI Slovenia",
        "SB Solomon Islands",
        "SO Somalia",
        "ZA South Africa",
        "GS South Georgia",
        "KR South Korea",
        "ES Spain",
        "LK Sri Lanka",
        "SH St. Helena",
        "KN St. Kitts and Nevis",
        "LC St. Lucia",
        "PM St. Pierre and Miquelon",
        "VC St. Vincent and the Grenadines",
        "SD Sudan",
        "SR Suriname",
        "SJ Svalbard and Jan Mayen",
        "SZ Swaziland",
        "SE Sweden",
        "CH Switzerland",
        "TW Taiwan",
        "TJ Tajikistan",
        "TZ Tanzania",
        "TH Thailand",
        "TG Togo",
        "TO Tonga",
        "TT Trinidad and Tobago",
        "TN Tunisia",
        "TR Turkey",
        "TM Turkmenistan",
        "TC Turks and Caicos Islands",
        "TV Tuvalu",
        "UG Uganda",
        "UA Ukraine",
        "AE United Arab Emirates",
        "UK United Kingdom",
        "UY Uruguay",
        "US USA",
        "UZ Uzbekistan",
        "VU Vanuatu",
        "VA Vatican City State",
        "VE Venezuela",
        "VN Vietnam",
        "VG Virgin Islands (British)",
        "WF Wallis and Futuna",
        "YE Yemen",
        "ZM Zambia",
        "ZW Zimbabwe",
    };

    if (!name.isEmpty()) {
        for (const auto &country : brickLinkCountries) {
            QString countryStr = QString::fromLatin1(country);
            if (countryStr.mid(3) == name)
                return countryStr.left(2);
        }
    }
    return { };
}


const std::vector<const Color *> &Core::colors() const
{
    return m_colors;
}

const std::vector<const Category *> &Core::categories() const
{
    return m_categories;
}

const std::vector<const ItemType *> &Core::itemTypes() const
{
    return m_item_types;
}

const std::vector<const Item *> &Core::items() const
{
    return m_items;
}

const Category *Core::category(uint id) const
{
    auto it = std::lower_bound(m_categories.cbegin(), m_categories.cend(), id, &Category::lowerBound);
    if ((it != m_categories.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const Color *Core::color(uint id) const
{
    auto it = std::lower_bound(m_colors.cbegin(), m_colors.cend(), id, &Color::lowerBound);
    if ((it != m_colors.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const Color *Core::colorFromName(const QString &name) const
{
    if (name.isEmpty())
        return nullptr;

    auto it = std::find_if(m_colors.cbegin(), m_colors.cend(), [name](const auto &color) {
        return !color->name().compare(name, Qt::CaseInsensitive);
    });
    if (it != m_colors.cend())
        return *it;
    return nullptr;
}


const Color *Core::colorFromLDrawId(int ldrawId) const
{
    auto it = std::find_if(m_colors.cbegin(), m_colors.cend(), [ldrawId](const auto &color) {
        return (color->ldrawId() == ldrawId);
    });
    if (it != m_colors.cend())
        return *it;
    return nullptr;
}


const ItemType *Core::itemType(char id) const
{
    auto it = std::lower_bound(m_item_types.cbegin(), m_item_types.cend(), id, &ItemType::lowerBound);
    if ((it != m_item_types.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const Item *Core::item(char tid, const QByteArray &id) const
{
    auto needle = std::make_pair(tid, id);
    auto it = std::lower_bound(m_items.cbegin(), m_items.cend(), needle, Item::lowerBound);
    if ((it != m_items.cend()) && ((*it)->itemType()->id() == tid) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

const PartColorCode *Core::partColorCode(uint id)
{
    auto it = std::lower_bound(m_pccs.cbegin(), m_pccs.cend(), id, &PartColorCode::lowerBound);
    if ((it != m_pccs.cend()) && ((*it)->id() == id))
        return *it;
    return nullptr;
}

void Core::cancelTransfers()
{
    if (m_transfer)
        m_transfer->abortAllJobs();
    m_diskloadPool.clear();
    m_diskloadPool.waitForDone();
}

QString Core::defaultDatabaseName(DatabaseVersion version) const
{
    return u"database-v" % QString::number(int(version));
}

QDateTime Core::databaseDate() const
{
    return m_databaseDate;
}

bool Core::isDatabaseUpdateNeeded() const
{
    return updateNeeded(m_databaseDate.isValid(), m_databaseDate, m_db_update_iv);
}

void Core::clear()
{
    cancelTransfers();

    m_pg_cache.clear();
    m_pic_cache.clear();

    qDeleteAll(m_colors);
    qDeleteAll(m_item_types);
    qDeleteAll(m_categories);
    qDeleteAll(m_items);
    qDeleteAll(m_pccs);

    m_colors.clear();
    m_item_types.clear();
    m_categories.clear();
    m_items.clear();
    m_pccs.clear();
    m_changelog.clear();
}


//
// Database (de)serialization
//

bool Core::readDatabase(QString *infoText, const QString &filename)
{
    try {
        clear();

        stopwatch *sw = nullptr; //new stopwatch("Core::readDatabase()");

        QDateTime generationDate;
        QString info;
        if (infoText)
            infoText->clear();

        QFile f(!filename.isEmpty() ? filename : dataPath() + defaultDatabaseName());

        if (!f.open(QFile::ReadOnly))
            throw Exception(&f, "could not open database for reading");

        const char *data = reinterpret_cast<char *>(f.map(0, f.size()));

        if (!data)
            throw Exception("could not memory map the database (%1)").arg(f.fileName());

        QByteArray ba = QByteArray::fromRawData(data, int(f.size()));
        QBuffer buf(&ba);
        buf.open(QIODevice::ReadOnly);
        ChunkReader cr(&buf, QDataStream::LittleEndian);
        QDataStream &ds = cr.dataStream();

        if (!cr.startChunk() || cr.chunkId() != ChunkId('B','S','D','B'))
            throw Exception("invalid database format - wrong magic (%1)").arg(f.fileName());

        if (cr.chunkVersion() != int(DatabaseVersion::Latest)) {
            throw Exception("invalid database version: expected %1, but got %2")
                .arg(int(DatabaseVersion::Latest)).arg(cr.chunkVersion());
        }

        bool gotColors = false, gotCategories = false, gotItemTypes = false, gotItems = false;
        bool gotChangeLog = false, gotPccs = false;

        auto check = [&ds, &f]() {
            if (ds.status() != QDataStream::Ok)
                throw Exception("failed to read from database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
        };

        while (cr.startChunk()) {
            switch (cr.chunkId() | quint64(cr.chunkVersion()) << 32) {
            case ChunkId('I','N','F','O') | 1ULL << 32: {
                ds >> info;
                break;
            }
            case ChunkId('D','A','T','E') | 1ULL << 32: {
                ds >> generationDate;
                break;
            }
            case ChunkId('C','O','L',' ') | 1ULL << 32: {
                quint32 colc = 0;
                ds >> colc;
                check();

                for (quint32 i = colc; i; i--) {
                    auto *col = readColorFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_colors.emplace_back(col);
                }
                gotColors = true;
                break;
            }
            case ChunkId('C','A','T',' ') | 1ULL << 32: {
                quint32 catc = 0;
                ds >> catc;
                check();

                for (quint32 i = catc; i; i--) {
                    auto *cat = readCategoryFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_categories.emplace_back(cat);
                }
                gotCategories = true;
                break;
            }
            case ChunkId('T','Y','P','E') | 1ULL << 32: {
                quint32 ittc = 0;
                ds >> ittc;
                check();

                for (quint32 i = ittc; i; i--) {
                    auto *itt = readItemTypeFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_item_types.emplace_back(itt);
                }
                gotItemTypes = true;
                break;
            }
            case ChunkId('I','T','E','M') | 1ULL << 32: {
                quint32 itc = 0;
                ds >> itc;
                check();

                m_items.reserve(itc);
                for (quint32 i = itc; i; i--) {
                    auto *item = readItemFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_items.emplace_back(item);
                }
                gotItems = true;
                break;
            }
            case ChunkId('C','H','G','L') | 1ULL << 32: {
                quint32 clc = 0;
                ds >> clc;
                check();

                m_changelog.reserve(clc);
                for (quint32 i = clc; i; i--) {
                    QByteArray entry;
                    ds >> entry;
                    check();
                    m_changelog.emplace_back(entry);
                }
                gotChangeLog = true;
                break;
            }
            case ChunkId('P','C','C',' ') | 1ULL << 32: {
                if (!gotItems || !gotColors)
                    throw Exception("found a 'PCC ' chunk before the 'ITEM' and 'COL ' chunks");

                quint32 pccc = 0;
                ds >> pccc;
                check();

                m_pccs.reserve(pccc);
                for (quint32 i = pccc; i; i--) {
                    auto *pcc = readPCCFromDatabase(ds, DatabaseVersion::Latest);
                    check();
                    m_pccs.emplace_back(pcc);
                }
                gotPccs = true;
                break;
            }
            default: {
                cr.skipChunk();
                check();
                break;
            }
            }
            if (!cr.endChunk()) {
                throw Exception("missed the end of a chunk when reading from database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
            }
        }
        if (!cr.endChunk()) {
            throw Exception("missed the end of the root chunk when reading from database (%1) at position %2")
                .arg(f.fileName()).arg(f.pos());
        }

        delete sw;

        if (!gotColors || !gotCategories || !gotItemTypes || !gotItems || !gotChangeLog || !gotPccs) {
            throw Exception("not all required data chunks were found in the database (%1)")
                .arg(f.fileName());
        }

        qDebug().noquote() << "Loaded database from" << f.fileName()
                 << "\n  Generated at:" << QLocale().toString(generationDate)
                 << "\n  Colors      :" << m_colors.size()
                 << "\n  Item Types  :" << m_item_types.size()
                 << "\n  Categories  :" << m_categories.size()
                 << "\n  Items       :" << m_items.size()
                 << "\n  PCCs        :" << m_pccs.size()
                 << "\n  ChangeLog   :" << m_changelog.size();

        m_databaseDate = generationDate;
        emit databaseDateChanged(generationDate);

        if (!info.isEmpty())
            qDebug() << "Info :" << info;
        if (infoText)
            *infoText = info;
        return true;

    } catch (const Exception &e) {
        qWarning() << "Error reading database:" << e.what();
        clear();
        return false;
    }
}


bool Core::writeDatabase(const QString &filename, DatabaseVersion version,
                         const QString &infoText) const
{
    if (version <= DatabaseVersion::Invalid)
        return false; // too old

    QString fn(!filename.isEmpty() ? filename : dataPath() + defaultDatabaseName(version));

    try {
        QSaveFile f(fn);
        if (!f.open(QIODevice::WriteOnly))
            throw Exception(&f, "could not open database for writing");

        ChunkWriter cw(&f, QDataStream::LittleEndian);
        QDataStream &ds = cw.dataStream();

        auto check = [&ds, &f](bool ok) {
            if (!ok || (ds.status() != QDataStream::Ok))
                throw Exception("failed to write to database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
        };

        check(cw.startChunk(ChunkId('B','S','D','B'), uint(version)));

        if (!infoText.isEmpty()) {
            check(cw.startChunk(ChunkId('I','N','F','O'), 1));
            ds << infoText;
            check(cw.endChunk());
        }

        check(cw.startChunk(ChunkId('D','A','T','E'), 1));
        ds << QDateTime::currentDateTimeUtc();
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','O','L',' '), 1));
        ds << quint32(m_colors.size());
        for (const Color *col : m_colors)
            writeColorToDatabase(col, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','A','T',' '), 1));
        ds << quint32(m_categories.size());
        for (const Category *cat : m_categories)
            writeCategoryToDatabase(cat, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('T','Y','P','E'), 1));
        ds << quint32(m_item_types.size());
        for (const ItemType *itt : m_item_types)
            writeItemTypeToDatabase(itt, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('I','T','E','M'), 1));
        ds << quint32(m_items.size());
        for (const Item *item : m_items)
            writeItemToDatabase(item, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','H','G','L'), 1));
        ds << quint32(m_changelog.size());
        for (const QByteArray &cl : m_changelog)
            ds << cl;
        check(cw.endChunk());

        if (version >= DatabaseVersion::Version_3) {
            check(cw.startChunk(ChunkId('P','C','C',' '), 1));
            ds << quint32(m_pccs.size());
            for (const PartColorCode *pcc : m_pccs)
                writePCCToDatabase(pcc, ds, version);
            check(cw.endChunk());
        }

        check(cw.endChunk()); // BSDB root chunk

        if (!f.commit())
            throw Exception(f.errorString());

        return true;

    } catch (const Exception &e) {
        qWarning() << "Error writing database:" << e.what();
        return false;
    }
}


Color *Core::readColorFromDatabase(QDataStream &dataStream, DatabaseVersion)
{
    QScopedPointer<Color> col(new Color);

    quint32 flags;
    float popularity;
    QByteArray name;

    dataStream >> col->m_id >> name >> col->m_ldraw_id >> col->m_color >> flags >> popularity
            >> col->m_year_from >> col->m_year_to;

    col->m_name = QString::fromUtf8(name);
    col->m_type = static_cast<Color::Type>(flags);
    col->m_popularity = qreal(popularity);

    return col.take();
}

void Core::writeColorToDatabase(const Color *col, QDataStream &dataStream, DatabaseVersion)
{
    dataStream << col->m_id << col->m_name.toUtf8() << col->m_ldraw_id << col->m_color
               << quint32(col->m_type) << float(col->m_popularity)
               << col->m_year_from << col->m_year_to;
}


Category *Core::readCategoryFromDatabase(QDataStream &dataStream, DatabaseVersion)
{
    QScopedPointer<Category> cat(new Category);

    QByteArray name;
    quint32 id;
    dataStream >> id >> name;
    cat->m_id = id;
    cat->m_name = QString::fromUtf8(name);

    return cat.take();
}

void Core::writeCategoryToDatabase(const Category *cat, QDataStream &dataStream, DatabaseVersion)
{
    dataStream << quint32(cat->m_id) << cat->m_name.toUtf8();
}


ItemType *Core::readItemTypeFromDatabase(QDataStream &dataStream, DatabaseVersion)
{
    QScopedPointer<ItemType> itt(new ItemType);

    quint8 flags = 0;
    qint32 catcount = 0;
    qint8 id = 0, picid = 0;
    QByteArray name;
    dataStream >> id >> picid >> name >> flags >> catcount;

    itt->m_name = QString::fromUtf8(name);
    itt->m_id = id;
    itt->m_picture_id = id;

    itt->m_categories.resize(catcount);

    for (int i = 0; i < catcount; i++) {
        quint32 catid = 0;
        dataStream >> catid;
        itt->m_categories[i] = BrickLink::core()->category(catid);
    }

    itt->m_has_inventories   = flags & 0x01;
    itt->m_has_colors        = flags & 0x02;
    itt->m_has_weight        = flags & 0x04;
    itt->m_has_year          = flags & 0x08;
    itt->m_has_subconditions = flags & 0x10;

    return itt.take();
}

void Core::writeItemTypeToDatabase(const ItemType *itt, QDataStream &dataStream, DatabaseVersion)
{
    quint8 flags = 0;
    flags |= (itt->m_has_inventories   ? 0x01 : 0);
    flags |= (itt->m_has_colors        ? 0x02 : 0);
    flags |= (itt->m_has_weight        ? 0x04 : 0);
    flags |= (itt->m_has_year          ? 0x08 : 0);
    flags |= (itt->m_has_subconditions ? 0x10 : 0);

    dataStream << qint8(itt->m_id) << qint8(itt->m_picture_id) << itt->m_name.toUtf8() << flags;

    dataStream << qint32(itt->m_categories.size());
    for (const BrickLink::Category *cat : itt->m_categories)
        dataStream << quint32(cat->id());
}


Item *Core::readItemFromDatabase(QDataStream &dataStream, DatabaseVersion)
{
    QScopedPointer<Item> item(new Item);

    qint8 ittid = 0;
    quint32 catid = 0;
    QByteArray id;
    QByteArray name;

    dataStream >> id >> name >> ittid >> catid;
    item->m_id = id;
    item->m_name = QString::fromUtf8(name);
    item->m_item_type = BrickLink::core()->itemType(ittid);
    item->m_category = BrickLink::core()->category(catid);

    quint32 colorid = 0;
    quint32 index = 0, year = 0;
    qint64 invupd = 0;
    dataStream >> colorid >> invupd >> item->m_weight >> index >> year;
    item->m_color = BrickLink::core()->color(colorid);
    item->m_index = index;
    item->m_year = year;
    item->m_last_inv_update = invupd;

    quint32 appears = 0, appears_size = 0;
    dataStream >> appears;
    if (appears)
        dataStream >> appears_size;

    if (appears && appears_size) {
        auto *ptr = new quint32 [appears_size];
        item->m_appears_in = ptr;

        *ptr++ = appears;
        *ptr++ = appears_size;

        for (quint32 i = 2; i < appears_size; i++)
            dataStream >> *ptr++;
    }
    else
        item->m_appears_in = nullptr;

    quint32 consists = 0;
    dataStream >> consists;

    if (consists) {
        item->m_consists_of.resize(consists);
        union {
            quint64 ui64;
            Item::ConsistsOf co;
        } u;

        for (quint32 i = 0; i < consists; ++i) {
            dataStream >> u.ui64;
            item->m_consists_of[i] = u.co;
        }
    }
    else
        item->m_consists_of.clear();

    quint32 known_colors_count;
    dataStream >> known_colors_count;
    item->m_known_colors.resize(int(known_colors_count));

    for (int i = 0; i < int(known_colors_count); i++) {
        quint32 colid = 0;
        dataStream >> colid;
        item->m_known_colors[i] = colid;
    }

    return item.take();
}

void Core::writeItemToDatabase(const Item *item, QDataStream &dataStream, DatabaseVersion v)
{
    dataStream << item->m_id << item->m_name.toUtf8() << qint8(item->m_item_type->id());

    if (v <= DatabaseVersion::Version_2)
        dataStream << quint32(1); // this used to be a list of category ids
    dataStream << quint32(item->category()->id());

    quint32 colorid = item->m_color ? item->m_color->id() : Color::InvalidId;
    dataStream << colorid << qint64(item->m_last_inv_update) << item->m_weight
       << quint32(item->m_index) << quint32(item->m_year);

    if (item->m_appears_in && item->m_appears_in [0] && item->m_appears_in [1]) {
        quint32 *ptr = item->m_appears_in;

        for (quint32 i = 0; i < item->m_appears_in [1]; i++)
            dataStream << *ptr++;
    }
    else
        dataStream << quint32(0);

    if (!item->m_consists_of.empty()) {
        dataStream << quint32(item->m_consists_of.size());
        union {
            quint64 ui64;
            Item::ConsistsOf co;
        } u;

        for (quint32 i = 0; i < quint32(item->m_consists_of.size()); ++i) {
            u.co = item->m_consists_of.at(i);
            dataStream << u.ui64;
        }
    }
    else
        dataStream << quint32(0);

    if (v >= DatabaseVersion::Version_2) {
        dataStream << quint32(item->m_known_colors.size());
        for (auto cid : qAsConst(item->m_known_colors))
            dataStream << quint32(cid);
    }
}

PartColorCode *Core::readPCCFromDatabase(QDataStream &dataStream, Core::DatabaseVersion) const
{
    QScopedPointer<PartColorCode> pcc(new PartColorCode);

    qint8 itemTypeId;
    QString itemId;
    uint colorId;
    dataStream >> pcc->m_id >> itemTypeId >> itemId >> colorId;

    pcc->m_item = item(itemTypeId, itemId.toLatin1());
    pcc->m_color = color(colorId);
    return pcc.take();
}

void Core::writePCCToDatabase(const PartColorCode *pcc,
                              QDataStream &dataStream, Core::DatabaseVersion)
{
    dataStream << pcc->id() << qint8(pcc->item()->itemType()->id()) << pcc->item()->id()
               << pcc->color()->id();
}


bool Core::applyChangeLog(const Item *&item, const Color *&color, Incomplete *inc)
{
    if (!inc)
        return false;

    const Item *fixed_item = item;
    const Color *fixed_color = color;

    QByteArray itemtypeid { 1, inc->m_itemtype_id };
    QByteArray itemid { inc->m_item_id };
    uint colorid = inc->m_color_id;

    if (!itemtypeid.isEmpty() && !inc->m_itemtype_name.isEmpty())
        itemtypeid = inc->m_itemtype_name.toLatin1().toUpper().left(1);

    for (int i = int(m_changelog.size()) - 1; i >= 0 && !(fixed_color && fixed_item); --i) {
        const ChangeLogEntry &cl = ChangeLogEntry(m_changelog.at(size_t(i)));

        if (!fixed_item) {
            if ((cl.type() == ChangeLogEntry::ItemId) ||
                    (cl.type() == ChangeLogEntry::ItemMerge) ||
                    (cl.type() == ChangeLogEntry::ItemType)) {
                if ((itemtypeid == cl.from(0).left(1)) &&
                        (itemid == cl.from(1))) {
                    itemtypeid = cl.to(0).left(1);
                    itemid = cl.to(1);

                    if (!itemtypeid.isEmpty() && !itemid.isEmpty())
                        fixed_item = core()->item(itemtypeid.at(0), itemid);
                }
            }
        }
        if (!fixed_color) {
            if (cl.type() == ChangeLogEntry::ColorMerge) {
                if (colorid == cl.from(0).toUInt()) {
                    bool ok;
                    colorid = cl.to(0).toUInt(&ok);
                    if (ok)
                        fixed_color = core()->color(colorid);
                }
            }
        }
    }

    if (fixed_item && !item)
        item = fixed_item;
    if (fixed_color && !color)
        color = fixed_color;

    return (fixed_item && fixed_color);
}

qreal Core::itemImageScaleFactor() const
{
    return m_item_image_scale_factor;
}

void Core::setItemImageScaleFactor(qreal f)
{
    if (!qFuzzyCompare(f, m_item_image_scale_factor)) {
        m_item_image_scale_factor = f;

        m_noImageCache.clear();
        m_colorImageCache.clear();

        emit itemImageScaleFactorChanged(f);
    }
}

bool Core::isLDrawEnabled() const
{
    return !m_ldraw_datadir.isEmpty();
}

QString Core::ldrawDataPath() const
{
    return m_ldraw_datadir;
}

void Core::setLDrawDataPath(const QString &ldrawDataPath)
{
    m_ldraw_datadir = ldrawDataPath;
}

const QVector<const Color *> Item::knownColors() const
{
    QVector<const Color *> result;
    for (auto colId : m_known_colors) {
        auto color = core()->color(colId);
        if (color)
            result << color;
    }
    return result;
}

const Item *Item::ConsistsOf::item() const
{
    return BrickLink::core()->items().at(m_index);
}

const Color *Item::ConsistsOf::color() const
{
    return BrickLink::core()->color(m_color);
}

} // namespace BrickLink

#include "moc_bricklink.cpp"

