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
                 const QVector<int> &viewPositions, const Document::ItemList &items);
    ~AddRemoveCmd() override;
    int id() const override;

    void redo() override;
    void undo() override;

    static QString genDesc(bool is_add, int count);

private:
    Document *         m_doc;
    QVector<int>       m_positions;
    QVector<int>       m_viewPositions;
    Document::ItemList m_items;
    Type               m_type;
};

class ChangeCmd : public QUndoCommand
{
public:
    ChangeCmd(Document *doc, Document::Item *item, const Document::Item &value);
    int id() const override;

    void redo() override;
    void undo() override;

private:
    Document *      m_doc;
    Document::Item *m_item;
    Document::Item  m_value;
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

class DifferenceModeCmd : public QUndoCommand
{
public:
    DifferenceModeCmd(Document *doc, bool active);
    int id() const override;

    void redo() override;
    void undo() override;

private:
    Document *m_doc;
    bool m_active;
};


class SortFilterCmd : public QUndoCommand
{
public:
    SortFilterCmd(Document *doc, int column, Qt::SortOrder order,
                  const QString &filterString, const QVector<Filter> &filterList);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    Document *m_doc;
    QDateTime m_created;
    int m_column;
    Qt::SortOrder m_order;
    QString m_filterString;
    QVector<Filter> m_filterList;

    QVector<BrickLink::InvItem *> m_unsorted;
};
