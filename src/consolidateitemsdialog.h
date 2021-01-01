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

#include "bricklinkfwd.h"
#include "ui_consolidateitemsdialog.h"

class QAbstractButton;
class Document;
class DocumentProxyModel;


class ConsolidateItemsDialog : public QDialog, private Ui::ConsolidateItemsDialog
{
    Q_OBJECT
public:
    ConsolidateItemsDialog(BrickLink::InvItem *existitem, BrickLink::InvItem *newitem, bool existing_attributes, QWidget *parent = nullptr);

    bool yesNoToAll() const;
    bool attributesFromExisting() const;
};
