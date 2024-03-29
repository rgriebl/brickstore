// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <vector>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QVector>

#include "global.h"
#include "item.h"


namespace BrickLink {

class TextImport
{
public:
    TextImport();
    ~TextImport();

    bool import(const QString &path);
    void finalizeDatabase();
    void setApiQuirks(const QSet<ApiQuirk> &apiQuirks);
    void setApiKeys(const QHash<QByteArray, QString> &apiKeys);

    enum ImportInventoriesStep {
        ImportFromDiskCache,
        ImportAfterDownload,
    };

    bool importInventories(std::vector<bool> &processedInvs, ImportInventoriesStep step);

    void calculateCategoryRecency();
    void calculatePartsYearUsed();
    void calculateItemTypeCategories();
    void calculateKnownAssemblyColors();

    const std::vector<Item> &items() const;

private:
    void readColors(const QString &path);
    void readCategories(const QString &path);
    void readItemTypes(const QString &path);
    void readItems(const QString &path, const ItemType *itt);
    void readAdditionalItemCategories(const QString &path, const ItemType *itt);
    void readPartColorCodes(const QString &path);
    bool readInventory(const Item *item, ImportInventoriesStep step);
    void readLDrawColors(const QString &ldconfigPath, const QString &rebrickableColorsPath);
    void readInventoryList(const QString &path);
    void readChangeLog(const QString &path);
    void readRelationships(const QString &path);

    int findItemIndex(char type, const QByteArray &id) const;
    int findColorIndex(uint id) const;
    int findCategoryIndex(uint id) const;

    void calculateColorPopularity();
    void addToKnownColors(int itemIndex, int colorIndex);

private:
    Database *m_db;
    // item-idx -> { color-idx -> { vector < qty, item-idx > } }
    QHash<uint, QHash<uint, QVector<QPair<int, uint>>>> m_appears_in_hash;
    // item-idx -> { vector < consists-of > }
    QHash<uint, QVector<Item::ConsistsOf>>   m_consists_of_hash;
    // item-idx -> secs since epoch
    QHash<uint, qint64> m_inventoryLastUpdated;
};

} // namespace BrickLink
