// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <vector>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QVector>

#include "global.h"
#include "changelogentry.h"
#include "item.h"


namespace BrickLink {

class TextImport
{
public:
    TextImport();
    ~TextImport();

    bool import(const QString &path);
    void exportTo(Database *db);

    enum ImportInventoriesStep {
        ImportFromDiskCache,
        ImportAfterDownload,
    };

    bool importInventories(std::vector<bool> &processedInvs, ImportInventoriesStep step);

    void calculateCategoryRecency();
    void calculatePartsYearUsed();
    void calculateItemTypeCategories();

    const std::vector<Item> &items() const { return m_items; }

private:
    void readColors(const QString &path);
    void readCategories(const QString &path);
    void readItemTypes(const QString &path);
    void readItems(const QString &path, ItemType *itt);
    void readAdditionalItemCategories(const QString &path, BrickLink::ItemType *itt);
    void readPartColorCodes(const QString &path);
    bool readInventory(const Item *item, ImportInventoriesStep step);
    void readLDrawColors(const QString &ldconfigPath, const QString &rebrickableColorsPath);
    void readInventoryList(const QString &path);
    void readChangeLog(const QString &path);

    int findItemIndex(char type, const QByteArray &id) const;
    int findColorIndex(uint id) const;
    int findCategoryIndex(uint id) const;

    void calculateColorPopularity();
    void addToKnownColors(int itemIndex, int colorIndex);

private:
    std::vector<Color>         m_colors;
    std::vector<Color>         m_ldrawExtraColors;
    std::vector<ItemType>      m_item_types;
    std::vector<Category>      m_categories;
    std::vector<Item>          m_items;
    std::vector<QByteArray>    m_changelog;
    std::vector<ItemChangeLogEntry>  m_itemChangelog;
    std::vector<ColorChangeLogEntry> m_colorChangelog;
    std::vector<PartColorCode> m_pccs;
    // item-idx -> { color-idx -> { vector < qty, item-idx > } }
    QHash<uint, QHash<uint, QVector<QPair<int, uint>>>> m_appears_in_hash;
    // item-idx -> { vector < consists-of > }
    QHash<uint, QVector<Item::ConsistsOf>>   m_consists_of_hash;
};

} // namespace BrickLink
