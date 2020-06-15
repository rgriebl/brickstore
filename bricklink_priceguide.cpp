/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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
#include <string.h>

#include <qfile.h>
#include <qfileinfo.h>
#include <q3textstream.h>
#include <qtextstream.h>
#include <qlocale.h>
#include <qjson_p.h>

#include "bricklink.h"

#define QUOTE(string)  _QUOTE(string)
#define _QUOTE(string) #string

BrickLink::PriceGuide *BrickLink::priceGuide(const Item *item, const BrickLink::Color *color, bool high_priority)
{
    if (!item || !color)
    {
        return 0;
    }

    QString key;
    key.sprintf("%c@%d@%d", item->itemType()->id(), item->index(), color->id());

    PriceGuide *pg = m_price_guides.cache[key];

    if (!pg)
    {
        pg = new PriceGuide(item, color);
        m_price_guides.cache.insert(key, pg);
    }

    if (pg && updateNeeded(pg->valid(), pg->lastUpdate(), m_price_guides.update_iv))
    {
        addPriceGuideToUpdate(pg, high_priority);
    }

    return pg;
}

void BrickLink::flushPriceGuidesToUpdate()
{
    if (m_price_guides_to_update->size() > 0)
    {
        updatePriceGuides(m_price_guides_to_update);
        m_price_guides_to_update = new QVector<BrickLink::PriceGuide *>();
    }
}

void BrickLink::addPriceGuideToUpdate(BrickLink::PriceGuide *pg, bool flush)
{
    if (!pg || pg->m_update_status == Updating || m_price_guides_to_update->contains(pg))
    {
        return;
    }

    if (!m_online)
    {
        pg->m_update_status = UpdateFailed;
        emit priceGuideUpdated(pg);
        return;
    }

    pg->m_update_status = Updating;
    pg->addRef();
    m_price_guides_to_update->append(pg);

    if (flush || m_price_guides_to_update->size() == 500)
    {
        updatePriceGuides(m_price_guides_to_update);
        m_price_guides_to_update = new QVector<BrickLink::PriceGuide *>();
    }
}

BrickLink::PriceGuide::PriceGuide(const BrickLink::Item *item, const BrickLink::Color *color)
{
    m_item = item;
    m_color = color;

    m_valid = false;
    m_update_status = Ok;

    memset(m_quantities, 0, sizeof(m_quantities));
    memset(m_lots, 0, sizeof(m_lots));
    memset(m_prices, 0, sizeof(m_prices));

    load_from_disk();
}

BrickLink::PriceGuide::~PriceGuide()
{
}

void BrickLink::PriceGuide::save_to_disk()
{
    QString path = BrickLink::inst()->dataPath(m_item, m_color);

    if (path.isEmpty())
    {
        return;
    }

    path += "priceguide.txt";

    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
    {
        Q3TextStream ts(&f);

        ts << "# Price Guide for part #" << m_item->id() << " (" << m_item->name() << "), color #" << m_color->id() << " (" << m_color->name() << ")\n";
        ts << "# last update: " << m_fetched.toString() << "\n#\n";

        for (int t = 0; t < TimeCount; t++)
        {
            for (int c = 0; c < ConditionCount; c++)
            {
                ts << (t == AllTime ? 'A' : (t == PastSix ? 'P' : 'C')) << '\t' << (c == New ? 'N' : 'U') << '\t';
                ts << m_quantities[t][c] << '\t'
                   << m_lots[t][c] << '\t'
                   << m_prices[t][c][Lowest].toCString() << '\t'
                   << m_prices[t][c][Average].toCString() << '\t'
                   << m_prices[t][c][WAverage].toCString() << '\t'
                   << m_prices[t][c][Highest].toCString() << '\n';
            }
        }
    }
}

void BrickLink::PriceGuide::load_from_disk()
{
    QString path = BrickLink::inst()->dataPath(m_item, m_color);

    if (path.isEmpty())
    {
        return;
    }

    path += "priceguide.txt";

    bool loaded[TimeCount][ConditionCount];
    ::memset(loaded, 0, sizeof(loaded));

    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
    {
        Q3TextStream ts(&f);
        QString line;

        while (!(line = ts.readLine()).isNull())
        {
            if (!line.length() || line[0] == '#' || line[0] == '\r')
            {
                continue;
            }

            QStringList sl = QStringList::split('\t', line, true);

            if (sl.count() != 8 || sl[0].length() != 1 || sl[1].length() != 1)
            {
                continue;
            }

            int t = -1;
            int c = -1;

            switch (sl[0][0].latin1())
            {
                case 'A': t = AllTime; break;
                case 'P': t = PastSix; break;
                case 'C': t = Current; break;
            }

            switch (sl[1][0].latin1())
            {
                case 'N': c = New;  break;
                case 'U': c = Used; break;
            }

            if (t != -1 && c != -1)
            {
                m_quantities[t][c]       = sl[2].toInt();
                m_lots[t][c]             = sl[3].toInt();
                m_prices[t][c][Lowest]   = money_t::fromCString(sl[4]);
                m_prices[t][c][Average]  = money_t::fromCString(sl[5]);
                m_prices[t][c][WAverage] = money_t::fromCString(sl[6]);
                m_prices[t][c][Highest]  = money_t::fromCString(sl[7]);

                loaded[t][c] = true;
            }
        }

        f.close();
    }

    m_valid = true;

    for (int t = 0; t < TimeCount; t++)
    {
        for (int c = 0; c < ConditionCount; c++)
        {
            m_valid &= loaded[t][c];
        }
    }

    if (m_valid)
    {
        QFileInfo fi(path);
        m_fetched = fi.lastModified();
    }
}

