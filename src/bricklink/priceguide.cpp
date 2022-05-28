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

#include <QtCore/QLocale>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QTextStream>
#include <QtCore/QRegularExpression>

#include "bricklink/priceguide.h"
#include "bricklink/core.h"
#include "bricklink/item.h"
#include "bricklink/color.h"
#include "utility/utility.h"


namespace BrickLink {

bool PriceGuide::s_scrapeHtml = true;

PriceGuide::PriceGuide(const Item *item, const Color *color)
    : m_item(item)
    , m_color(color)
{
    m_valid = false;
    m_updateAfterLoad = false;
    m_update_status = UpdateStatus::Ok;
}

PriceGuide::~PriceGuide()
{
    cancelUpdate();
}

void PriceGuide::saveToDisk(const QDateTime &fetched, const Data &data)
{
    std::unique_ptr<QSaveFile> f(core()->dataSaveFile(u"priceguide.txt", m_item, m_color));

    if (f && f->isOpen()) {
        QTextStream ts(f.get());

        ts << "# Price Guide for part #" << m_item->id() << " (" << m_item->name() << "), color #" << m_color->id() << " (" << m_color->name() << ")\n";
        ts << "# last update: " << fetched.toString() << "\n#\n";

        for (int ti = 0; ti < int(Time::Count); ti++) {
            for (int ci = 0; ci < int(Condition::Count); ci++) {
                ts << (ti == int(Time::PastSix) ? 'O' : 'I') << '\t' << (ci == int(Condition::New) ? 'N' : 'U') << '\t'
                   << data.lots[ti][ci] << '\t'
                   << data.quantities[ti][ci] << '\t'
                   << QString::number(data.prices[ti][ci][int(Price::Lowest)], 'f', 3) << '\t'
                   << QString::number(data.prices[ti][ci][int(Price::Average)], 'f', 3) << '\t'
                   << QString::number(data.prices[ti][ci][int(Price::WAverage)], 'f', 3) << '\t'
                   << QString::number(data.prices[ti][ci][int(Price::Highest)], 'f', 3) << '\n';
            }
        }
        f->commit();
    }
}

void PriceGuide::setIsValid(bool valid)
{
    if (valid != m_valid) {
        m_valid = valid;
        emit isValidChanged(valid);
    }
}

void PriceGuide::setUpdateStatus(UpdateStatus status)
{
    if (status != m_update_status) {
        m_update_status = status;
        emit updateStatusChanged(status);
    }
}

void PriceGuide::setLastUpdated(const QDateTime &dt)
{
    if (dt != m_fetched) {
        m_fetched = dt;
        emit lastUpdatedChanged(dt);
    }
}


bool PriceGuide::loadFromDisk(QDateTime &fetched, Data &data) const
{
    if (!m_item || !m_color)
        return false;

    std::unique_ptr<QFile> f(core()->dataReadFile(u"priceguide.txt", m_item, m_color));

    if (f && f->isOpen()) {
        if (parse(f->readAll(), data)) {
            fetched = f->fileTime(QFileDevice::FileModificationTime);
            return true;
        }
    }
    return false;
}

bool PriceGuide::parse(const QByteArray &ba, Data &result) const
{
    result = { };

    QTextStream ts(ba);
    QString line;

    while (!(line = ts.readLine()).isNull()) {
        if (line.isEmpty() || (line[0] == '#'_l1) || (line[0] == '\r'_l1))         // skip comments fast
            continue;

        QStringList sl = line.split('\t'_l1, Qt::KeepEmptyParts);

        if ((sl.count() != 8) || (sl[0].length() != 1) || (sl[1].length() != 1)) {             // sanity check
            continue;
        }

        int ti = -1;
        int ci = -1;
        bool oldformat = false;

        switch (sl[0][0].toLatin1()) {
        case 'P': oldformat = true;
                  Q_FALLTHROUGH();
        case 'O': ti = int(Time::PastSix);
                  break;
        case 'C': oldformat = true;
                  Q_FALLTHROUGH();
        case 'I': ti = int(Time::Current);
                  break;
        }

        switch (sl[1][0].toLatin1()) {
        case 'N': ci = int(Condition::New);  break;
        case 'U': ci = int(Condition::Used); break;
        }

        if ((ti != -1) && (ci != -1)) {
            result.lots[ti][ci]                         = sl[oldformat ? 3 : 2].toInt();
            result.quantities[ti][ci]                   = sl[oldformat ? 2 : 3].toInt();
            result.prices[ti][ci][int(Price::Lowest)]   = sl[4].toDouble();
            result.prices[ti][ci][int(Price::Average)]  = sl[5].toDouble();
            result.prices[ti][ci][int(Price::WAverage)] = sl[6].toDouble();
            result.prices[ti][ci][int(Price::Highest)]  = sl[7].toDouble();
        }
    }
    return true;
}

bool PriceGuide::parseHtml(const QByteArray &ba, Data &result)
{
    static const QRegularExpression re(R"(<B>([A-Za-z-]*?): </B><.*?> (\d+) <.*?> (\d+) <.*?> US \$([0-9.,]+) <.*?> US \$([0-9.,]+) <.*?> US \$([0-9.,]+) <.*?> US \$([0-9.,]+) <)"_l1);

    QString s = QString::fromUtf8(ba).replace("&nbsp;"_l1, " "_l1);
    QLocale en_US("en_US"_l1);

    result = { };

    int matchCounter = 0;
    int startPos = 0;

    int currentPos = s.indexOf("Current Items for Sale"_l1);
    bool hasCurrent = (currentPos > 0);
    int pastSixPos = s.indexOf("Past 6 Months Sales"_l1);
    bool hasPastSix = (pastSixPos > 0);

//    qWarning() << s;

    for (int i = 0; i < 4; ++i) {
        auto m = re.match(s, startPos);
        if (m.hasMatch()) {
            int ti = -1;
            int ci = -1;

            int matchPos = m.capturedStart(0);
            int matchEnd = m.capturedEnd(0);

            // if both pastSix and current are available, pastSix comes first
            if (hasCurrent && (matchPos > currentPos))
                ti = int(Time::Current);
            else if (hasPastSix && (matchPos > pastSixPos))
                ti = int(Time::PastSix);

            const QString condStr = m.captured(1);
            if (condStr == "Used"_l1) {
                ci = int(Condition::Used);
            } else if (condStr == "New"_l1) {
                ci = int(Condition::New);
            } else if (condStr.isEmpty()) {
                ci = int(Condition::New);
                if (result.lots[ti][ci])
                    ci = int(Condition::Used);
            }

//            qWarning() << i << ti << ci << m.capturedTexts().mid(1);
//            qWarning() << "   start:" << startPos << "match start:" << matchPos << "match end:" << matchEnd;

            if ((ti == -1) || (ci == -1))
                continue;

            result.lots[ti][ci]                         = en_US.toInt(m.captured(2));
            result.quantities[ti][ci]                   = en_US.toInt(m.captured(3));
            result.prices[ti][ci][int(Price::Lowest)]   = en_US.toDouble(m.captured(4));
            result.prices[ti][ci][int(Price::Average)]  = en_US.toDouble(m.captured(5));
            result.prices[ti][ci][int(Price::WAverage)] = en_US.toDouble(m.captured(6));
            result.prices[ti][ci][int(Price::Highest)]  = en_US.toDouble(m.captured(7));

            ++matchCounter;
            startPos = matchEnd;
        }
    }
    return (matchCounter > 0);
}


void PriceGuide::update(bool highPriority)
{
    core()->updatePriceGuide(this, highPriority);
}

void PriceGuide::cancelUpdate()
{
    if (core())
        core()->cancelPriceGuideUpdate(this);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype PriceGuide
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents the price guide for a BrickLink item.

    Each price guide of an item in the BrickLink catalog is available as a PriceGuide object.

    You cannot create PriceGuide objects yourself, but you can retrieve a PriceGuide object given the
    item and color id via BrickLink::priceGuide().

    \note PriceGuides aren't readily available, but need to be asynchronously loaded (or even
          downloaded) at runtime. You need to connect to the signal BrickLink::priceGuideUpdated()
          to know when the data has been loaded.

