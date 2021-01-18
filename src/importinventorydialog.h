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

#include "ui_importinventorydialog.h"


class ImportInventoryDialog : public QDialog, private Ui::ImportInventoryDialog
{
    Q_OBJECT
public:
    ImportInventoryDialog(QWidget *parent = nullptr);

    bool setItem(const BrickLink::Item *item);
    const BrickLink::Item *item() const;
    int quantity() const;
    BrickLink::Condition condition() const;

protected:
    void showEvent(QShowEvent *) override;
    QSize sizeHint() const override;

protected slots:
    void checkItem(const BrickLink::Item *it, bool ok);
};
