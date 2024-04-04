// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <vector>

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QVector>
#include <QCoro/QCoroTask>

#include "global.h"
#include "item.h"


class MiniZip;
class Transfer;

namespace BrickLink {

class TextImport
{
public:
    explicit TextImport();
    ~TextImport();


    void initialize(bool skipDownload);
    QCoro::Task<> login(const QString &username, const QString &password,
                        const QString &rebrickableApiKey);
    QCoro::Task<> importCatalog();
    QCoro::Task<> importInventories();
    void finalize();
    void exportDatabase();

    void setApiQuirks(const QSet<ApiQuirk> &apiQuirks);
    void setApiKeys(const QHash<QByteArray, QString> &apiKeys);

private:
    QCoro::Task<QByteArray> download(const QUrl &url, const QString &fileName);

    void readColors(const QByteArray &xml);
    void readCategories(const QByteArray &xml);
    void readItemTypes(const QByteArray &xml);
    void readItems(const QByteArray &xml, const ItemType *itt);
    void readAdditionalItemCategories(const QByteArray &csv, const ItemType *itt);
    void readPartColorCodes(const QByteArray &xml);
    const Item *readInventory(char itemTypeId, const QByteArray &itemId,
                              const QDateTime &lastModified, const QByteArray &xml,
                              bool failSilently = false);
    void readLDrawColors(const QByteArray &ldconfig, const QByteArray &rebrickableColors);
    void readInventoryList(const QByteArray &csv);
    void readChangeLog(const QByteArray &csv);
    QCoro::Task<> readRelationships(const QByteArray &html);

    void calculateColorPopularity();
    void calculateItemTypeCategories();
    void calculateKnownAssemblyColors();
    void calculateCategoryRecency();
    void calculatePartsYearUsed();

    void addToKnownColors(int itemIndex, int colorIndex);

    int loadLastRunInventories(const std::function<void (char, const QByteArray &, const QDateTime &, const QByteArray &)> &callback);

    void nextStep(const QString &text);
    void message(const QString &text);
    void message(int level, const QString &text);

private:
    std::unique_ptr<Transfer> m_trans;
    QString m_archiveName;
    std::unique_ptr<MiniZip> m_downloadArchive;
    std::unique_ptr<MiniZip> m_lastRunArchive;

    Database *m_db;
    // item-idx -> { color-idx -> { vector < qty, item-idx > } }
    QHash<uint, QHash<uint, QVector<QPair<int, uint>>>> m_appears_in_hash;
    // item-idx -> { vector < consists-of > }
    QHash<uint, QVector<Item::ConsistsOf>>   m_consists_of_hash;
    // item-idx -> secs since epoch
    QHash<uint, qint64> m_inventoryLastUpdated;

    QString m_rebrickableApiKey;
    bool m_skipDownload = false;
    int m_currentStep = 0;
};

} // namespace BrickLink
