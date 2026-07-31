// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>

#include <QtCore/QDateTime>

#include "bricklink/global.h"

class Transfer;
class TransferJob;


namespace BrickLink {

class PriceGuideCache;
class PriceGuide;

// Price guides live in the PriceGuideCache, but they are shared: whoever displays one keeps it alive
// for as long as it is needed, whether it is still cached or not. Always hold on to a price guide via
// a PriceGuideRef, never via a raw pointer.
using PriceGuideRef = std::shared_ptr<PriceGuide>;

// Not exposed to QML: BrickLink::QmlPriceGuide is the QML facing half, as QML needs an object whose
// lifetime it can manage in order to hold on to a reference at all.
class PriceGuide : public QObject
{
    Q_OBJECT

    struct Private { };

public:
    // Both return nullptr once the database has been swapped out from under us: the pointers are
    // raw and the objects they pointed at are long gone by then. See isStale().
    const Item *item() const;
    const Color *color() const;
    VatType vatType() const           { return m_vatType; }

    // A price guide outlives its cache entry whenever someone still holds a reference, and a
    // database update can happen in between. Such a price guide cannot be used for anything
    // anymore - it is not even possible to tell which item it belonged to.
    bool isStale() const;
    QDateTime lastUpdated() const     { return m_lastUpdated; }

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_updateStatus; }

    int quantity(BrickLink::Time t, BrickLink::Condition c) const           { return m_data.quantities[int(t)][int(c)]; }
    int lots(BrickLink::Time t, BrickLink::Condition c) const               { return m_data.lots[int(t)][int(c)]; }
    double price(BrickLink::Time t, BrickLink::Condition c, BrickLink::Price p) const  { return m_data.prices[int(t)][int(c)][int(p)]; }

    PriceGuide(Private, const Item *item, const Color *color, VatType vatType);
    ~PriceGuide() override;
    Q_DISABLE_COPY_MOVE(PriceGuide)

    struct Data
    {
        int    quantities [int(Time::Count)][int(Condition::Count)] = { };
        int    lots       [int(Time::Count)][int(Condition::Count)] = { };
        double prices     [int(Time::Count)][int(Condition::Count)][int(Price::Count)] = { };
    };


signals:
    void isValidChanged(bool newIsValid);
    void lastUpdatedChanged(const QDateTime &newLastUpdated);
    void updateStatusChanged(BrickLink::UpdateStatus newUpdateStatus);

private:
    const Item * m_item;
    const Color *m_color;

    QDateTime    m_lastUpdated;

    quint32      m_generation      = 0; // of the database the item/color pointers point into

    VatType      m_vatType         : 8 = VatType::Excluded;
    char         m_retrieverId     : 8 = '0';
    bool         m_valid           : 1 = false;
    bool         m_updateAfterLoad : 1 = false;
    UpdateStatus m_updateStatus    : 3 = UpdateStatus::Ok;
    uint         m_reserved        : 11 = 0;

    Data         m_data;

private:
    void setIsValid(bool valid);
    void setUpdateStatus(UpdateStatus status);
    void setLastUpdated(const QDateTime &dt);

    friend class PriceGuideCache;
    friend class PriceGuideCachePrivate;
};


class PriceGuideCachePrivate;

class PriceGuideCache : public QObject
{
    Q_OBJECT

public:
    explicit PriceGuideCache(Core *core);
    ~PriceGuideCache() override;

    void setUpdateInterval(int interval);
    void clearCache();
    QPair<int, int> cacheStats() const;

    PriceGuideRef priceGuide(const Item *item, const Color *color, bool highPriority = false);
    PriceGuideRef priceGuide(const Item *item, const Color *color, VatType vatType,
                             bool highPriority = false);

    void updatePriceGuide(const PriceGuideRef &pg, bool highPriority = false);
    void cancelPriceGuideUpdate(const PriceGuideRef &pg);
    void cancelAllPriceGuideUpdates();

    QString retrieverName() const;
    QString retrieverId() const;

    QVector<VatType> supportedVatTypes() const;
    bool setCurrentVatType(VatType vatType);
    VatType currentVatType() const;

    static QIcon iconForVatType(VatType vatType);
    static QString descriptionForVatType(VatType vatType);

signals:
    // carries a reference, so a slot cannot be handed a price guide that dies while it runs
    void priceGuideUpdated(const BrickLink::PriceGuideRef &pg);
    void currentVatTypeChanged(BrickLink::VatType vatType);

private:
    PriceGuideCachePrivate *d;
};


} // namespace BrickLink

// std::shared_ptr, unlike QSharedPointer, has no automatic metatype: needed for TransferJob's
// user data, which is what keeps a price guide alive while it is being fetched.
Q_DECLARE_METATYPE(BrickLink::PriceGuideRef)
