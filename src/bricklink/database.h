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
#pragma once

#include <QObject>
#include <QDateTime>

#include "bricklink/global.h"
#include "bricklink/color.h"
#include "bricklink/category.h"
#include "bricklink/item.h"
#include "bricklink/itemtype.h"
#include "bricklink/changelogentry.h"
#include "bricklink/partcolorcode.h"


class Transfer;

namespace BrickLink {

class Core;


class Database : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
    Q_PROPERTY(BrickLink::UpdateStatus updateStatus READ updateStatus NOTIFY updateStatusChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)

public:
    ~Database() override;

    enum class Version {
        Invalid,
        V1, // deprecated
        V2, // deprecated
        V3, // deprecated
        V4, // 2021.5.1
        V5, // 2022.1.1
        V6, // 2022.2.1

        OldestStillSupported = V4,

        Latest = V6
    };

    Q_INVOKABLE bool isUpdateNeeded() const;

    bool isValid() const          { return m_valid; }
    QDateTime lastUpdated() const { return m_lastUpdated; }
    BrickLink::UpdateStatus updateStatus() const  { return m_updateStatus; }

    static QString defaultDatabaseName(Version version = Version::Latest);

    bool startUpdate();
    bool startUpdate(bool force);
    void cancelUpdate();

    void read(const QString &fileName = { });
    void write(const QString &fileName, Version version) const;

signals:
    void updateStarted();
    void updateProgress(int received, int total);
    void updateFinished(bool success, const QString &message);
    void updateStatusChanged(BrickLink::UpdateStatus updateStatus);
    void lastUpdatedChanged(const QDateTime &lastUpdated);
    void validChanged(bool valid);
    void databaseAboutToBeReset();
    void databaseReset();

private:
    Database(QObject *parent = nullptr);
    void setUpdateStatus(UpdateStatus updateStatus);

    void clear();

    bool m_valid = false;
    BrickLink::UpdateStatus m_updateStatus = BrickLink::UpdateStatus::UpdateFailed;
    int m_updateInterval = 0;
    QDateTime m_lastUpdated;
    Transfer *m_transfer;

    std::vector<Color>               m_colors;
    std::vector<Category>            m_categories;
    std::vector<ItemType>            m_itemTypes;
    std::vector<Item>                m_items;
    std::vector<ItemChangeLogEntry>  m_itemChangelog;
    std::vector<ColorChangeLogEntry> m_colorChangelog;
    std::vector<PartColorCode>       m_pccs;

    friend class Core;
    friend class TextImport;

    // IO

    static void readColorFromDatabase(Color &col, QDataStream &dataStream, Version v);
    void writeColorToDatabase(const Color &color, QDataStream &dataStream, Version v) const;
    static void readCategoryFromDatabase(Category &cat, QDataStream &dataStream, Version v);
    void writeCategoryToDatabase(const Category &category, QDataStream &dataStream, Version v) const;
    static void readItemTypeFromDatabase(ItemType &itt, QDataStream &dataStream, Version v);
    void writeItemTypeToDatabase(const ItemType &itemType, QDataStream &dataStream, Version v) const;
    static void readItemFromDatabase(Item &item, QDataStream &dataStream, Version v);
    void writeItemToDatabase(const Item &item, QDataStream &dataStream, Version v) const;
    static void readPCCFromDatabase(PartColorCode &pcc, QDataStream &dataStream, Version v);
    void writePCCToDatabase(const PartColorCode &pcc, QDataStream &dataStream, Version v) const;
    static void readItemChangeLogFromDatabase(ItemChangeLogEntry &e, QDataStream &dataStream, Version v);
    void writeItemChangeLogToDatabase(const ItemChangeLogEntry &e, QDataStream &dataStream, Version v) const;
    static void readColorChangeLogFromDatabase(ColorChangeLogEntry &e, QDataStream &dataStream, Version v);
    void writeColorChangeLogToDatabase(const ColorChangeLogEntry &e, QDataStream &dataStream, Version v) const;
};

} // namespace BrickLink
