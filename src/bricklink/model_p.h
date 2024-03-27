// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QAbstractItemModel>

#include "bricklink/item.h"
#include "bricklink/model.h"


namespace BrickLink {

class InventoryModel;


class InternalInventoryModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ~InternalInventoryModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    struct Entry
    {
        Entry() = default;
        Entry(const QString &sectionTitle);
        Entry(const BrickLink::Item *item, const BrickLink::Color *color, int quantity);
        ~Entry();

        bool isSection() const { return !m_item && !m_sectionTitle.isEmpty(); }

        const BrickLink::Item *m_item = nullptr;
        const BrickLink::Color *m_color = nullptr;
        int m_quantity = 0;
        QString m_sectionTitle;

        Q_DISABLE_COPY_MOVE(Entry)
    };

    const Entry *entry(const QModelIndex &idx) const;

    using Mode = InventoryModel::Mode;
    using SimpleLot = InventoryModel::SimpleLot;

protected:
    InternalInventoryModel(Mode mode, const QVector<SimpleLot> &list, QObject *parent);

    void fillConsistsOf(const QVector<SimpleLot> &list);
    void fillAppearsIn(const QVector<SimpleLot> &list);
    void fillCanBuild(const QVector<SimpleLot> &lots);
    void fillRelationships(const QVector<SimpleLot> &lots);

    QVector<Entry *> m_entries;
    Mode m_mode;

    friend class InventoryModel;
private:
    Q_DISABLE_COPY_MOVE(InternalInventoryModel)
};

} // namespace BrickLink
