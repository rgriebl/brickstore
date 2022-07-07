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

#include "bricklink/global.h"
#include "common/document.h"
#include "ui_consolidateitemsdialog.h"


class View;

class ConsolidateItemsDialog : public QDialog, private Ui::ConsolidateItemsDialog
{
    Q_OBJECT

public:
    ConsolidateItemsDialog(const View *view, const LotList &lots,
                           int preselectedIndex, Document::LotConsolidation::Mode mode,
                           int current, int total, QWidget *parent = nullptr);

    int consolidateToIndex() const;
    bool repeatForAll() const;
    bool costQuantityAverage() const;
    Document::LotConsolidation::Mode consolidateRemaining() const;

protected:
    QSize sizeHint() const override;

private:
    bool m_forAll = false;
};
