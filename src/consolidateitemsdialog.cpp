/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#include <QTableView>
#include <QHeaderView>

#include "bricklink.h"
#include "consolidateitemsdialog.h"
#include "document.h"
#include "documentdelegate.h"


ConsolidateItemsDialog::ConsolidateItemsDialog(BrickLink::InvItem *existitem, BrickLink::InvItem *newitem, bool existing_attributes, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    BrickLink::InvItemList list;
    list << existitem << newitem;
    Document *doc = Document::createTemporary(list);

    doc->setParent(this);
    w_list->horizontalHeader()->setSectionsMovable(true);
    w_list->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    w_list->setContextMenuPolicy(Qt::NoContextMenu);
    setFocusProxy(w_list);

    DocumentProxyModel *view = new DocumentProxyModel(doc);
    w_list->setModel(view);

    DocumentDelegate *dd = new DocumentDelegate(doc, view, w_list);
    dd->setReadOnly(true);
    w_list->setItemDelegate(dd);
    w_list->verticalHeader()->setDefaultSectionSize(dd->defaultItemHeight(w_list));

    w_list->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    w_list->setMinimumHeight(8 + w_list->frameWidth() * 2 +
                             w_list->horizontalHeader()->height() +
                             w_list->verticalHeader()->length() +
                             w_list->style()->pixelMetric(QStyle::PM_ScrollBarExtent));

    w_list->selectionModel()->setCurrentIndex(view->index(existing_attributes ? 0 : 1, 0), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

bool ConsolidateItemsDialog::yesNoToAll() const
{
    return w_for_all->isChecked();
}

bool ConsolidateItemsDialog::attributesFromExisting() const
{
    return w_list->selectionModel()->isRowSelected(0, QModelIndex());
}
