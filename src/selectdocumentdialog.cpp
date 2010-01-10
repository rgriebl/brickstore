/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#include <QListWidget>
#include <QClipboard>

#include "document.h"
#include "selectdocumentdialog.h"

Q_DECLARE_METATYPE(const Document *)


SelectDocumentDialog::SelectDocumentDialog(const Document *self, const QString &headertext, QWidget *parent, Qt::WindowFlags fl)
    : QDialog(parent, fl)
{
    setupUi(this);
    
    w_header->setText(headertext);

    m_clipboard_list = BrickLink::InvItemMimeData::items(QApplication::clipboard()->mimeData());
    
    w_document->setChecked(m_clipboard_list.isEmpty());
    w_clipboard->setEnabled(!m_clipboard_list.isEmpty());

    foreach (const Document *doc, Document::allDocuments()) {
        if (doc != self) {
            QListWidgetItem *item = new QListWidgetItem(doc->title(), w_document_list);
            item->setData(Qt::UserRole, doc);
        }
    }
    if (w_document_list->count())
        w_document_list->setCurrentRow(0);
    
    connect(w_clipboard, SIGNAL(toggled(bool)), this, SLOT(updateButtons()));
    connect(w_document_list, SIGNAL(currentItemChanged(QListWidgetItem *. QListWidgetItem *)), this, SLOT(updateButtons()));
    connect(w_document_list, SIGNAL(itemActivated(QListWidgetItem *)), this, SLOT(itemActivated(QListWidgetItem *)));

    updateButtons();
}

SelectDocumentDialog::~SelectDocumentDialog()
{
    qDeleteAll(m_clipboard_list);
}

void SelectDocumentDialog::updateButtons()
{
    bool b = w_clipboard->isChecked() || w_document_list->currentItem();
    setEnabled(b);
}

void SelectDocumentDialog::itemActivated(QListWidgetItem *)
{
    w_buttons->button(QDialogButtonBox::Ok)->animateClick();
}

BrickLink::InvItemList SelectDocumentDialog::items() const
{
    BrickLink::InvItemList list;

    if (w_clipboard->isChecked()) {
        foreach (BrickLink::InvItem *item, m_clipboard_list)
            list.append(new BrickLink::InvItem(*item));
    } else {
        if (w_document_list->currentItem()) {
            const Document *doc = w_document_list->currentItem()->data(Qt::UserRole).value<const Document *>();
            
            if (doc) {
                foreach (Document::Item *item, doc->items())
                    list.append(new BrickLink::InvItem(*item));
            }
        }
    }
    return list;
}
