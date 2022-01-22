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

#include <QLabel>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QBoxLayout>
#include <QEvent>

#include "common/config.h"
#include "managecolumnlayoutsdialog.h"


class ColumnLayoutItem : public QListWidgetItem
{
public:
    ColumnLayoutItem() = default;
    ~ColumnLayoutItem() override;

    QString m_id;
    QString m_name;
};

ColumnLayoutItem::~ColumnLayoutItem()
{ }



ManageColumnLayoutsDialog::ManageColumnLayoutsDialog(QWidget *parent)
    : QDialog(parent)
{
    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(m_label);
    lay->addWidget(m_list);
    lay->addWidget(m_buttons);

    auto ids = Config::inst()->columnLayoutIds();

    QMap<int, QString> pos;
    for (const auto &id : ids)
        pos.insert(Config::inst()->columnLayoutOrder(id), id);

    for (auto &id : qAsConst(pos)) {
        auto cli = new ColumnLayoutItem();

        cli->m_id = id;
        cli->m_name = Config::inst()->columnLayoutName(id);
        cli->setText(cli->m_name);
        cli->setCheckState(Qt::Checked);
        cli->setFlags(cli->flags() | Qt::ItemIsEditable);

        m_list->addItem(cli);
    }
    connect(m_buttons, &QDialogButtonBox::accepted, this, &ManageColumnLayoutsDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    retranslateUi();
}

void ManageColumnLayoutsDialog::retranslateUi()
{
    setWindowTitle(tr("Manage Column Layouts"));
    m_label->setText(tr("Drag the column layouts into the order you prefer, double-click to rename them and delete them by removing the check mark."));
}

void ManageColumnLayoutsDialog::accept()
{
    QStringList newOrder;

    for (int i = 0; i < m_list->count(); ++i) {
        auto *cli = static_cast<ColumnLayoutItem *>(m_list->item(i));

        auto id = cli->m_id;
        bool remove = (cli->checkState() == Qt::Unchecked);

        if (remove) {
            Config::inst()->deleteColumnLayout(id);
        } else {
            newOrder.append(id);
            if (cli->text() != cli->m_name)
                Config::inst()->renameColumnLayout(id, cli->text());
        }
    }
    Config::inst()->reorderColumnLayouts(newOrder);

    QDialog::accept();
}

void ManageColumnLayoutsDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(e);
}

#include "moc_managecolumnlayoutsdialog.cpp"
