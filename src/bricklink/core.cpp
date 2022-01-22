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
#include <QJsonDocument>
#include <QJsonObject>
#include <QRunnable>
#include <QPixmapCache>
#include <QRegularExpression>


#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#  include <qhashfunctions.h>
#  define q5Hash qHash
#else
#  include "utility/q5hashfunctions.h"
#endif
#include "utility/utility.h"
#include "utility/systeminfo.h"
#include "utility/stopwatch.h"
#include "utility/chunkreader.h"
#include "utility/chunkwriter.h"
#include "utility/exception.h"
#include "utility/transfer.h"

#include "bricklink/category.h"
#include "bricklink/changelogentry.h"
#include "bricklink/color.h"
#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/itemtype.h"
#include "bricklink/lot.h"
#include "bricklink/partcolorcode.h"
#include "bricklink/picture.h"
#include "bricklink/priceguide.h"
#if !defined(BS_BACKEND)
#  include "bricklink/cart.h"
#  include "bricklink/order.h"
#  include "bricklink/store.h"
#endif


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
            query.addQueryItem(QString(QLatin1Char(item->itemTypeId())), QLatin1String(item->id()));
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
            query.addQueryItem(QString(QLatin1Char(item->itemTypeId())), Utility::urlQueryEscape(item->id()));
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
            query.addQueryItem("itemType"_l1, QString(QLatin1Char(item->itemTypeId())));

            // workaround for BL not accepting the -X suffix for sets, instructions and boxes
            QString id = QLatin1String(item->id());
            char itt = item->itemTypeId();

            if (itt == 'S' || itt == 'I' || itt == 'O') {
                int pos = id.lastIndexOf('-'_l1);
                if (pos >= 0)
                    id.truncate(pos);
            }
            query.addQueryItem("q"_l1, Utility::urlQueryEscape(id));

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
            query.addQueryItem(QString(QLatin1Char(item->itemTypeId())), Utility::urlQueryEscape(item->id()));
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
            query.addQueryItem("q"_l1, Utility::urlQueryEscape(static_cast<const char *>(opt)));
        url.setQuery(query);
        break;
    }
    case URL_StoreItemDetail: {
        auto lotId = static_cast<const unsigned int *>(opt);
        if (lotId && *lotId) {
            url = "https://www.bricklink.com/inventory_detail.asp"_l1;
            QUrlQuery query;
            query.addQueryItem("invID"_l1, Utility::urlQueryEscape(QString::number(*lotId)));
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
            query.addQueryItem("catType"_l1, QString(QLatin1Char(item->itemTypeId())));
            QString queryTerm = QLatin1String(item->id());
            if (queryTerm.contains('-'_l1)) {
                queryTerm = item->name();
                queryTerm.remove('('_l1);
                queryTerm.remove(')'_l1);
            }
            query.addQueryItem("q"_l1, Utility::urlQueryEscape(queryTerm));
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
            query.addQueryItem("ID"_l1, Utility::urlQueryEscape(orderId));
            url.setQuery(query);
        }
        break;
    }
    case URL_ShoppingCart: {
        auto shopId = static_cast<const int *>(opt);
        if (shopId && *shopId) {
            url = "https://www.bricklink.com/v2/globalcart.page"_l1;
            QUrlQuery query;
            query.addQueryItem("sid"_l1, Utility::urlQueryEscape(QString::number(*shopId)));
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


QString Core::dataPath() const
{
    return m_datadir;
}

QString Core::dataFileName(QStringView fileName, const Item *item, const Color *color) const
{
    // Avoid huge directories with 1000s of entries.
    // sse4.2 is only used if a seed value is supplied
    // please note: Qt6's qHash is incompatible
    uchar hash = q5Hash(QString::fromLatin1(item->id()), 42) & 0xff;

    QString p = m_datadir % QLatin1Char(item->itemTypeId()) % u'/' % (hash < 0x10 ? u"0" : u"")
            % QString::number(hash, 16) % u'/' % QLatin1String(item->id()) % u'/'
            % (color ? QString::number(color->id()) : QString()) % (color ? u"/" : u"")
            % fileName;

    return p;
}

QFile *Core::dataReadFile(QStringView fileName, const Item *item, const Color *color) const
{
    auto f = new QFile(dataFileName(fileName, item, color));
    f->open(QIODevice::ReadOnly);
    return f;
}

QSaveFile *Core::dataSaveFile(QStringView fileName, const Item *item, const Color *color) const
{
    auto p = dataFileName(fileName, item, color);

    if (!QDir(fileName.isEmpty() ? p : p.left(p.size() - int(fileName.size()))).mkpath("."_l1))
        return nullptr;

    auto f = new QSaveFile(p);
    if (!f->open(QIODevice::WriteOnly)) {
        qWarning() << "BrickLink::Core::dataSaveFile failed to open" << f->fileName()
                   << "for writing:" << f->errorString();
    }
    return f;
}

void Core::setCredentials(const QPair<QString, QString> &credentials)
{
    if (m_credentials != credentials) {
        bool wasAuthenticated = m_authenticated;
        m_authenticated = false;
        if (m_authenticatedTransfer)
            m_authenticatedTransfer->abortAllJobs();
        bool newUserId = (credentials.first != m_credentials.first);
        m_credentials = credentials;
        if (wasAuthenticated) {
            emit authenticationChanged(false);

            QUrl url("https://www.bricklink.com/ajax/renovate/loginandout.ajax"_l1);
            QUrlQuery q;
            q.addQueryItem("do_logout"_l1, "true"_l1);
            url.setQuery(q);

            auto logoutJob = TransferJob::get(url);
            m_authenticatedTransfer->retrieve(logoutJob, true);
        }
        if (newUserId)
            emit userIdChanged(m_credentials.first);
    }
}

QString Core::userId() const
{
    return m_credentials.first;
}

bool Core::isAuthenticated() const
{
    return m_authenticated;
}

void Core::retrieveAuthenticated(TransferJob *job)
{
    if (!m_authenticated) {
        if (!m_loginJob) {
            QUrl url("https://www.bricklink.com/ajax/renovate/loginandout.ajax"_l1);
            QUrlQuery q;
            q.addQueryItem("userid"_l1,          Utility::urlQueryEscape(m_credentials.first));
            q.addQueryItem("password"_l1,        Utility::urlQueryEscape(m_credentials.second));
            q.addQueryItem("keepme_loggedin"_l1, "1"_l1);
            url.setQuery(q);

            m_loginJob = TransferJob::post(url, nullptr, true /* no redirects */);
            m_authenticatedTransfer->retrieve(m_loginJob, true);
        }
        m_jobsWaitingForAuthentication << job;
    } else {
        m_authenticatedTransfer->retrieve(job);
    }
}



Core *Core::s_inst = nullptr;

Core *Core::create(const QString &datadir, QString *errstring)
{
    if (!s_inst) {
        s_inst = new Core(datadir);
#if !defined(BS_BACKEND)
        s_inst->m_store = new Store(s_inst);
        s_inst->m_orders = new Orders(s_inst);
        s_inst->m_carts = new Carts(s_inst);
#endif

        QString test = s_inst->dataPath();

        if (!test.isEmpty()) {
            QFileInfo fi(test);

            if (!fi.exists()) {
                QDir(test).mkpath("."_l1);
                fi.refresh();
            }
            if (!fi.exists() || !fi.isDir() || !fi.isReadable() || !fi.isWritable())
                test.clear();
        }

        if (test.isEmpty()) {
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
    , m_transfer(new Transfer(this))
    , m_authenticatedTransfer(new Transfer(this))
{
    connect(m_transfer, &Transfer::finished,
            this, [this](TransferJob *job) {
        if (job) {
            if (Picture *pic = job->userData("picture").value<Picture *>())
                pictureJobFinished(job, pic);
            else if (PriceGuide *pg = job->userData("priceGuide").value<PriceGuide *>())
                priceGuideJobFinished(job, pg);
        }
    });
    connect(m_transfer, &Transfer::overallProgress,
            this, &Core::transferProgress);

    connect(m_authenticatedTransfer, &Transfer::finished,
            this, [this](TransferJob *job) {
        if (!job) {
            return;
        } else if (job == m_loginJob) {
            m_loginJob = nullptr;

            QString error;

            if (job->responseCode() == 200 && job->data()) {
                auto json = QJsonDocument::fromJson(*job->data());
                if (!json.isNull()) {
                    bool wasAuthenticated = m_authenticated;
                    m_authenticated = (json.object().value("returnCode"_l1).toInt() == 0);
                    if (!m_authenticated)
                        error = json.object().value("returnMessage"_l1).toString();

                    if (wasAuthenticated != m_authenticated)
                        emit authenticationChanged(m_authenticated);
                }
            } else {
                error = job->errorString();
            }
            if (!error.isEmpty())
                emit authenticationFailed(m_credentials.first, error);

            for (TransferJob *authJob : qAsConst(m_jobsWaitingForAuthentication))
                m_authenticatedTransfer->retrieve(authJob);
            m_jobsWaitingForAuthentication.clear();

            if (!m_authenticated)
                m_authenticatedTransfer->abortAllJobs();
        } else {
            if (job->responseCode() == 302 &&
                    (job->redirectUrl().toString().contains("v2/login.page"_l1)
                     || job->redirectUrl().toString().contains("login.asp?"_l1))) {
                m_authenticated = false;
                emit authenticationChanged(m_authenticated);

                job->resetForReuse();
                QMetaObject::invokeMethod(this, [=, this]() {
                    retrieveAuthenticated(job);
                }, Qt::QueuedConnection);
            } else {
                emit authenticatedTransferFinished(job);
            }
        }
    });
    connect(m_authenticatedTransfer, &Transfer::overallProgress,
            this, &Core::authenticatedTransferOverallProgress);
    connect(m_authenticatedTransfer, &Transfer::progress,
            this, &Core::authenticatedTransferProgress);
    connect(m_authenticatedTransfer, &Transfer::started,
            this, &Core::authenticatedTransferStarted);

    m_diskloadPool.setMaxThreadCount(QThread::idealThreadCount() * 3);
    m_online = true;

    // the max. pic cache size is at least 1GB and at max half the physical memory on 64bit systems
    // the max. pg cache size is at least 5.000 and 10.000 if more than 3GB of RAM are available
    quint64 picCacheMem = 1'000'000'000ULL;
    int pgCacheEntries = 5'000;

#if Q_PROCESSOR_WORDSIZE >= 8
    picCacheMem = qMax(picCacheMem, SystemInfo::inst()->physicalMemory() / 2);
    if (SystemInfo::inst()->physicalMemory() >= 3'000'000'000ULL)
        pgCacheEntries *= 2;
#endif

    m_pic_cache.setMaxCost(int(picCacheMem / 1024)); // each pic has the cost of memory used in KB
    m_pg_cache.setMaxCost(pgCacheEntries); // each priceguide has a cost of 1
}

Core::~Core()
{
    clear();
    s_inst = nullptr;
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

void Core::setOnlineStatus(bool isOnline)
{
    m_online = isOnline;
    if (!isOnline)
        cancelTransfers();
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

bool Core::isDatabaseValid() const
{
    return !m_items.empty();
}

const Category *Core::category(uint id) const
{
    auto it = std::lower_bound(m_categories.cbegin(), m_categories.cend(), id, &Category::lessThan);
    if ((it != m_categories.cend()) && (it->id() == id))
        return &(*it);
    return nullptr;
}

const Color *Core::color(uint id) const
{
    auto it = std::lower_bound(m_colors.cbegin(), m_colors.cend(), id, &Color::lessThan);
    if ((it != m_colors.cend()) && (it->id() == id))
        return &(*it);
    return nullptr;
}

const Color *Core::colorFromName(const QString &name) const
{
    if (name.isEmpty())
        return nullptr;

    auto it = std::find_if(m_colors.cbegin(), m_colors.cend(), [name](const auto &color) {
        return !color.name().compare(name, Qt::CaseInsensitive);
    });
    if (it != m_colors.cend())
        return &(*it);
    return nullptr;
}


const Color *Core::colorFromLDrawId(int ldrawId) const
{
    auto it = std::find_if(m_colors.cbegin(), m_colors.cend(), [ldrawId](const auto &color) {
        return (color.ldrawId() == ldrawId);
    });
    if (it != m_colors.cend())
        return &(*it);
    return nullptr;
}


const ItemType *Core::itemType(char id) const
{
    auto it = std::lower_bound(m_item_types.cbegin(), m_item_types.cend(), id, &ItemType::lessThan);
    if ((it != m_item_types.cend()) && (it->id() == id))
        return &(*it);
    return nullptr;
}

const Item *Core::item(char tid, const QByteArray &id) const
{
    auto needle = std::make_pair(tid, id);
    auto it = std::lower_bound(m_items.cbegin(), m_items.cend(), needle, Item::lessThan);
    if ((it != m_items.cend()) && (it->m_itemTypeId == tid) && (it->m_id == id))
        return &(*it);
    return nullptr;
}

const Item *Core::item(const std::string &tids, const QByteArray &id) const
{
    for (const char &tid : tids) {
        auto needle = std::make_pair(tid, id);
        auto it = std::lower_bound(m_items.cbegin(), m_items.cend(), needle, Item::lessThan);
        if ((it != m_items.cend()) && (it->m_itemTypeId == tid) && (it->m_id == id))
            return &(*it);
    }
    return nullptr;
}

const PartColorCode *Core::partColorCode(uint id)
{
    auto it = std::lower_bound(m_pccs.cbegin(), m_pccs.cend(), id, &PartColorCode::lessThan);
    if ((it != m_pccs.cend()) && (it->id() == id))
        return &(*it);
    return nullptr;
}

void Core::cancelTransfers()
{
    if (m_transfer)
        m_transfer->abortAllJobs();
    if (m_authenticatedTransfer)
        m_authenticatedTransfer->abortAllJobs();
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

    m_colors.clear();
    m_item_types.clear();
    m_categories.clear();
    m_items.clear();
    m_pccs.clear();
    m_itemChangelog.clear();
    m_colorChangelog.clear();
}


//
// Database (de)serialization
//

bool Core::readDatabase(const QString &filename)
{
    try {
        clear();

        stopwatch *sw = nullptr; //new stopwatch("Core::readDatabase()");

        QDateTime generationDate;

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
        bool gotItemChangeLog = false, gotColorChangeLog = false, gotPccs = false;

        auto check = [&ds, &f]() {
            if (ds.status() != QDataStream::Ok)
                throw Exception("failed to read from database (%1) at position %2")
                    .arg(f.fileName()).arg(f.pos());
        };

        auto sizeCheck = [&f](int s, int max) {
            if (s > max)
                throw Exception("failed to read from database (%1) at position %2: size value %L3 is larger than expected maximum %L4")
                    .arg(f.fileName()).arg(f.pos()).arg(s).arg(max);
        };

        while (cr.startChunk()) {
            switch (cr.chunkId() | quint64(cr.chunkVersion()) << 32) {
            case ChunkId('D','A','T','E') | 1ULL << 32: {
                ds >> generationDate;
                break;
            }
            case ChunkId('C','O','L',' ') | 1ULL << 32: {
                quint32 colc = 0;
                ds >> colc;
                check();
                sizeCheck(colc, 1'000);

                m_colors.resize(colc);
                for (quint32 i = 0; i < colc; ++i) {
                    readColorFromDatabase(m_colors[i], ds, DatabaseVersion::Latest);
                    check();
                }
                gotColors = true;
                break;
            }
            case ChunkId('C','A','T',' ') | 1ULL << 32: {
                quint32 catc = 0;
                ds >> catc;
                check();
                sizeCheck(catc, 10'000);

                m_categories.resize(catc);
                for (quint32 i = 0; i < catc; ++i) {
                    readCategoryFromDatabase(m_categories[i], ds, DatabaseVersion::Latest);
                    check();
                }
                gotCategories = true;
                break;
            }
            case ChunkId('T','Y','P','E') | 1ULL << 32: {
                quint32 ittc = 0;
                ds >> ittc;
                check();
                sizeCheck(ittc, 20);

                m_item_types.resize(ittc);
                for (quint32 i = 0; i < ittc; ++i) {
                    readItemTypeFromDatabase(m_item_types[i], ds, DatabaseVersion::Latest);
                    check();
                }
                gotItemTypes = true;
                break;
            }
            case ChunkId('I','T','E','M') | 1ULL << 32: {
                quint32 itc = 0;
                ds >> itc;
                check();
                sizeCheck(itc, 1'000'000);

                m_items.resize(itc);
                for (quint32 i = 0; i < itc; ++i) {
                    readItemFromDatabase(m_items[i], ds, DatabaseVersion::Latest);
                    check();
                }
                m_items.shrink_to_fit();
                gotItems = true;
                break;
            }
            case ChunkId('I','C','H','G') | 1ULL << 32: {
                quint32 clc = 0;
                ds >> clc;
                check();
                sizeCheck(clc, 1'000'000);

                m_itemChangelog.resize(clc);
                for (quint32 i = 0; i < clc; ++i) {
                    readItemChangeLogFromDatabase(m_itemChangelog[i], ds, DatabaseVersion::Latest);
                    check();
                }
                gotItemChangeLog = true;
                break;
            }
            case ChunkId('C','C','H','G') | 1ULL << 32: {
                quint32 clc = 0;
                ds >> clc;
                check();
                sizeCheck(clc, 1'000);

                m_colorChangelog.resize(clc);
                for (quint32 i = 0; i < clc; ++i) {
                    readColorChangeLogFromDatabase(m_colorChangelog[i], ds, DatabaseVersion::Latest);
                    check();
                }
                gotColorChangeLog = true;
                break;
            }
            case ChunkId('P','C','C',' ') | 1ULL << 32: {
                if (!gotItems || !gotColors)
                    throw Exception("found a 'PCC ' chunk before the 'ITEM' and 'COL ' chunks");

                quint32 pccc = 0;
                ds >> pccc;
                check();
                sizeCheck(pccc, 1'000'000);

                m_pccs.resize(pccc);
                for (quint32 i = 0; i < pccc; ++i) {
                    readPCCFromDatabase(m_pccs[i], ds, DatabaseVersion::Latest);
                    check();
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

        if (!gotColors || !gotCategories || !gotItemTypes || !gotItems || !gotItemChangeLog
                || !gotColorChangeLog || !gotPccs) {
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
                 << "\n  ChangeLog I :" << m_itemChangelog.size()
                 << "\n  ChangeLog C :" << m_colorChangelog.size();

        m_databaseDate = generationDate;
        emit databaseDateChanged(generationDate);

        return true;

    } catch (const Exception &e) {
        qWarning() << "Error reading database:" << e.what();
        clear();
        return false;
    }
}


bool Core::writeDatabase(const QString &filename, DatabaseVersion version) const
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

        check(cw.startChunk(ChunkId('D','A','T','E'), 1));
        ds << QDateTime::currentDateTimeUtc();
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','O','L',' '), 1));
        ds << quint32(m_colors.size());
        for (const Color &col : m_colors)
            writeColorToDatabase(col, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('C','A','T',' '), 1));
        ds << quint32(m_categories.size());
        for (const Category &cat : m_categories)
            writeCategoryToDatabase(cat, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('T','Y','P','E'), 1));
        ds << quint32(m_item_types.size());
        for (const ItemType &itt : m_item_types)
            writeItemTypeToDatabase(itt, ds, version);
        check(cw.endChunk());

        check(cw.startChunk(ChunkId('I','T','E','M'), 1));
        ds << quint32(m_items.size());
        for (const Item &item : m_items)
            writeItemToDatabase(item, ds, version);
        check(cw.endChunk());

        if (version >= DatabaseVersion::Version_5) {
            check(cw.startChunk(ChunkId('I','C','H','G'), 1));
            ds << quint32(m_itemChangelog.size());
            for (const ItemChangeLogEntry &e : m_itemChangelog)
                writeItemChangeLogToDatabase(e, ds, version);
            check(cw.endChunk());

            check(cw.startChunk(ChunkId('C','C','H','G'), 1));
            ds << quint32(m_colorChangelog.size());
            for (const ColorChangeLogEntry &e : m_colorChangelog)
                writeColorChangeLogToDatabase(e, ds, version);
            check(cw.endChunk());
        } else {
            check(cw.startChunk(ChunkId('C','H','G','L'), 1));
            ds << quint32(m_itemChangelog.size() + m_colorChangelog.size());
            for (const ItemChangeLogEntry &e : m_itemChangelog) {
                ds << static_cast<QByteArray>("\x03\t" % QByteArray(1, e.fromItemTypeId()) % '\t' % e.fromItemId()
                                              % '\t' % e.toItemTypeId() % '\t' % e.toItemId());
            }
            for (const ColorChangeLogEntry &e : m_colorChangelog) {
                ds << static_cast<QByteArray>("\x07\t" % QByteArray::number(e.fromColorId()) % "\tx\t"
                                              % QByteArray::number(e.toColorId()) % "\tx");
            }
            check(cw.endChunk());
        }

        if (version >= DatabaseVersion::Version_3) {
            check(cw.startChunk(ChunkId('P','C','C',' '), 1));
            ds << quint32(m_pccs.size());
            for (const PartColorCode &pcc : m_pccs)
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


void Core::readColorFromDatabase(Color &col, QDataStream &dataStream, DatabaseVersion)
{
    quint32 flags;

    dataStream >> col.m_id >> col.m_name >> col.m_ldraw_id >> col.m_color >> flags
            >> col.m_popularity >> col.m_year_from >> col.m_year_to;
    col.m_type = static_cast<Color::Type>(flags);
}

void Core::writeColorToDatabase(const Color &col, QDataStream &dataStream, DatabaseVersion v)
{
    dataStream << col.m_id;
    if (v <= DatabaseVersion::Version_3)
        dataStream << col.m_name.toUtf8();
    else
        dataStream << col.m_name;
    dataStream << col.m_ldraw_id << col.m_color << quint32(col.m_type) << col.m_popularity
               << col.m_year_from << col.m_year_to;
}


void Core::readCategoryFromDatabase(Category &cat, QDataStream &dataStream, DatabaseVersion)
{
    dataStream >> cat.m_id >> cat.m_name;
}

void Core::writeCategoryToDatabase(const Category &cat, QDataStream &dataStream, DatabaseVersion v)
{
    dataStream << cat.m_id;
    if (v <= DatabaseVersion::Version_3)
        dataStream << cat.m_name.toUtf8();
    else
        dataStream << cat.m_name;
}


void Core::readItemTypeFromDatabase(ItemType &itt, QDataStream &dataStream, DatabaseVersion)
{
    quint8 flags = 0;
    quint32 catSize = 0;
    dataStream >> (qint8 &) itt.m_id >> (qint8 &) itt.m_picture_id >> itt.m_name >> flags >> catSize;

    itt.m_categoryIndexes.reserve(catSize);
    while (catSize-- > 0) {
        quint16 catIdx = 0;
        dataStream >> catIdx;
        itt.m_categoryIndexes.emplace_back(catIdx);
    }

    itt.m_has_inventories   = flags & 0x01;
    itt.m_has_colors        = flags & 0x02;
    itt.m_has_weight        = flags & 0x04;
    itt.m_has_year          = flags & 0x08;
    itt.m_has_subconditions = flags & 0x10;
}

void Core::writeItemTypeToDatabase(const ItemType &itt, QDataStream &dataStream, DatabaseVersion v)
{
    quint8 flags = 0;
    flags |= (itt.m_has_inventories   ? 0x01 : 0);
    flags |= (itt.m_has_colors        ? 0x02 : 0);
    flags |= (itt.m_has_weight        ? 0x04 : 0);
    flags |= (itt.m_has_year          ? 0x08 : 0);
    flags |= (itt.m_has_subconditions ? 0x10 : 0);

    dataStream << qint8(itt.m_id) << qint8(itt.m_picture_id);
    if (v <= DatabaseVersion::Version_3)
        dataStream << itt.m_name.toUtf8();
    else
        dataStream << itt.m_name;
    dataStream << flags << quint32(itt.m_categoryIndexes.size());

    for (const quint16 catIdx : itt.m_categoryIndexes) {
        if (v <= DatabaseVersion::Version_3)
            dataStream << quint32(core()->categories()[catIdx].id());
        else
            dataStream << catIdx;
    }
}


void Core::readItemFromDatabase(Item &item, QDataStream &dataStream, DatabaseVersion)
{
    dataStream >> item.m_id >> item.m_name >> item.m_itemTypeIndex >> item.m_categoryIndex
            >> item.m_defaultColorIndex >> (qint8 &) item.m_itemTypeId >> item.m_year
            >> item.m_lastInventoryUpdate >> item.m_weight;

    quint32 appearsInSize = 0;
    dataStream >> appearsInSize;
    item.m_appears_in.clear();
    item.m_appears_in.reserve(appearsInSize);

    union {
        quint32 ui32;
        Item::AppearsInRecord ai;
    } appearsInUnion;

    while (appearsInSize-- > 0) {
        dataStream >> appearsInUnion.ui32;
        item.m_appears_in.push_back(appearsInUnion.ai);
    }
    item.m_appears_in.shrink_to_fit();

    quint32 consistsOfSize = 0;
    dataStream >> consistsOfSize;
    item.m_consists_of.clear();
    item.m_consists_of.reserve(consistsOfSize);

    union {
        quint64 ui64;
        Item::ConsistsOf co;
    } consistsOfUnion;

    while (consistsOfSize-- > 0) {
        dataStream >> consistsOfUnion.ui64;
        item.m_consists_of.push_back(consistsOfUnion.co);
    }
    item.m_consists_of.shrink_to_fit();

    quint32 knownColorsSize;
    dataStream >> knownColorsSize;
    item.m_knownColorIndexes.clear();
    item.m_knownColorIndexes.reserve(knownColorsSize);

    quint16 colorIndex;

    while (knownColorsSize-- > 0) {
        dataStream >> colorIndex;
        item.m_knownColorIndexes.push_back(colorIndex);
    }
    item.m_knownColorIndexes.shrink_to_fit();
}

void Core::writeItemToDatabase(const Item &item, QDataStream &dataStream, DatabaseVersion v)
{
    dataStream << item.m_id;
    if (v <= DatabaseVersion::Version_3) {
        dataStream << item.m_name.toUtf8()
                   << qint8(item.m_itemTypeId)
                   << quint32(item.category()->id())
                   << quint32((item.m_defaultColorIndex != -1) ? item.defaultColor()->id()
                                                               : Color::InvalidId)
                   << item.m_lastInventoryUpdate
                   << item.m_weight
                   << quint32(&item - core()->items().data()) // the index
                   << quint32(item.m_year);
    } else {
        dataStream << item.m_name
                   << item.m_itemTypeIndex
                   << item.m_categoryIndex
                   << item.m_defaultColorIndex
                   << qint8(item.m_itemTypeId)
                   << item.m_year
                   << item.m_lastInventoryUpdate
                   << item.m_weight;
    }

    union {
        quint32 ui32;
        Item::AppearsInRecord ai;
    } appearsInUnion;
    if (v <= DatabaseVersion::Version_3) {
        quint32 colorCount = 0;
        for (auto it = item.m_appears_in.cbegin(); it != item.m_appears_in.cend(); ) {
            ++colorCount;
            it += (1 + it->m20);
        }
        dataStream << colorCount; // color count
        if (colorCount)
            dataStream << quint32(2 + item.m_appears_in.size()); // dword count
        for (auto it = item.m_appears_in.cbegin(); it != item.m_appears_in.cend(); ) {
            // fix color header (index -> id)
            uint colorCount = it->m20;
            uint colorId = quint32(core()->colors()[it->m12].id());
            appearsInUnion.ai.m12 = colorId;
            appearsInUnion.ai.m20 = colorCount;
            dataStream << appearsInUnion.ui32;
            ++it;

            for (uint i = 0; i < colorCount; ++i, ++it) {
                appearsInUnion.ai = *it;
                dataStream << appearsInUnion.ui32;
            }
        }
    } else {
        dataStream << quint32(item.m_appears_in.size());
        for (const auto &ai : item.m_appears_in) {
            appearsInUnion.ai = ai;
            dataStream << appearsInUnion.ui32;
        }
    }

    dataStream << quint32(item.m_consists_of.size());
    union {
        quint64 ui64;
        Item::ConsistsOf co;
    } consistsOfUnion;
    for (const auto &co : item.m_consists_of) {
        consistsOfUnion.co = co;
        if (v <= DatabaseVersion::Version_3) // fix color index -> id
            consistsOfUnion.co.m_colorIndex = core()->colors()[consistsOfUnion.co.m_colorIndex].id();
        dataStream << consistsOfUnion.ui64;
    }

    dataStream << quint32(item.m_knownColorIndexes.size());
    for (const quint16 colorIndex : item.m_knownColorIndexes) {
        if (v <= DatabaseVersion::Version_3)
            dataStream << quint32(core()->colors()[colorIndex].id());
        else
            dataStream << colorIndex;
    }
}

void Core::readPCCFromDatabase(PartColorCode &pcc, QDataStream &dataStream, Core::DatabaseVersion) const
{
    qint32 itemIndex, colorIndex;
    dataStream >> pcc.m_id >> itemIndex >> colorIndex;
    pcc.m_itemIndex = itemIndex;
    pcc.m_colorIndex = colorIndex;
}

void Core::writePCCToDatabase(const PartColorCode &pcc,
                              QDataStream &dataStream, Core::DatabaseVersion v)
{
    if (v <= DatabaseVersion::Version_3) {
        dataStream << pcc.id() << qint8(pcc.item()->itemTypeId())
                   << QString::fromLatin1(pcc.item()->id()) << pcc.color()->id();
    } else {
        dataStream << pcc.m_id << qint32(pcc.m_itemIndex) << qint32(pcc.m_colorIndex);
    }
}

void Core::readItemChangeLogFromDatabase(ItemChangeLogEntry &e, QDataStream &dataStream, DatabaseVersion) const
{
    dataStream >> e.m_fromTypeAndId >> e.m_toTypeAndId;
}

void Core::writeItemChangeLogToDatabase(const ItemChangeLogEntry &e, QDataStream &dataStream, DatabaseVersion)
{
    dataStream << e.m_fromTypeAndId << e.m_toTypeAndId;
}

void Core::readColorChangeLogFromDatabase(ColorChangeLogEntry &e, QDataStream &dataStream, DatabaseVersion) const
{
    dataStream >> e.m_fromColorId >> e.m_toColorId;
}

void Core::writeColorChangeLogToDatabase(const ColorChangeLogEntry &e, QDataStream &dataStream, DatabaseVersion)
{
    dataStream << e.m_fromColorId << e.m_toColorId;
}


bool Core::applyChangeLog(const Item *&item, const Color *&color, Incomplete *inc)
{
    if (!inc)
        return false;

    if (!item) {
        QByteArray itemTypeAndId = inc->m_itemtype_id % inc->m_item_id;
        if (!inc->m_itemtype_name.isEmpty())
            itemTypeAndId[0] = inc->m_itemtype_name.at(0).toUpper().toLatin1();

        auto it = std::lower_bound(m_itemChangelog.cbegin(), m_itemChangelog.cend(), itemTypeAndId);
        if ((it != m_itemChangelog.cend()) && (*it == itemTypeAndId))
            item = core()->item(it->toItemTypeId(), it->toItemId());
    }
    if (!color) {
        uint colorId = inc->m_color_id;
        auto it = std::lower_bound(m_colorChangelog.cbegin(), m_colorChangelog.cend(), colorId);
        if ((it != m_colorChangelog.cend()) && (*it == colorId))
            color = core()->color(it->toColorId());
    }

    return (item && color);
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

Core::ResolveResult Core::resolveIncomplete(Lot *lot)
{
    if (!lot->isIncomplete())
        return ResolveResult::Direct;

    BrickLink::Incomplete ic = *lot->isIncomplete();

    if ((ic.m_itemtype_id != BrickLink::ItemType::InvalidId) && !ic.m_item_id.isEmpty())
        lot->setItem(BrickLink::core()->item(ic.m_itemtype_id, ic.m_item_id));

    if (lot->item()) {
        if (auto *lic = lot->isIncomplete()) {
            lic->m_item_id.clear();
            lic->m_item_name.clear();
            lic->m_itemtype_id = BrickLink::ItemType::InvalidId;
            lic->m_itemtype_name.clear();
            lic->m_category_id = BrickLink::Category::InvalidId;
            lic->m_category_name.clear();
        }
    }

    if (ic.m_color_id != BrickLink::Color::InvalidId)
        lot->setColor(BrickLink::core()->color(ic.m_color_id));

    if (lot->color()) {
        if (auto *lic = lot->isIncomplete()) {
            lic->m_color_id = BrickLink::Color::InvalidId;
            lic->m_color_name.clear();
        }
    }

    if (lot->item() && lot->color()) {
        lot->setIncomplete(nullptr);
        return ResolveResult::Direct;

    } else {
        auto item = lot->item();
        auto color = lot->color();

        qWarning().noquote() << "Unknown item/color id:"
                             << (ic.m_itemtype_id ? QByteArray(1, ic.m_itemtype_id) : QByteArray("-"))
                             << ic.m_item_id << "@" << ic.m_color_id;

        bool ok = applyChangeLog(item, color, lot->isIncomplete());

        if (ok) {
            qWarning().noquote() << " > resolved via CL to:" << QByteArray(1, item->itemTypeId())
                                 << item->id() << "@" << color->id();
        }

        lot->setItem(item);
        lot->setColor(color);

        Q_ASSERT(ok == !lot->isIncomplete());
        return ok ? ResolveResult::ChangeLog : ResolveResult::Fail;
    }
}





} // namespace BrickLink














namespace BrickLink {

class PriceGuideLoaderJob : public QRunnable
{
public:
    explicit PriceGuideLoaderJob(PriceGuide *pg)
        : QRunnable()
        , m_pg(pg)
    {
        pg->m_update_status = UpdateStatus::Loading;
    }

    void run() override;

    PriceGuide *priceGuide() const
    {
        return m_pg;
    }

private:
    Q_DISABLE_COPY(PriceGuideLoaderJob)

    PriceGuide *m_pg;
};

void PriceGuideLoaderJob::run()
{
    if (m_pg) {
        QDateTime fetched;
        PriceGuide::Data data;
        bool valid = m_pg->loadFromDisk(fetched, data);
        auto pg = m_pg;

        QMetaObject::invokeMethod(core(), [=]() {
            pg->m_valid = valid;
            pg->m_update_status = UpdateStatus::Ok;
            if (valid) {
                pg->m_fetched = fetched;
                pg->m_data = data;
            }
            core()->priceGuideLoaded(pg);
        }, Qt::QueuedConnection);
    }
}

}

BrickLink::PriceGuide *BrickLink::Core::priceGuide(const Item *item,
                                                   const Color *color, bool highPriority)
{
    if (!item || !color)
        return nullptr;

    quint64 key = quint64(color->id()) << 32 | quint64(item->itemTypeId()) << 24 | quint64(item->index());
    PriceGuide *pg = m_pg_cache [key];

    bool needToLoad = false;

    if (!pg) {
        pg = new PriceGuide(item, color);
        if (!m_pg_cache.insert(key, pg)) {
            qWarning("Can not add priceguide to cache (cache max/cur: %d/%d, cost: %d)",
                     int(m_pg_cache.maxCost()), int(m_pg_cache.totalCost()), 1);
            return nullptr;
        }
        needToLoad = true;
    }

    if (highPriority) {
        if (!pg->isValid()) {
            pg->m_valid = pg->loadFromDisk(pg->m_fetched, pg->m_data);
            pg->m_update_status = UpdateStatus::Ok;
        }

        if (updateNeeded(pg->isValid(), pg->lastUpdated(), m_pg_update_iv))
            updatePriceGuide(pg, highPriority);
    }
    else if (needToLoad) {
        pg->addRef();
        m_diskloadPool.start(new PriceGuideLoaderJob(pg));
    }

    return pg;
}


void BrickLink::Core::priceGuideLoaded(BrickLink::PriceGuide *pg)
{
    if (pg) {
         if (pg->m_updateAfterLoad
                 || updateNeeded(pg->isValid(), pg->lastUpdated(), m_pg_update_iv))  {
             pg->m_updateAfterLoad = false;
             updatePriceGuide(pg, false);
         }
         emit priceGuideUpdated(pg);
         pg->release();
     }
}

void BrickLink::Core::updatePriceGuide(BrickLink::PriceGuide *pg, bool highPriority)
{
    if (!pg || (pg->m_update_status == UpdateStatus::Updating))
        return;

    if (!m_online || !m_transfer) {
        pg->m_update_status = UpdateStatus::UpdateFailed;
        emit priceGuideUpdated(pg);
        return;
    }

    if (pg->m_update_status == UpdateStatus::Loading) {
        pg->m_updateAfterLoad = true;
        return;
    }

    pg->m_update_status = UpdateStatus::Updating;
    pg->addRef();

    QUrlQuery query;
    QUrl url;

    if (pg->m_scrapedHtml) {
        url = QUrl("https://www.bricklink.com/priceGuideSummary.asp"_l1);
        query.addQueryItem("a"_l1,       QString(QLatin1Char(pg->item()->itemTypeId())));
        query.addQueryItem("vcID"_l1,    "1"_l1); // USD
        query.addQueryItem("vatInc"_l1,  "Y"_l1);
        query.addQueryItem("viewExclude"_l1, "Y"_l1);
        query.addQueryItem("ajView"_l1,  "Y"_l1); // only the AJAX snippet
        query.addQueryItem("colorID"_l1, QString::number(pg->color()->id()));
        query.addQueryItem("itemID"_l1,  Utility::urlQueryEscape(pg->item()->id()));
        query.addQueryItem("uncache"_l1, QString::number(QDateTime::currentMSecsSinceEpoch()));
    } else {
        //?{item type}={item no}&colorID={color ID}&cCode={currency code}&cExc={Y to exclude incomplete sets}
        url = QUrl("https://www.bricklink.com/BTpriceSummary.asp"_l1);

        query.addQueryItem(QString(QLatin1Char(pg->item()->itemTypeId())).toUpper(),
                           QLatin1String(pg->item()->id()));
        query.addQueryItem("colorID"_l1,  QString::number(pg->color()->id()));
        query.addQueryItem("cCode"_l1,    "USD"_l1);
        query.addQueryItem("cExc"_l1,     "Y"_l1); //  Y == exclude incomplete sets
    }
    url.setQuery(query);

    //qDebug ( "PG request started for %s", (const char *) url );
    pg->m_transferJob = TransferJob::get(url, nullptr, 2);
    pg->m_transferJob->setUserData("priceGuide", QVariant::fromValue(pg));

    m_transfer->retrieve(pg->m_transferJob, highPriority);
}

void BrickLink::Core::cancelPriceGuideUpdate(BrickLink::PriceGuide *pg)
{
    if (pg->m_transferJob)
        m_transfer->abortJob(pg->m_transferJob);
}


void BrickLink::Core::priceGuideJobFinished(TransferJob *j, PriceGuide *pg)
{
    pg->m_transferJob = nullptr;
    pg->m_update_status = UpdateStatus::UpdateFailed;

    if (j->isCompleted()) {
        if (pg->m_scrapedHtml)
            pg->m_valid = pg->parseHtml(*j->data(), pg->m_data);
        else
            pg->m_valid = pg->parse(*j->data(), pg->m_data);

        if (pg->m_valid) {
            pg->m_fetched = QDateTime::currentDateTime();
            pg->saveToDisk(pg->m_fetched, pg->m_data);
            pg->m_update_status = UpdateStatus::Ok;
        }
    } else if (!j->isAborted()) {
        qWarning() << "PriceGuide download failed:" << j->errorString() << "(" << j->responseCode() << ")";
    }

    emit priceGuideUpdated(pg);
    pg->release();
}




namespace BrickLink {

class PictureLoaderJob : public QRunnable
{
public:
    explicit PictureLoaderJob(Picture *pic)
        : QRunnable()
        , m_pic(pic)
    {
        pic->m_update_status = UpdateStatus::Loading;
    }

    void run() override;

    Picture *picture() const
    {
        return m_pic;
    }

private:
    Q_DISABLE_COPY(PictureLoaderJob)

    Picture *m_pic;
};

void PictureLoaderJob::run()
{
    if (m_pic) {
        QDateTime fetched;
        QImage image;
        bool valid = m_pic->loadFromDisk(fetched, image);
        auto pic = m_pic;
        QMetaObject::invokeMethod(core(), [=]() {
            pic->m_valid = valid;
            pic->m_update_status = UpdateStatus::Ok;
            if (valid) {
                pic->m_fetched = fetched;
                pic->m_image = image;
            }
            core()->pictureLoaded(pic);
        }, Qt::QueuedConnection);
    }
}

}

QSize BrickLink::Core::standardPictureSize() const
{
    QSize s(80, 60);
    qreal f = core()->itemImageScaleFactor();
    if (!qFuzzyCompare(f, 1))
        s *= f;
    return s;
}

BrickLink::Picture *BrickLink::Core::picture(const Item *item, const Color *color, bool highPriority)
{
    if (!item)
        return nullptr;

    quint64 key = Picture::key(item, color);
    Picture *pic = m_pic_cache[key];

    bool needToLoad = false;

    if (!pic) {
        pic = new Picture(item, color);
        if (!m_pic_cache.insert(key, pic, pic->cost())) {
            qWarning("Can not add picture to cache (cache max/cur: %d/%d, item: %s)",
                     int(m_pic_cache.maxCost()), int(m_pic_cache.totalCost()), item->id().constData());
            return nullptr;
        }
        needToLoad = true;
    }

    if (highPriority) {
        if (!pic->isValid()) {
            pic->m_valid = pic->loadFromDisk(pic->m_fetched, pic->m_image);
            pic->m_update_status = UpdateStatus::Ok;

            m_pic_cache.setObjectCost(key, pic->cost());
        }

        if (updateNeeded(pic->isValid(), pic->lastUpdated(), m_pic_update_iv))
            updatePicture(pic, highPriority);

    } else if (needToLoad) {
        pic->addRef();
        m_diskloadPool.start(new PictureLoaderJob(pic));
    }

    return pic;
}

BrickLink::Picture *BrickLink::Core::largePicture(const Item *item, bool high_priority)
{
    if (!item)
        return nullptr;
    return picture(item, nullptr, high_priority);
}


void BrickLink::Core::pictureLoaded(Picture *pic)
{
    if (pic) {
        if (pic->m_updateAfterLoad
                || updateNeeded(pic->isValid(), pic->lastUpdated(), m_pic_update_iv)) {
            pic->m_updateAfterLoad = false;
            updatePicture(pic, false);
        }
        emit pictureUpdated(pic);
        pic->release();

        m_pic_cache.setObjectCost(Picture::key(pic->item(), pic->color()), pic->cost());
    }
}

QPair<int, int> BrickLink::Core::pictureCacheStats() const
{
    return qMakePair(m_pic_cache.totalCost(), m_pic_cache.maxCost());
}

QPair<int, int> BrickLink::Core::priceGuideCacheStats() const
{
    return qMakePair(m_pg_cache.totalCost(), m_pg_cache.maxCost());
}

void BrickLink::Core::updatePicture(Picture *pic, bool highPriority)
{
    if (!pic || (pic->m_update_status == UpdateStatus::Updating))
        return;

    if (!m_online || !m_transfer) {
        pic->m_update_status = UpdateStatus::UpdateFailed;
        emit pictureUpdated(pic);
        return;
    }

    if (pic->m_update_status == UpdateStatus::Loading) {
        pic->m_updateAfterLoad = true;
        return;
    }

    pic->m_update_status = UpdateStatus::Updating;
    pic->addRef();

    bool large = (!pic->color());

    QString url;

    if (large) {
        url = u"https://img.bricklink.com/" % QLatin1Char(pic->item()->itemType()->pictureId())
                % u"L/" % QLatin1String(pic->item()->id()) % u".jpg";
    }
    else {
        url = u"https://img.bricklink.com/ItemImage/" % QLatin1Char(pic->item()->itemType()->pictureId())
                % u"N/" % QString::number(pic->color()->id()) % u'/'
                % QLatin1String(pic->item()->id()) % u".png";
    }

    //qDebug() << "PIC request started for" << url;
    QSaveFile *f = pic->saveFile();
    pic->m_transferJob = TransferJob::get(url, f);
    pic->m_transferJob->setUserData("picture", QVariant::fromValue(pic));
    m_transfer->retrieve(pic->m_transferJob, highPriority);
}

void BrickLink::Core::cancelPictureUpdate(Picture *pic)
{
    if (pic->m_transferJob)
        m_transfer->abortJob(pic->m_transferJob);
}


void BrickLink::Core::pictureJobFinished(TransferJob *j, Picture *pic)
{
    pic->m_transferJob = nullptr;
    bool large = (!pic->color());

    if (j->isCompleted() && j->file()) {
        static_cast<QSaveFile *>(j->file())->commit();

        // the pic is still ref'ed, so we just forward it to the loader
        pic->m_update_status = UpdateStatus::Loading;
        m_diskloadPool.start(new PictureLoaderJob(pic));
        return;

    } else if (large && (j->responseCode() == 404) && (j->url().path().endsWith(".jpg"_l1))) {
        // There's no large JPG image, so try a GIF image instead (mostly very old sets)
        // We save the GIF with an JPG extension if we succeed, but Qt uses the file header on
        // loading to do the right thing.

        if (!m_transfer) {
            pic->m_update_status = UpdateStatus::UpdateFailed;
        } else {
            pic->m_update_status = UpdateStatus::Updating;

            QUrl url = j->url();
            url.setPath(url.path().replace(".jpg"_l1, ".gif"_l1));

            QSaveFile *f = pic->saveFile();
            TransferJob *job = TransferJob::get(url, f);
            job->setUserData("picture", QVariant::fromValue<Picture *>(pic));
            m_transfer->retrieve(job);
            pic->m_transferJob = job;

            // the pic is still ref'ed: leave it that way for one more loop
            return;
        }
    } else {
        pic->m_update_status = UpdateStatus::UpdateFailed;

        qWarning() << "Image download failed:" << j->errorString() << "(" << j->responseCode() << ")";
    }

    emit pictureUpdated(pic);
    pic->release();
}









#include "moc_core.cpp"