    The following three enumerations are used to retrieve the price guide data from this object:

    \b Time
    \value BrickLink.PastSix   The sales in the last six months.
    \value BrickLink.Current   The items currently for sale.

    \b Condition
    \value BrickLink.New       Only items in new condition.
    \value BrickLink.Used      Only items in used condition.

    \b Price
    \value BrickLink.Lowest    The lowest price.
    \value BrickLink.Average   The average price.
    \value BrickLink.WAverage  The weighted average price.
    \value BrickLink.Highest   The highest price.

*/
/*! \qmlproperty bool PriceGuide::isNull
    \readonly
    Returns whether this PriceGuide is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty Item PriceGuide::item
    \readonly
    The BrickLink item reference this price guide is requested for.
*/
/*! \qmlproperty Color PriceGuide::color
    \readonly
    The BrickLink color reference this price guide is requested for.
*/
/*! \qmlproperty date PriceGuide::lastUpdated
    \readonly
    Holds the time stamp of the last successful update of this price guide.
*/
/*! \qmlproperty UpdateStatus PriceGuide::updateStatus
    \readonly
    Returns the current update status. The available values are:
    \value BrickLink.Ok            The last picture load (or download) was successful.
    \value BrickLink.Loading       BrickStore is currently loading the picture from the local cache.
    \value BrickLink.Updating      BrickStore is currently downloading the picture from BrickLink.
    \value BrickLink.UpdateFailed  The last download from BrickLink failed. isValid might still be
                                   \c true, if there was a valid picture available before the failed
                                   update!
*/
/*! \qmlproperty bool PriceGuide::isValid
    \readonly
    Returns whether this object currently holds valid price guide data.
*/
/*! \qmlmethod PriceGuide::update(bool highPriority = false)
    Tries to re-download the price guide from the BrickLink server. If you set \a highPriority to \c
    true the load/download request will be pre-prended to the work queue instead of appended.
*/
/*! \qmlmethod int PriceGuide::quantity(Time time, Condition condition)
    Returns the number of items for sale (or item that have been sold) given the \a time frame and
    \a condition. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time and
    Condition enumerations.
*/
/*! \qmlmethod int PriceGuide::lots(Time time, Condition condition)
    Returns the number of lots for sale (or lots that have been sold) given the \a time frame and
    \a condition. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time and
    Condition enumerations.
*/
/*! \qmlmethod real PriceGuide::price(Time time, Condition condition, Price price)
    Returns the price of items for sale (or item that have been sold) given the \a time frame,
    \a condition and \a price type. Returns \c 0 if no data is available.
    See the PriceGuide type documentation for the possible values of the Time,
    Condition and Price enumerations.
*/

} // namespace BrickLink
