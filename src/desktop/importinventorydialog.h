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

#include <QDialog>
#include "bricklink/global.h"

QT_FORWARD_DECLARE_CLASS(QStringListModel)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QDialogButtonBox)
QT_FORWARD_DECLARE_CLASS(QLabel)

class SelectItem;
class ImportInventoryWidget;


class ImportInventoryDialog : public QDialog
{
    Q_OBJECT
public:
    ImportInventoryDialog(const BrickLink::Item *item, int quantity,
                          BrickLink::Condition condition, QWidget *parent = nullptr);
    ImportInventoryDialog(QWidget *parent = nullptr);
    ~ImportInventoryDialog() override;

    bool setItem(const BrickLink::Item *item);
    const BrickLink::Item *item() const;
    int quantity() const;
    BrickLink::Condition condition() const;
    BrickLink::Status extraParts() const;
    BrickLink::PartOutTraits partOutTraits() const;

protected:
    void changeEvent(QEvent *e) override;
    void showEvent(QShowEvent *) override;
    void keyPressEvent(QKeyEvent *e) override;
    QSize sizeHint() const override;

    void languageChange();

protected slots:
    void checkItem(const BrickLink::Item *it, bool ok);
    void importInventory();

private:
    const BrickLink::Item *m_verifyItem = nullptr;
    SelectItem *m_select = nullptr;
    QLabel *m_verifyLabel = nullptr;
    ImportInventoryWidget *m_import;
    QDialogButtonBox *m_buttons;
    QStringListModel *m_favoriteFilters;
};
