// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QDateTime>
#include <QtQml/qqmlregistration.h>

#include "bricklink/global.h"
#include "utility/ref.h"

class Transfer;
class TransferJob;


namespace BrickLink {

class PriceGuideCache;

class PriceGuide : public QObject, public Ref
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(const BrickLink::Item *item READ item CONSTANT FINAL)
    Q_PROPERTY(const BrickLink::Color *color READ color CONSTANT FINAL)
    Q_PROPERTY(BrickLink::VatType vatType READ vatType CONSTANT FINAL)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged FINAL)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged FINAL)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged FINAL)

public:
    const Item *item() const          { return m_item; }
    const Color *color() const        { return m_color; }
    VatType vatType() const           { return m_vatType; }

    Q_INVOKABLE void update(bool highPriority = false);
    QDateTime lastUpdated() const     { return m_lastUpdated; }
    Q_INVOKABLE void cancelUpdate();

    bool isValid() const              { return m_valid; }
    UpdateStatus updateStatus() const { return m_updateStatus; }

    Q_INVOKABLE int quantity(BrickLink::Time t, BrickLink::Condition c) const           { return m_data.quantities[int(t)][int(c)]; }
    Q_INVOKABLE int lots(BrickLink::Time t, BrickLink::Condition c) const               { return m_data.lots[int(t)][int(c)]; }
    Q_INVOKABLE double price(BrickLink::Time t, BrickLink::Condition c, BrickLink::Price p) const  { return m_data.prices[int(t)][int(c)][int(p)]; }

    PriceGuide(std::nullptr_t) : PriceGuide(nullptr, nullptr, VatType::Excluded) { } // for scripting only!
    ~PriceGuide() override;

    Q_INVOKABLE void addRef()         { Ref::addRef(); }
    Q_INVOKABLE void release()        { Ref::release(); }
    Q_INVOKABLE int refCount() const  { return Ref::refCount(); }

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

    VatType      m_vatType         : 8 = VatType::Excluded;
    char         m_retrieverId     : 8 = '0';
    bool         m_valid           : 1 = false;
    bool         m_updateAfterLoad : 1 = false;
    UpdateStatus m_updateStatus    : 3 = UpdateStatus::Ok;
    uint         m_reserved        : 11 = 0;

    Data         m_data;

    static PriceGuideCache *s_cache;

private:
    PriceGuide(const Item *item, const Color *color, VatType vatType);

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

    PriceGuide *priceGuide(const Item *item, const Color *color, bool highPriority = false);
    PriceGuide *priceGuide(const Item *item, const Color *color, VatType vatType,
                           bool highPriority = false);

    void updatePriceGuide(PriceGuide *pg, bool highPriority = false);
    void cancelPriceGuideUpdate(PriceGuide *pg);

    QString retrieverName() const;
    QString retrieverId() const;

    QVector<VatType> supportedVatTypes() const;
    bool setCurrentVatType(VatType vatType);
    VatType currentVatType() const;

    static QIcon iconForVatType(VatType vatType);
    static QString descriptionForVatType(VatType vatType);

signals:
    void priceGuideUpdated(BrickLink::PriceGuide *pg);
    void currentVatTypeChanged(BrickLink::VatType vatType);

private:
    PriceGuideCachePrivate *d;
};


} // namespace BrickLink

Q_DECLARE_METATYPE(BrickLink::PriceGuide *)

// tell Qt that PriceGuides are shared and can't simply be deleted
// (Q3Cache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<BrickLink::PriceGuide>(BrickLink::PriceGuide &c) { return c.refCount() == 0; }
