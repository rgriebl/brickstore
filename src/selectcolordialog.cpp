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
#include <QPushButton>

#include "selectcolordialog.h"
#include "selectcolor.h"
#include "utility.h"
#include "bricklink.h"


SelectColorDialog::SelectColorDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    w_sc->restoreState(SelectColor::defaultState());

    connect(w_sc, &SelectColor::colorSelected,
            this, &SelectColorDialog::checkColor);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    setFocusProxy(w_sc);
}

void SelectColorDialog::setColor(const BrickLink::Color *color)
{
    w_sc->setCurrentColor(color);
}

void SelectColorDialog::setColorAndItem(const BrickLink::Color *color, const BrickLink::Item *item)
{
    w_sc->setCurrentColorAndItem(color, item);
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
    setWindowFlags(Qt::Tool | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

    m_pos = pos; // we need to delay the positioning, because X11 doesn't know the frame size yet
    return QDialog::exec();
}

#if defined(Q_OS_LINUX)

void SelectColorDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::WindowStateChange) {
        if (windowState() & Qt::WindowMaximized)
            QMetaObject::invokeMethod(this, [this]() {
                setWindowState(Qt::WindowNoState | Qt::WindowActive);
                Utility::setPopupPos(this, m_pos);
            }, Qt::QueuedConnection);
    }
    return QDialog::changeEvent(e);
}
#elif defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)

bool SelectColorDialog::event(QEvent *e)
{
    if (e->type() == QEvent::NonClientAreaMouseButtonDblClick) {
        if (m_pos.isValid()) {
            QMetaObject::invokeMethod(this, [this]() {
                Utility::setPopupPos(this, m_pos);
            }, Qt::QueuedConnection);
            return true;
        }
    }
    return QDialog::event(e);
}
#endif

void SelectColorDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);

    activateWindow();
    w_sc->setFocus();

    if (m_pos.isValid()) {
        QMetaObject::invokeMethod(this, [this]() {
            Utility::setPopupPos(this, m_pos);
        }, Qt::QueuedConnection);
    }
}

#include "moc_selectcolordialog.cpp"
