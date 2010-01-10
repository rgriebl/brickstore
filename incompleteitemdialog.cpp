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
#include <QStackedWidget>
#include <QDesktopServices>
#include <QUrl>

#include "bricklink.h"
#include "incompleteitemdialog.h"
#include "selectitemdialog.h"
#include "selectcolordialog.h"


IncompleteItemDialog::IncompleteItemDialog(BrickLink::InvItem *item, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f), m_item(item)
{
    setupUi(this);

    w_incomplete->setText(createDisplayString(m_item));

    bool itemok = (m_item->item());
    bool colorok = (m_item->color());

    w_item_stack->setShown(!itemok);

    // peeron links!

    if (!itemok) {
        w_item_stack->setCurrentIndex(0);
        w_item_add_info->setText(QString("<a href=\"%1\">%2</a>").arg(BrickLink::core()->url(BrickLink::URL_ItemChangeLog, m_item->isIncomplete()->m_item_id.toLatin1().constData()).toString()).
                                                                  arg(tr("BrickLink Item Change Log")));
    }

    w_color_stack->setShown(!colorok);

    if (!colorok) {
        w_color_stack->setCurrentIndex(0);
        w_color_add_info->setText(QString("<a href=\"%1\">%2</a>" ).arg(BrickLink::core()->url(BrickLink::URL_ColorChangeLog).toString()).
                                                                    arg(tr("BrickLink Color Change Log")));
    }

    connect(w_fix_color, SIGNAL(clicked()), this, SLOT(fixColor()));
    connect(w_fix_item, SIGNAL(clicked()), this, SLOT(fixItem()));

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    w_buttons->button(QDialogButtonBox::Cancel)->setText(tr("Delete"));
}

void IncompleteItemDialog::fixItem()
{
    const BrickLink::InvItem::Incomplete *inc = m_item->isIncomplete();
    const BrickLink::ItemType *itt = inc->m_itemtype_id.isEmpty() ? 0 : BrickLink::core()->itemType(inc->m_itemtype_id[0].toLatin1());
    const BrickLink::Category *cat = inc->m_category_id.isEmpty() ? 0 : BrickLink::core()->category(inc->m_category_id.toInt());

    SelectItemDialog dlg(false, this);

    dlg.setWindowTitle(tr("Fix Item"));
    //TODO: implement filter for SelectItem
    //dlg.setItemTypeCategoryAndFilter(itt, cat, !inc->m_item_name.isEmpty() ? inc->m_item_name : inc->m_item_id);

    if (dlg.exec() == QDialog::Accepted) {
        const BrickLink::Item *it = dlg.item();

        m_item->setItem(it);
        w_item_fixed->setText(tr("New Item is: %1, %2 %3" ).arg(it->itemType()->name()).
                                                            arg(it->id()).arg(it->name()));
        w_item_stack->setCurrentIndex(1);

        checkFixed();
    }
}

void IncompleteItemDialog::fixColor()
{
    SelectColorDialog dlg(this);

    dlg.setWindowTitle(tr("Fix Color"));

    if (dlg.exec() == QDialog::Accepted) {
        const BrickLink::Color *col = dlg.color();

        m_item->setColor(col);
        w_color_fixed->setText(tr("New color is: %1" ).arg(col->name()));
        w_color_stack->setCurrentIndex(1);

        checkFixed();
    }
}

void IncompleteItemDialog::checkFixed()
{
	bool ok = (m_item->item() && m_item->color());

	if (ok)
		m_item->setIncomplete(0);

    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

QString IncompleteItemDialog::createDisplayString(BrickLink::InvItem *ii)
{
    const BrickLink::InvItem::Incomplete *inc = ii->isIncomplete();

    QString str_qty, str_cat, str_typ, str_itm, str_col;

    // Quantity
    if (ii->quantity())
        str_qty = QString::number(ii->quantity ());

    // Item
    if (ii->item()) {
        str_itm = QString("%1 %2").arg(ii->item()->id()).arg(ii->item()->name());
    } else {
        str_itm = createDisplaySubString(tr("Item"), QString(), inc->m_item_id, inc->m_item_name);
    }

    // Itemtype
    if (ii->itemType()) {
        str_typ = ii->itemType()->name();
    } else {
        const BrickLink::ItemType *itt = inc->m_itemtype_id.isEmpty() ? 0 : BrickLink::core()->itemType(inc-> m_itemtype_id[0].toLatin1());
        str_typ = createDisplaySubString(tr("Type"), itt ? QString(itt->name()) : QString(), inc->m_itemtype_id, inc->m_itemtype_name);
    }

    // Category
    if (ii->category()) {
        str_cat = ii->category()->name();
    } else {
        const BrickLink::Category *cat = inc->m_category_id.isEmpty() ? 0 : BrickLink::core()->category(inc->m_category_id.toInt());
        str_cat = createDisplaySubString(tr("Category"), cat ? QString(cat->name()) : QString(), inc->m_category_id, inc->m_category_name);
    }

    // Color
    if (ii->color()) {
        str_col = ii->color()->name();
    } else {
        const BrickLink::Color *col = inc->m_color_id.isEmpty() ? 0 : BrickLink::core()->color(inc->m_color_id.toInt());
        str_col = createDisplaySubString(tr("Color"), col ? QString(col->name()) : QString(), inc->m_color_id, inc->m_color_name);
    }

    return QString("<b>%1</b> <b>%2</b>, <b>%3</b> (<b>%4</b>, <b>%5</b>)").arg(str_qty).arg(str_col).arg(str_itm).arg(str_typ).arg(str_cat);
}

QString IncompleteItemDialog::createDisplaySubString(const QString &what, const QString &realname, const QString &id, const QString &name)
{
    QString str;

    if (!realname.isEmpty()) {
        str = realname;
    } else {
        if (!name.isEmpty())
            str = name;

        if (!id.isEmpty()) {
            if (!str.isEmpty())
                str += " ";
            str += QString("[%1]").arg(id);
        }

        if (str.isEmpty())
            str = tr("Unknown");

        str.prepend(what + ": ");
    }
    return str;
}
