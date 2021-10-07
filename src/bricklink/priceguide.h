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

#include <QtCore/QDateTime>

#include "global.h"
#include "utility/ref.h"

class TransferJob;


namespace BrickLink {

class PriceGuide : public Ref
{
public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }

    void update(bool highPriority = false);
    QDateTime lastUpdated() const     { return m_fetched; }
    void cancelUpdate();

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_update_status; }

    int quantity(Time t, Condition c) const           { return m_data.quantities[int(t)][int(c)]; }
    int lots(Time t, Condition c) const               { return m_data.lots[int(t)][int(c)]; }
    double price(Time t, Condition c, Price p) const  { return m_data.prices[int(t)][int(c)][int(p)]; }

    PriceGuide(std::nullptr_t) : PriceGuide(nullptr, nullptr) { } // for scripting only!
    ~PriceGuide() override;

private:
    const Item *  m_item;
    const Color * m_color;

    QDateTime     m_fetched;

    bool          m_valid;
    bool          m_updateAfterLoad;
    bool          m_scrapedHtml = s_scrapeHtml;
    static bool   s_scrapeHtml;
    // 1 byte padding here
    UpdateStatus  m_update_status;

    TransferJob * m_transferJob = nullptr;

    struct Data {
        int    quantities [int(Time::Count)][int(Condition::Count)] = { };
        int    lots       [int(Time::Count)][int(Condition::Count)] = { };
        double prices     [int(Time::Count)][int(Condition::Count)][int(Price::Count)] = { };
    } m_data;

private:
    PriceGuide(const Item *item, const Color *color);

    bool loadFromDisk(QDateTime &fetched, Data &data) const;
    void saveToDisk(const QDateTime &fetched, const Data &data);

    bool parse(const QByteArray &ba, Data &result) const;
    bool parseHtml(const QByteArray &ba, Data &result);

    friend class Core;
    friend class PriceGuideLoaderJob;
};

} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::PriceGuide *)


// tell Qt that PriceGuides are shared and can't simply be deleted
// (Q3Cache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<BrickLink::PriceGuide>(BrickLink::PriceGuide &c) { return c.refCount() == 0; }
