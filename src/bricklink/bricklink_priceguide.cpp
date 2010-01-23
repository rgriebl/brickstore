/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#include <cstring>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QLocale>

#include "bricklink.h"


BrickLink::PriceGuide *BrickLink::Core::priceGuide(const BrickLink::Item *item, const BrickLink::Color *color, bool high_priority)
{
    if (!item || !color)
        return 0;

    quint64 key = quint64(color->id()) << 32 | quint64(item->itemType()->id()) << 24 | quint64(item->index());

    PriceGuide *pg = m_pg_cache [key];

    if (!pg) {
        pg = new PriceGuide(item, color);
        if (!m_pg_cache.insert(key, pg)) {
            qWarning("Can not add priceguide to cache (cache max/cur: %d/%d, cost: %d)", m_pic_cache.maxCost(), m_pic_cache.totalCost(), 1);
            return 0;
        }
    }

    if (pg && (updateNeeded(pg->valid(), pg->lastUpdate(), m_pg_update_iv)))
        updatePriceGuide(pg, high_priority);

    return pg;
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
    //qDebug ( "Deleting PG %p for %c_%s_%d", this, m_item->itemType ( )->id ( ), m_item->id ( ), m_color->id ( ));
}

void BrickLink::PriceGuide::save_to_disk()
{
    QString path = BrickLink::core()->dataPath(m_item, m_color);

    if (path.isEmpty())
        return;
    path += "priceguide.txt";

    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        QTextStream ts(&f);

        ts << "# Price Guide for part #" << m_item->id() << " (" << m_item->name() << "), color #" << m_color->id() << " (" << m_color->name() << ")\n";
        ts << "# last update: " << m_fetched.toString() << "\n#\n";

        for (int t = 0; t < TimeCount; t++) {
            for (int c = 0; c < ConditionCount; c++) {
                ts << (t == PastSix ? 'O' : 'I') << '\t' << (c == New ? 'N' : 'U') << '\t'
                   << m_lots [t][c] << '\t'
                   << m_quantities [t][c] << '\t'
                   << m_prices [t][c][Lowest].toUSD() << '\t'
                   << m_prices [t][c][Average].toUSD() << '\t'
                   << m_prices [t][c][WAverage].toUSD() << '\t'
                   << m_prices [t][c][Highest].toUSD() << '\n';
            }
        }
    }
}

void BrickLink::PriceGuide::load_from_disk()
{
    QString path = BrickLink::core()->dataPath(m_item, m_color);

    if (path.isEmpty())
        return;
    path += "priceguide.txt";

    m_valid = false;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        parse(f.readAll());
        f.close();
    }

    if (m_valid) {
        QFileInfo fi(path);
        m_fetched = fi.lastModified();
    }
}

void BrickLink::PriceGuide::parse(const QByteArray &ba)
{
    memset(m_quantities, 0, sizeof(m_quantities));
    memset(m_lots, 0, sizeof(m_lots));
    memset(m_prices, 0, sizeof(m_prices));

    QTextStream ts(ba);
    QString line;

    while (!(line = ts.readLine()).isNull()) {
        if (!line.length() || (line[0] == '#') || (line[0] == '\r'))         // skip comments fast
            continue;

        QStringList sl = line.split('\t', QString::KeepEmptyParts);

        if ((sl.count() != 8) || (sl[0].length() != 1) || (sl[1].length() != 1)) {             // sanity check
            continue;
        }

        int t = -1;
        int c = -1;
        bool oldformat = false;

        switch (sl [0][0].toLatin1()) {
        case 'P': oldformat = true;
        case 'O': t = PastSix;
                  break;
        case 'C': oldformat = true;
        case 'I': t = Current;
                  break;
        }

        switch (sl [1][0].toLatin1()) {
        case 'N': c = New;  break;
        case 'U': c = Used; break;
        }

        if ((t != -1) && (c != -1)) {
            m_lots[t][c]             = sl[oldformat ? 3 : 2].toInt();
            m_quantities[t][c]       = sl[oldformat ? 2 : 3].toInt();
            m_prices[t][c][Lowest]   = Currency::fromUSD(sl[4]);
            m_prices[t][c][Average]  = Currency::fromUSD(sl[5]);
            m_prices[t][c][WAverage] = Currency::fromUSD(sl[6]);
            m_prices[t][c][Highest]  = Currency::fromUSD(sl[7]);
        }
    }

    m_valid = true;
}


void BrickLink::PriceGuide::update(bool high_priority)
{
    BrickLink::core()->updatePriceGuide(this, high_priority);
}


void BrickLink::Core::updatePriceGuide(BrickLink::PriceGuide *pg, bool high_priority)
{
    if (!pg || (pg->m_update_status == Updating))
        return;

    if (!m_online || !m_transfer) {
        pg->m_update_status = UpdateFailed;
        emit priceGuideUpdated(pg);
        return;
    }

    pg->m_update_status = Updating;
    pg->addRef();

    QUrl url("http://www.bricklink.com/BTpriceSummary.asp"); //?{item type}={item no}&colorID={color ID}&cCode={currency code}&cExc={Y to exclude incomplete sets}

    url.addQueryItem(QChar(pg->item()->itemType()->id()).toUpper(),
                                 pg->item()->id());
    url.addQueryItem("colorID",  QString::number(pg->color()->id()));
    url.addQueryItem("cCode",    "USD");
    url.addQueryItem("cExc",     "Y"); //  Y == exclude incomplete sets

    //qDebug ( "PG request started for %s", (const char *) url );
    TransferJob *job = TransferJob::get(url);
    job->setUserData('G', pg);
    m_transfer->retrieve(job, high_priority);
}


void BrickLink::Core::priceGuideJobFinished(ThreadPoolJob *pj)
{
    TransferJob *j = static_cast<TransferJob *>(pj);

    if (!j || !j->data())
        return;

    PriceGuide *pg = j->userData<PriceGuide>('G');

    if (!pg)
        return;

    pg->m_update_status = UpdateFailed;

    if (j->isCompleted()) {
        pg->parse(*j->data());
        if (pg->m_valid) {
            pg->m_fetched = QDateTime::currentDateTime();
            pg->save_to_disk();
            pg->m_update_status = Ok;
        }
    }

    emit priceGuideUpdated(pg);
    pg->release();
}
