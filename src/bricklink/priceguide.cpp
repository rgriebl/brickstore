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

#include <QtCore/QScopedPointer>
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


bool BrickLink::PriceGuide::s_scrapeHtml = true;

BrickLink::PriceGuide::PriceGuide(const BrickLink::Item *item, const BrickLink::Color *color)
    : m_item(item)
    , m_color(color)
{
    m_valid = false;
    m_updateAfterLoad = false;
    m_update_status = UpdateStatus::Ok;
}

BrickLink::PriceGuide::~PriceGuide()
{
    cancelUpdate();
}

void BrickLink::PriceGuide::saveToDisk(const QDateTime &fetched, const Data &data)
{
    QScopedPointer<QSaveFile> f(core()->dataSaveFile(u"priceguide.txt", m_item, m_color));

    if (f && f->isOpen()) {
        QTextStream ts(f.data());

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

bool BrickLink::PriceGuide::loadFromDisk(QDateTime &fetched, Data &data) const
{
    if (!m_item || !m_color)
        return false;

    QScopedPointer<QFile> f(core()->dataReadFile(u"priceguide.txt", m_item, m_color));

    if (f && f->isOpen()) {
        if (parse(f->readAll(), data)) {
            fetched = f->fileTime(QFileDevice::FileModificationTime);
            return true;
        }
    }
    return false;
}

bool BrickLink::PriceGuide::parse(const QByteArray &ba, Data &result) const
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

bool BrickLink::PriceGuide::parseHtml(const QByteArray &ba, Data &result)
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


void BrickLink::PriceGuide::update(bool highPriority)
{
    core()->updatePriceGuide(this, highPriority);
}

void BrickLink::PriceGuide::cancelUpdate()
{
    if (core())
        core()->cancelPriceGuideUpdate(this);
}
