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
#include <QApplication>
#include <QClipboard>
#include <QListWidget>
#include <QRadioButton>
#include <QGridLayout>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QBoxLayout>

#include <QGroupBox>
#include <QCheckBox>
#include <QButtonGroup>

#include "document.h"
#include "selectdocumentdialog.h"


SelectDocument::SelectDocument(const Document *self, QWidget *parent)
    : QWidget(parent)
{
    m_clipboard = new QRadioButton(tr("Items from Clipboard"));
    m_document = new QRadioButton(tr("Items from an already open document:"));
    m_documentList = new QListWidget();

    auto layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setColumnMinimumWidth(0, 20);
    layout->setColumnStretch(1, 100);
    layout->setRowStretch(2, 100);
    layout->addWidget(m_clipboard, 0, 0, 1, 2);
    layout->addWidget(m_document, 1, 0, 1, 2);
    layout->addWidget(m_documentList, 2, 1, 1, 1);

    m_itemsFromClipboard = BrickLink::InvItemMimeData::items(QApplication::clipboard()->mimeData());

    foreach (const Document *doc, Document::allDocuments()) {
        if (doc != self) {
            QListWidgetItem *item = new QListWidgetItem(doc->fileNameOrTitle(), m_documentList);
            item->setData(Qt::UserRole, QVariant::fromValue(doc));
        }
    }

    bool hasClip = !m_itemsFromClipboard.isEmpty();
    bool hasDocs = m_documentList->count() > 0;

    m_clipboard->setEnabled(hasClip);
    m_document->setEnabled(hasDocs);
    m_documentList->setEnabled(hasDocs);

    if (hasDocs) {
        m_document->setChecked(true);
        m_documentList->setCurrentRow(0);
    } else if (hasClip) {
        m_clipboard->setChecked(true);
    }

    auto emitSelected = [this]() { emit documentSelected(isDocumentSelected()); };

    connect(m_clipboard, &QAbstractButton::toggled,
            this, [this, emitSelected]() { emitSelected(); });
    connect(m_documentList, &QListWidget::itemSelectionChanged,
            this, [this, emitSelected]() { emitSelected(); });

    QMetaObject::invokeMethod(this, emitSelected, Qt::QueuedConnection);
}

BrickLink::InvItemList SelectDocument::items() const
{
    BrickLink::InvItemList list;

    if (m_clipboard->isChecked()) {
        for (const BrickLink::InvItem *item : m_itemsFromClipboard)
            list.append(new BrickLink::InvItem(*item));
    } else {
        if (!m_documentList->selectedItems().isEmpty()) {
            const auto *doc = m_documentList->selectedItems().constFirst()
                    ->data(Qt::UserRole).value<const Document *>();
            if (doc) {
                const auto items = doc->items();
                for (const Document::Item *item : items)
                    list.append(new BrickLink::InvItem(*item));
            }
        }
    }
    return list;
}

SelectDocument::~SelectDocument()
{
    qDeleteAll(m_itemsFromClipboard);
}

bool SelectDocument::isDocumentSelected() const
{
    return m_clipboard->isChecked() || !m_documentList->selectedItems().isEmpty();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SelectDocumentDialog::SelectDocumentDialog(const Document *self, const QString &headertext,
                                           QWidget *parent)
    : QDialog(parent)
{
    m_sd = new SelectDocument(self);

    auto label = new QLabel(headertext);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_sd);
    layout->addWidget(buttons);

    connect(m_sd, &SelectDocument::documentSelected,
            m_ok, &QAbstractButton::setEnabled);
}

BrickLink::InvItemList SelectDocumentDialog::items() const
{
    return m_sd->items();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


SelectCopyFieldsDialog::SelectCopyFieldsDialog(const Document *self, const QString &headertext,
                                               const QVector<QPair<QString, bool>> &fields,
                                               QWidget *parent)
    : QDialog(parent)
{
    m_sd = new SelectDocument(self);

    auto label = new QLabel(headertext);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto updateButtons = [this]() {
        m_ok->setEnabled(!m_selectedFields.isEmpty() && m_sd->isDocumentSelected());
    };

    connect(m_sd, &SelectDocument::documentSelected,
            this, updateButtons);

    auto box = new QGroupBox(tr("Fields"));
    auto boxgrid = new QGridLayout(box);

    for (int i = 0; i < fields.size(); ++i) {
        const auto &f = fields.at(i);
        auto check = new QCheckBox(f.first);
        check->setChecked(f.second);
        boxgrid->addWidget(check, i / 3, i % 3);
        if (f.second)
            m_selectedFields << i;
        connect (check, &QCheckBox::toggled,
                 this, [this, i, updateButtons](bool b) {
            if (b && !m_selectedFields.contains(i))
                m_selectedFields.append(i);
            else
                m_selectedFields.removeAll(i);
            updateButtons();
        });
    }

    auto layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_sd);
    layout->addWidget(box);
    layout->addWidget(buttons);
}

BrickLink::InvItemList SelectCopyFieldsDialog::items() const
{
    return m_sd->items();
}

const QVector<int> &SelectCopyFieldsDialog::selectedFields() const
{
    return m_selectedFields;
}

#include "moc_selectdocumentdialog.cpp"

