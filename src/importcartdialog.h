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
#include "ui_importcartdialog.h"

class Transfer;
class TransferJob;
class CartModel;


class ImportCartDialog : public QDialog, private Ui::ImportCartDialog
{
    Q_OBJECT
public:
    ImportCartDialog(QWidget *parent = nullptr);
    ~ImportCartDialog() override;

    void updateCarts();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    virtual void changeEvent(QEvent *e) override;
    void languageChange();

protected slots:
    void checkSelected();
    void updateStatusLabel();

    void downloadFinished(TransferJob *job);
    void importCarts(const QModelIndexList &rows);
    void showCartsOnBrickLink();

private:
    QPushButton *w_import;
    QPushButton *w_showOnBrickLink;
    QDateTime m_lastUpdated;
    QVector<TransferJob *> m_currentUpdate;
    QVector<TransferJob *> m_cartDownloads;
    CartModel *m_cartModel;
};
