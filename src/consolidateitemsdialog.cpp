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
#include <algorithm>

#include <QPushButton>
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QScreen>
#include <QWindow>

#include "bricklink.h"
#include "consolidateitemsdialog.h"
#include "headerview.h"
#include "window.h"
#include "document.h"
#include "documentdelegate.h"
#include "config.h"


ConsolidateItemsDialog::ConsolidateItemsDialog(const Window *win,
                                               const BrickLink::InvItemList &items,
                                               int preselectedIndex, Window::Consolidate mode,
                                               int current, int total, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    if (total > 1)
        w_counter->setText(QString::fromLatin1("%1/%2").arg(current).arg(total));
    else
        w_counter->hide();

    bool newItems = (items.count() == 2) && win->document()->items().contains(items.at(0))
                     && !win->document()->items().contains(items.at(items.count() - 1));

    Q_ASSERT(newItems == (int(mode) >= int(Window::Consolidate::IntoExisting)));

    for (int i = int(Window::Consolidate::IntoNew); i > int(Window::Consolidate::Not); --i) {
        w_prefer_remaining->setItemData(i, QVariant::fromValue(static_cast<Window::Consolidate>(i)));

        bool forNewOnly = (i >= int(Window::Consolidate::IntoExisting));
        if (newItems != forNewOnly)
            w_prefer_remaining->removeItem(i);
    }

    QVector<int> fakeIndexes;
    for (int i = 0; i < items.size(); ++i)
        fakeIndexes << win->document()->index(items.at(i)).row();

    Document *doc = Document::createTemporary(items, fakeIndexes);
    doc->setParent(this);

    auto *headerView = new HeaderView(Qt::Horizontal, w_list);
    w_list->setHorizontalHeader(headerView);

    w_list->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    w_list->setContextMenuPolicy(Qt::NoContextMenu);

    setFocusProxy(w_list);

    auto *view = new DocumentProxyModel(doc, this);
    w_list->setModel(view);

    auto *dd = new DocumentDelegate(doc, view, w_list);
    dd->setReadOnly(true);
    w_list->setItemDelegate(dd);
    w_list->verticalHeader()->setDefaultSectionSize(dd->defaultItemHeight(w_list));
    w_list->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

    headerView->restoreLayout(win->currentColumnLayout());

    w_list->setMinimumHeight(8 + w_list->frameWidth() * 2 +
                             w_list->horizontalHeader()->height() +
                             w_list->verticalHeader()->length() +
                             w_list->style()->pixelMetric(QStyle::PM_ScrollBarExtent));

    w_list->selectionModel()->setCurrentIndex(view->index(preselectedIndex, 0),
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

    QMetaObject::invokeMethod(this, [this]() { resize(sizeHint()); }, Qt::QueuedConnection);
}

Window::Consolidate ConsolidateItemsDialog::consolidateRemaining() const
{
    if (m_forAll)
        return static_cast<Window::Consolidate>(w_prefer_remaining->currentIndex());
    else
        return Window::Consolidate::Not;
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
    // try to fit as much information on the screen as possible
    auto s = QDialog::sizeHint();
    auto w = w_list->viewport()->width() + w_list->horizontalScrollBar()->maximum()
            - w_list->horizontalScrollBar()->minimum() + 50;
    if (windowHandle() && windowHandle()->screen())
        s.rwidth() = qMin(w, windowHandle()->screen()->availableSize().width() * 7 / 8);
    return s;
}

#include "moc_consolidateitemsdialog.cpp"
