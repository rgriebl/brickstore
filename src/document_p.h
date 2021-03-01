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

#include <QUndoCommand>
#include "document.h"


class AddRemoveCmd : public QUndoCommand
{
public:
    enum Type { Add, Remove };

    AddRemoveCmd(Type t, Document *doc, const QVector<int> &positions,
                 const QVector<int> &sortedPositions, const QVector<int> &filteredPositions,
                 const Document::ItemList &items);
    ~AddRemoveCmd() override;
    int id() const override;

    void redo() override;
    void undo() override;

    static QString genDesc(bool is_add, int count);

private:
    Document *         m_doc;
    QVector<int>       m_positions;
    QVector<int>       m_sortedPositions;
    QVector<int>       m_filteredPositions;
    Document::ItemList m_items;
    Type               m_type;
};

class ChangeCmd : public QUndoCommand
{
public:
    ChangeCmd(Document *doc, const std::vector<std::pair<Document::Item *, Document::Item>> &changes,
              Document::Field hint = Document::Field(-1));
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    void updateText();

    Document *m_doc;
    uint m_loopCount;
    Document::Field m_hint;
    std::vector<std::pair<Document::Item *, Document::Item>> m_changes;

    static QTimer *s_eventLoopCounter;
};

class CurrencyCmd : public QUndoCommand
{
public:
    CurrencyCmd(Document *doc, const QString &ccode, qreal crate);
    ~CurrencyCmd() override;

    int id() const override;

    void redo() override;
    void undo() override;

private:
    Document * m_doc;
    QString    m_ccode;
    qreal      m_crate;
    double *   m_prices; // m_items.count() * 5 (price, origPrice, tierPrice * 3)
};

class ResetDifferenceModeCmd : public QUndoCommand
{
public:
    ResetDifferenceModeCmd(Document *doc);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    Document *m_doc;
    QHash<const BrickLink::InvItem *, BrickLink::InvItem> m_differenceBase;
};


class SortCmd : public QUndoCommand
{
public:
    SortCmd(Document *doc, int column, Qt::SortOrder order);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    Document *m_doc;
    QDateTime m_created;
    int m_column;
    Qt::SortOrder m_order;
    bool m_isSorted = false;

    QVector<BrickLink::InvItem *> m_unsorted;
};

class FilterCmd : public QUndoCommand
{
public:
    FilterCmd(Document *doc, const QString &filterString, const QVector<Filter> &filterList);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    Document *m_doc;
    QDateTime m_created;
    QString m_filterString;
    QVector<Filter> m_filterList;
    bool m_isFiltered = false;

    QVector<BrickLink::InvItem *> m_unfiltered;
};
