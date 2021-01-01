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

#include <QDate>
#include <QUrl>
#include <QObject>

#include "bricklinkfwd.h"

class ProgressDialog;


class ImportBLStore : public QObject
{
    Q_OBJECT
public:
    ImportBLStore(ProgressDialog *pd);
    ~ImportBLStore();

    const BrickLink::InvItemList &items() const;
    QString currencyCode() const;

private slots:
    virtual void gotten();

private:
    ProgressDialog *m_progress;
    BrickLink::InvItemList m_items;
    QString m_currencycode;
};


class ImportBLOrder : public QObject
{
    Q_OBJECT
public:
    ImportBLOrder(const QDate &from, const QDate &to, BrickLink::OrderType type, ProgressDialog *pd);
    ImportBLOrder(const QString &order, BrickLink::OrderType type, ProgressDialog *pd);

    const QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > &orders() const;

private slots:
    virtual void gotten();

private:
    void init();
    static QDate ymd2date(const QString &ymd);

private:
    ProgressDialog *       m_progress;
    QString                m_order_id;
    QDate                  m_order_from;
    QDate                  m_order_to;
    BrickLink::OrderType   m_order_type;
    QUrl                   m_url;
    bool                   m_retry_placed;
    int                    m_current_address;
    QList<QPair<BrickLink::Order *, BrickLink::InvItemList *> > m_orders;
};


class ImportBLCart : public QObject
{
    Q_OBJECT
public:
    ImportBLCart(int shopid, int cartid, ProgressDialog *pd);
    ~ImportBLCart() override;

    const BrickLink::InvItemList &items() const;
    QString currencyCode() const;

private slots:
    virtual void gotten();

private:
    ProgressDialog *       m_progress;
    BrickLink::InvItemList m_items;
    QString                m_currencycode;
};
