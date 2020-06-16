/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QPushButton>

#include "selectcolordialog.h"
#include "selectcolor.h"
#include "utility.h"
#include "bricklink.h"


SelectColorDialog::SelectColorDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    connect(w_sc, &SelectColor::colorSelected,
            this, &SelectColorDialog::checkColor);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    setFocusProxy(w_sc);
}

void SelectColorDialog::setColor(const BrickLink::Color *col)
{
    w_sc->setCurrentColor(col);
}

const BrickLink::Color *SelectColorDialog::color() const
{
    return w_sc->currentColor();
}

void SelectColorDialog::checkColor(const BrickLink::Color *col, bool ok)
{
    QPushButton *p = w_buttons->button(QDialogButtonBox::Ok);
    p->setEnabled((col));

    if (col && ok)
        p->animateClick();
}

int SelectColorDialog::execAtPosition(const QRect &pos)
{
    if (pos.isValid())
        Utility::setPopupPos(this, pos);

    return QDialog::exec();
}

void SelectColorDialog::showEvent(QShowEvent *)
{
    activateWindow();
    w_sc->setFocus();
}
