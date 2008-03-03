/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#include "dselectcolor.h"
#include "cselectcolor.h"
#include "cutility.h"


DSelectColor::DSelectColor(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    connect(w_sc, SIGNAL(colorSelected(const BrickLink::Color *, bool)), this, SLOT(checkColor(const BrickLink::Color *, bool)));

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    setFocusProxy(w_sc);
}

void DSelectColor::setColor(const BrickLink::Color *col)
{
    w_sc->setCurrentColor(col);
}

const BrickLink::Color *DSelectColor::color() const
{
    return w_sc->currentColor();
}

void DSelectColor::checkColor(const BrickLink::Color *col, bool ok)
{
    QPushButton *p = w_buttons->button(QDialogButtonBox::Ok);
    p->setEnabled((col));

    if (col && ok)
        p->animateClick();
}

int DSelectColor::exec(const QRect &pos)
{
    if (pos.isValid())
        CUtility::setPopupPos(this, pos);

    return QDialog::exec();
}

void DSelectColor::showEvent(QShowEvent *)
{
    activateWindow();
    w_sc->setFocus();
}