void BrickLink::PriceGuide::update(bool high_priority)
{
    BrickLink::inst()->addPriceGuideToUpdate(this, high_priority);
}

void BrickLink::updatePriceGuides(QVector<BrickLink::PriceGuide *> *priceGuides, bool high_priority)
{
    QString url = "https://api.bricklink.com/api/affiliate/v1/price_guide_batch?currency_code=USD&precision=3&api_key=" + QString( QUOTE(BS_BLAPIKEY) );

    QJsonArray json;
    foreach (PriceGuide *pg, *priceGuides)
    {
        QJsonObject pgItem;
        QJsonObject item;
        item["no"] = QString(pg->item()->id());
        item["type"] = pg->item()->itemType()->apiName();
        pgItem["item"] = item;
        pgItem["color_id"] = (int)pg->color()->id();

        json.append(pgItem);
    }

    QJsonDocument doc(json);
    QString strJson(doc.toJson(QJsonDocument::Compact));

    m_price_guides.transfer->postJson(url, strJson, priceGuides, high_priority);
}

void BrickLink::priceGuideJobFinished(CTransfer::Job *j)
{
    if (!j || !j->data() || !j->userObject())
    {
        return;
    }

    bool ok = false;
    QMap<QString, QJsonObject> pgItems;
    if (!j->failed())
    {
        QJsonParseError jerror;
        QJsonDocument jsonResponse = QJsonDocument::fromJson(*(j->data()), &jerror);
        ok = jerror.error == QJsonParseError::NoError && jsonResponse.object()["meta"].toObject()["code"].toInt() == 200;

        if (ok)
        {
            QJsonArray pgData = jsonResponse.object()["data"].toArray();
            foreach (QJsonValue value, pgData)
            {
                QJsonObject obj = value.toObject();
                QJsonObject item = obj["item"].toObject();

                QString key;
                QTextStream (&key) << "" << item["type"].toString() << "@" << item["no"].toString() << "@" << obj["color_id"].toInt();

                pgItems.insert(key.toLower(), obj, true);
            }
        }
    }

    QVector<BrickLink::PriceGuide *> *priceGuides = static_cast<QVector<BrickLink::PriceGuide *>*>(j->userObject());

    foreach (PriceGuide *pg, *priceGuides)
    {
        pg->m_update_status = UpdateFailed;

        if (ok)
        {
            QString key;
            QTextStream (&key) << "" << pg->item()->itemType()->apiName() << "@" << pg->item()->id() << "@" << pg->color()->id();

            QJsonObject pgItem = pgItems[key.toLower()];
            if (pgItem["item"].isObject() && pg->parse(pgItem))
            {
                pg-> m_update_status = Ok;
            }
        }

        emit priceGuideUpdated(pg);
        pg->release();
    }
}

namespace {
struct keyword {
    QString m_match;
    int     m_id;
};
}

bool BrickLink::PriceGuide::parse ( QJsonObject data )
{
    memset ( m_quantities, 0, sizeof( m_quantities ));
    memset ( m_lots, 0, sizeof( m_lots ));
    memset ( m_prices, 0, sizeof( m_prices ));

    struct keyword kw_times [] = {
        { "ordered",   PastSix },
        { "inventory", Current },
        { "" , -1 }
    };

    struct keyword kw_condition [] = {
        { "new",  New },
        { "used", Used },
        { "" , -1 }
    };

    struct keyword kw_price [] = {
        { "min_price", Lowest },
        { "", Average },
        { "", WAverage },
        { "max_price", Highest },
        { "" , -1 }
    };

    for ( keyword *tkw = kw_times; tkw->m_id != -1; tkw++ ) {
        for ( keyword *ckw = kw_condition; ckw->m_id != -1; ckw++ ) {
            m_quantities[tkw-> m_id][ckw-> m_id] = data[tkw->m_match + "_" + ckw->m_match].toObject()["unit_quantity"].toDouble();
            m_lots[tkw-> m_id][ckw-> m_id] = data[tkw->m_match + "_" + ckw->m_match].toObject()["total_quantity"].toDouble();


            for ( keyword *pkw = kw_price; pkw->m_id != -1; pkw++ ) {
                double price;

                if (!(pkw->m_match.isEmpty())) {
                    price = data[tkw->m_match + "_" + ckw->m_match].toObject()[pkw->m_match].toString().toDouble();
                }
                else {
                    if (pkw->m_id == Average) {
                        double totalquantity = data[tkw->m_match + "_" + ckw->m_match].toObject()["total_quantity"].toDouble();
                        if (totalquantity == 0)
                        {
                            price = 0;
                        }
                        else
                        {
                            double totalprice = data[tkw->m_match + "_" + ckw->m_match].toObject()["total_price"].toString().toDouble();
                            price = totalprice / totalquantity;
                        }
                    }

                    if (pkw->m_id == WAverage) {
                        double unitquantity = data[tkw->m_match + "_" + ckw->m_match].toObject()["unit_quantity"].toDouble();
                        if (unitquantity == 0)
                        {
                            price = 0;
                        }
                        else
                        {
                            double totalqtyprice = data[tkw->m_match + "_" + ckw->m_match].toObject()["total_qty_price"].toString().toDouble();
                            price = totalqtyprice / unitquantity;
                        }
                    }
                }

                m_prices [tkw-> m_id][ckw-> m_id][pkw-> m_id] = price;
            }
        }
    }

    m_valid = true;

    m_fetched = QDateTime::currentDateTime ( );
    save_to_disk ( );

    return m_valid;
}
