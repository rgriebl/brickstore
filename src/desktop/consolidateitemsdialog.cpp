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
#include <algorithm>

#include <QPushButton>
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QScreen>
#include <QWindow>

#include "common/documentmodel.h"
#include "consolidateitemsdialog.h"
#include "documentdelegate.h"
#include "headerview.h"
#include "view.h"


ConsolidateItemsDialog::ConsolidateItemsDialog(const View *view, const LotList &lots,
                                               int preselectedIndex, Document::LotConsolidation::Mode mode,
                                               int current, int total, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    if (total > 1)
        w_counter->setText(QString::fromLatin1("%1/%2").arg(current).arg(total));
    else
        w_counter->hide();

    bool newLots = (lots.count() == 2) && view->model()->lots().contains(lots.at(0))
            && !view->model()->lots().contains(lots.at(lots.count() - 1));

    Q_ASSERT(newLots == (int(mode) >= int(Document::LotConsolidation::Mode::IntoExisting)));

    for (int i = int(Document::LotConsolidation::Mode::IntoNew); i > int(Document::LotConsolidation::Mode::Not); --i) {
        w_prefer_remaining->setItemData(i, QVariant::fromValue(static_cast<Document::LotConsolidation::Mode>(i)));

        bool forNewOnly = (i >= int(Document::LotConsolidation::Mode::IntoExisting));
        if (newLots != forNewOnly)
            w_prefer_remaining->removeItem(i);
    }

    QVector<int> fakeIndexes;
    for (int i = 0; i < lots.size(); ++i)
        fakeIndexes << int(view->model()->lots().indexOf(lots.at(i)));

    DocumentModel *docModel = DocumentModel::createTemporary(lots, fakeIndexes);
    docModel->setParent(this);

    auto *headerView = new HeaderView(Qt::Horizontal, w_list);
    w_list->setHorizontalHeader(headerView);

    w_list->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    w_list->setContextMenuPolicy(Qt::NoContextMenu);

    setFocusProxy(w_list);

    w_list->setModel(docModel);

    auto *dd = new DocumentDelegate(w_list);
    dd->setReadOnly(true);
    w_list->setItemDelegate(dd);
    w_list->verticalHeader()->setDefaultSectionSize(dd->defaultItemHeight(w_list));
    w_list->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    headerView->restoreState(view->headerView()->saveState());
    headerView->showSection(DocumentModel::Index);
    if (headerView->visualIndex(DocumentModel::Index) != 0)
        headerView->moveSection(headerView->visualIndex(DocumentModel::Index), 0);

    w_list->selectionModel()->setCurrentIndex(docModel->index(preselectedIndex, 0),
                                              QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);

    if (current == total) {
        // if we just hid the buttons on the last of a series, the "Yes" button would
        // suddenly be in the same position where the "No" button was before...
        if (total == 1) {
            w_buttons->button(QDialogButtonBox::YesToAll)->hide();
            w_buttons->button(QDialogButtonBox::NoToAll)->hide();
        } else {
            w_buttons->button(QDialogButtonBox::YesToAll)->setDisabled(true);
            w_buttons->button(QDialogButtonBox::NoToAll)->setDisabled(true);
        }
        w_prefer_remaining->hide();
        w_label_remaining->hide();
    }

    connect(w_buttons, &QDialogButtonBox::clicked, this, [this](QAbstractButton *b) {
        switch (w_buttons->standardButton(b)) {
        case QDialogButtonBox::YesToAll:
        case QDialogButtonBox::NoToAll:
            m_forAll = true;
            break;
        default:
            break;
        }
    });

    QMetaObject::invokeMethod(this, [this]() {
        QSize sh = sizeHint();
        QRect screenRect = windowHandle()->screen()->availableGeometry();
        QSize maxSize = screenRect.size() * 7 / 8;

        if (sh.width() > maxSize.width())
            sh.setWidth(maxSize.width());
        if (sh.height() > maxSize.height())
            sh.setHeight(maxSize.height());

        int x = screenRect.x() + (screenRect.width() - sh.width()) / 2;
        int y = screenRect.y() + (screenRect.height() - sh.height()) / 2;
        move(x, y);
        resize(sh);
    }, Qt::QueuedConnection);
}

Document::LotConsolidation::Mode ConsolidateItemsDialog::consolidateRemaining() const
{
    if (m_forAll)
        return w_prefer_remaining->currentData().value<Document::LotConsolidation::Mode>();
    else
        return Document::LotConsolidation::Mode::Not;
}

int ConsolidateItemsDialog::consolidateToIndex() const
{
    return w_list->selectionModel()->currentIndex().row();
}

bool ConsolidateItemsDialog::repeatForAll() const
{
    return m_forAll;
}

bool ConsolidateItemsDialog::costQuantityAverage() const
{
    return w_qty_avg_cost->isChecked();
}

QSize ConsolidateItemsDialog::sizeHint() const
{
    // Try to fit as much information on the screen as possible
    // Ideally this should be the sizeHint() of the tableview, but we would need to derive from
    // QTableView and register this class in the designer to make it work.
    // This is ugly, but less hassle:

    auto sh = QDialog::sizeHint();

    int listWidth = w_list->horizontalHeader()->length()
            + w_list->verticalScrollBar()->sizeHint().width()
            + w_list->frameWidth() * 2;
    int listHeight = w_list->verticalHeader()->length()
            + w_list->horizontalHeader()->sizeHint().height()
            + w_list->horizontalScrollBar()->sizeHint().height()
            + w_list->frameWidth() * 2;

    sh.setWidth(qMax(sh.width(), listWidth + layout()->contentsMargins().left() + layout()->contentsMargins().right()));
    sh.setHeight(qMax(sh.height(), listHeight + sh.height() - w_list->sizeHint().height()));
    return sh;
}

#include "moc_consolidateitemsdialog.cpp"
