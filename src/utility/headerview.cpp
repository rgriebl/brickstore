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
#include <QContextMenuEvent>
#include <QMenu>
#include <QLayout>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QLabel>
#include <QDialog>

#include "headerview.h"


class SectionConfigDialog : public QDialog {
    Q_OBJECT

    class SectionItem : public QListWidgetItem {
    public:
        SectionItem() : QListWidgetItem(), m_lidx(-1) { }

        int logicalIndex() const { return m_lidx; }
        void setLogicalIndex(int idx) { m_lidx = idx; }

    private:
        int m_lidx;
    };

public:
    SectionConfigDialog(HeaderView *header)
        : QDialog(header), m_header(header)
    {
        m_label = new QLabel(this);
        m_label->setWordWrap(true);
        m_list = new QListWidget(this);
        m_list->setAlternatingRowColors(true);
        m_list->setDragDropMode(QAbstractItemView::InternalMove);
        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

        QVBoxLayout *lay = new QVBoxLayout(this);
        lay->addWidget(m_label);
        lay->addWidget(m_list);
        lay->addWidget(m_buttons);

        QVector<SectionItem *> v(m_header->count());
        for (int i = 0; i < m_header->count(); ++i) {
            SectionItem *it = 0;

            if (m_header->isSectionAvailable(i)) {
                it = new SectionItem();
                it->setText(header->model()->headerData(i, m_header->orientation(), Qt::DisplayRole).toString());
                it->setCheckState(header->isSectionHidden(i) ? Qt::Unchecked : Qt::Checked);
                it->setLogicalIndex(i);
            }
            v[m_header->visualIndex(i)] = it;
        }
        foreach(SectionItem *si, v) {
            if (si)
                m_list->addItem(si);
        }
        connect(m_buttons, SIGNAL(accepted()), this, SLOT(accept()));
        connect(m_buttons, SIGNAL(rejected()), this, SLOT(reject()));

        retranslateUi();
    }

    void retranslateUi()
    {
        setWindowTitle(tr("Configure Columns"));
        m_label->setText(tr("Drag the columns into the order you prefer and show/hide them using the check mark."));
    }

    void accept()
    {
        for (int vi = 0; vi < m_list->count(); ++vi) {
            SectionItem *si = static_cast<SectionItem *>(m_list->item(vi));
            int li = si->logicalIndex();

            int oldvi = m_header->visualIndex(li);

            if (oldvi != vi)
                m_header->moveSection(oldvi, vi);

            bool oldvis = !m_header->isSectionHidden(li);
            bool vis = (si->checkState() == Qt::Checked);

            if (vis != oldvis)
                vis ? m_header->showSection(li) : m_header->hideSection(li);
        }

        QDialog::accept();
    }

    void changeEvent(QEvent *e)
    {
        if (e->type() == QEvent::LanguageChange)
            retranslateUi();
        QDialog::changeEvent(e);
    }

private:
    HeaderView *      m_header;
    QLabel *          m_label;
    QListWidget *     m_list;
    QDialogButtonBox *m_buttons;
};


///////////////////////////////////////////////////////////////////


HeaderView::HeaderView(Qt::Orientation o, QWidget *parent)
    : QHeaderView(o, parent)
{
}

void HeaderView::setModel(QAbstractItemModel *m)
{
    QAbstractItemModel *oldm = model();

    if (m == oldm)
        return;

    bool horiz = (orientation() == Qt::Horizontal);
    if (oldm) {
        disconnect(oldm, horiz ? SIGNAL(columnsRemoved(QModelIndex,int,int)) : SIGNAL(rowsRemoved(QModelIndex,int,int)),
                   this, SLOT(sectionsRemoved(QModelIndex,int,int)));
    }


    if (m) {
        connect(m, horiz ? SIGNAL(columnsRemoved(QModelIndex,int,int)) : SIGNAL(rowsRemoved(QModelIndex,int,int)),
                this, SLOT(sectionsRemoved(QModelIndex,int,int)));
    }

    QHeaderView::setModel(m);

    m_unavailable.clear();
}

void HeaderView::sectionsRemoved(const QModelIndex &parent, int logicalFirst, int logicalLast)
{
    if (parent.isValid())
        return;

    for (int i = logicalFirst; i <= logicalLast; ++i)
        m_unavailable.removeOne(i);
}

bool HeaderView::isSectionAvailable(int section) const
{
    return !m_unavailable.contains(section);
}

void HeaderView::setSectionAvailable(int section, bool avail)
{
    bool oldavail = isSectionAvailable(section);

    if (avail == oldavail)
        return;

    if (!avail) {
        hideSection(section);
        m_unavailable.append(section);
    } else {
        m_unavailable.removeOne(section);
    }
}

int HeaderView::availableSectionCount() const
{
    return count() - m_unavailable.count();
}

bool HeaderView::viewportEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::ContextMenu:
        showMenu(static_cast<QContextMenuEvent *>(e)->globalPos());
        e->accept();
        return true;
    default:
        break;
    }
    return QHeaderView::viewportEvent(e);
}

void HeaderView::showMenu(const QPoint &pos)
{
    QMenu m(this);

    m.addAction(tr("Configure columns..."))->setData(-1);
    m.addSeparator();

    QVector<QAction *> actions(count());
    for (int li = 0; li < count(); ++li) {
        QAction *a = 0;

        if (isSectionAvailable(li)) {
            a = new QAction(&m);
            a->setText(model()->headerData(li, Qt::Horizontal, Qt::DisplayRole).toString());
            a->setCheckable(true);
            a->setChecked(!isSectionHidden(li));
            a->setData(li);
        }
        actions[visualIndex(li)] = a;
    }
    foreach (QAction *a, actions) {
        if (a)
            m.addAction(a);
    }

    QAction *action = m.exec(pos);

    if (!action)
        return;
    int idx = action->data().toInt();
    bool on = action->isChecked();

    if (idx >= 0 && idx < count()) {
        on ? showSection(idx) : hideSection(idx);
    } else if (idx == -1) {
        SectionConfigDialog d(this);
        d.exec();
    } else {
        qWarning("HeaderView::menuActivated returned an invalid action");
    }
}

#include "headerview.moc"
