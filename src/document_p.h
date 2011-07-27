/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef __DOCUMENT_P_H__
#define __DOCUMENT_P_H__

#include <QUndoCommand>
#include "document.h"


class AddRemoveCmd : public QUndoCommand {
public:
    enum Type { Add, Remove };

    AddRemoveCmd(Type t, Document *doc, const QVector<int> &positions, const Document::ItemList &items, bool merge_allowed = false);
    ~AddRemoveCmd();

    int id() const;

    void redo();
    void undo();
    bool mergeWith(const QUndoCommand *other);

    static QString genDesc(bool is_add, uint count);

private:
    Document *         m_doc;
    QVector<int>       m_positions;
    Document::ItemList m_items;
    Type               m_type;
    bool               m_merge_allowed;
};

class ChangeCmd : public QUndoCommand {
public:
    ChangeCmd(Document *doc, int position, const Document::Item &item, bool merge_allowed = false);
    ~ChangeCmd();

    int id() const;

    void redo();
    void undo();
    bool mergeWith(const QUndoCommand *other);

private:
    Document *      m_doc;
    int             m_position;
    Document::Item  m_item;
    bool            m_merge_allowed;
};

class CurrencyCmd : public QUndoCommand {
public:
    CurrencyCmd(Document *doc, const QString &ccode, qreal crate);
    ~CurrencyCmd();

    virtual int id() const;

    void redo();
    void undo();

private:
    Document * m_doc;
    QString    m_ccode;
    qreal      m_crate;
    double *   m_prices; // m_items.count() * 5 (price, origPrice, tierPrice * 3)
};

#endif
