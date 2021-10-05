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

#include "bricklink/bricklinkfwd.h"
#include "view.h"
#include "ui_consolidateitemsdialog.h"


class ConsolidateItemsDialog : public QDialog, private Ui::ConsolidateItemsDialog
{
    Q_OBJECT

public:
    ConsolidateItemsDialog(const View *win, const LotList &lots,
                           int preselectedIndex, View::Consolidate mode, int current, int total,
                           QWidget *parent = nullptr);

    int consolidateToIndex() const;
    bool repeatForAll() const;
    bool costQuantityAverage() const;
    View::Consolidate consolidateRemaining() const;

protected:
    QSize sizeHint() const override;

private:
    bool m_forAll = false;
};
