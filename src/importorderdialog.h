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

#include <QDialog>
#include <QDateTime>

#include "bricklinkfwd.h"
#include "ui_importorderdialog.h"

class Transfer;
class TransferJob;
class OrderModel;


class ImportOrderDialog : public QDialog, private Ui::ImportOrderDialog
{
    Q_OBJECT
public:
    ImportOrderDialog(QWidget *parent = nullptr);
    ~ImportOrderDialog() override;

    void updateOrders();

protected:
    virtual void changeEvent(QEvent *e) override;
    void languageChange();

protected slots:
    void checkSelected();
    void activateItem();
    void updateStatusLabel();

    void downloadFinished(TransferJob *job);
    void importOrders();
    void showOrdersOnBrickLink();

private:
    void orderDownloadFinished(BrickLink::Order *order, TransferJob *job,
                               const QByteArray &xml = { });

    Transfer *m_trans;
    QPushButton *w_import;
    QPushButton *w_showOnBrickLink;
    QDateTime m_lastUpdated;
    QVector<TransferJob *> m_currentUpdate;
    struct OrderDownload {
        OrderDownload() = delete;
        OrderDownload(BrickLink::Order *order, TransferJob *xmlJob, TransferJob *addressJob)
            : m_order(order), m_xmlJob(xmlJob), m_addressJob(addressJob)
        { }
        BrickLink::Order *m_order;
        TransferJob *m_xmlJob;
        TransferJob *m_addressJob;
        QByteArray m_xmlData;
    };
    QVector<OrderDownload> m_orderDownloads;
    OrderModel *m_orderModel;
};
