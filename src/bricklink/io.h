// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtXml/QDomElement>

#include "bricklink/global.h"
#include "bricklink/lot.h"

namespace BrickLink::IO {

class ParseResult
{
public:
    ParseResult() = default;
    ParseResult(const LotList &lots);
    ParseResult(const ParseResult &) = delete;
    ParseResult(ParseResult &&pr) noexcept;

    virtual ~ParseResult();

    bool hasLots() const         { return !m_lots.isEmpty(); }
    const LotList &lots() const  { return m_lots; }
    LotList takeLots();
    QString currencyCode() const { return m_currencyCode; }
    Order *takeOrder();
    int invalidLotCount() const  { return m_invalidLotCount; }
    int fixedLotCount() const    { return m_fixedLotCount; }
    const QHash<const Lot *, Lot> &differenceModeBase() const { return m_differenceModeBase; }

    void addLot(Lot *&&lot);
    void setCurrencyCode(const QString &ccode) { m_currencyCode = ccode; }
    void incInvalidLotCount()    { ++m_invalidLotCount; }
    void incFixedLotCount()      { ++m_fixedLotCount; }
    void addToDifferenceModeBase(const Lot *lot, const Lot &base);

private:
    LotList m_lots;
    QString m_currencyCode;

    bool m_ownLots = true;
    int m_invalidLotCount = 0;
    int m_fixedLotCount = 0;

    QHash<const Lot *, Lot> m_differenceModeBase;
};

QString toWantedListXML(const LotList &lots, const QString &wantedList);
QString toInventoryRequest(const LotList &lots);
QString toBrickLinkUpdateXML(const LotList &lots,
                             const std::function<const Lot *(const Lot *)> &differenceBaseLot);

enum class Hint {
    Plain = 0x01,
    Wanted = 0x02,
    Order = 0x04,
    Store = 0x08,

    PlainOrWanted = Plain | Wanted
};

QString toBrickLinkXML(const LotList &lots);
ParseResult fromBrickLinkXML(const QByteArray &xml, Hint hint, const QDateTime &creationTime = { });

ParseResult fromPartInventory(const Item *item, const Color *color = nullptr, int quantity = 1,
                              Condition condition = Condition::New, Status extraParts = Status::Extra,
                              PartOutTraits partOutTraits = { }, Status status = Status::Include);

} // namespace BrickLink::IO
