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
                 const Document::ItemList &items, bool merge_allowed = false);
    ~AddRemoveCmd() override;

    int id() const override;

    void redo() override;
    void undo() override;
    bool mergeWith(const QUndoCommand *other) override;

    static QString genDesc(bool is_add, int count);

private:
    Document *         m_doc;
    QVector<int>       m_positions;
    Document::ItemList m_items;
    Type               m_type;
    bool               m_merge_allowed;
};

class ChangeCmd : public QUndoCommand
{
public:
    ChangeCmd(Document *doc, int position, const Document::Item &item, bool merge_allowed = false);

    int id() const override;

    void redo() override;
    void undo() override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    Document *      m_doc;
    int             m_position;
    Document::Item  m_item;
    bool            m_merge_allowed;
};

class CurrencyCmd : public QUndoCommand
{
public:
    CurrencyCmd(Document *doc, const QString &ccode, qreal crate);
    ~CurrencyCmd() override;

    virtual int id() const override;

    void redo() override;
    void undo() override;

private:
    Document * m_doc;
    QString    m_ccode;
    qreal      m_crate;
    double *   m_prices; // m_items.count() * 5 (price, origPrice, tierPrice * 3)
};
