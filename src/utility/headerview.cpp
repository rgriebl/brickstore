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
#include <QContextMenuEvent>
#include <QMenu>
#include <QLayout>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QLabel>
#include <QDialog>
#include <QHeaderView>
#include <QDebug>

#include "headerview.h"


class SectionItem : public QListWidgetItem
{
public:
    SectionItem() = default;
    ~SectionItem() override;

    int logicalIndex() const { return m_lidx; }
    void setLogicalIndex(int idx) { m_lidx = idx; }

private:
    int m_lidx = -1;
};

SectionItem::~SectionItem()
{ }

class SectionConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SectionConfigDialog(HeaderView *header)
        : QDialog(header), m_header(header)
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

        QVector<SectionItem *> v(m_header->count());
        for (int i = 0; i < m_header->count(); ++i) {
            SectionItem *it = nullptr;

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
        connect(m_buttons, &QDialogButtonBox::accepted, this, &SectionConfigDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        retranslateUi();
    }

    void retranslateUi()
    {
        setWindowTitle(tr("Configure Columns"));
        m_label->setText(tr("Drag the columns into the order you prefer and show/hide them using the check mark."));
    }

    void accept() override
    {
        for (int vi = 0; vi < m_list->count(); ++vi) {
            auto *si = static_cast<SectionItem *>(m_list->item(vi));
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

    void changeEvent(QEvent *e) override
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
        disconnect(oldm, horiz ? &QAbstractItemModel::columnsRemoved : &QAbstractItemModel::rowsRemoved,
                   this, &HeaderView::sectionsRemoved);
    }


    if (m) {
        connect(m, horiz ? &QAbstractItemModel::columnsRemoved : &QAbstractItemModel::rowsRemoved,
                this, &HeaderView::sectionsRemoved);
    }

    QHeaderView::setModel(m);

    m_unavailable.clear();
}

#define CONFIG_HEADER qint32(0x3b285931)

bool HeaderView::restoreLayout(const QByteArray &config)
{
    if (config.isEmpty())
        return false;

    QDataStream ds(config);
    qint32 magic = 0, version = 0, count = -1;

    ds >> magic >> version >> count;

    if ((ds.status() != QDataStream::Ok)
            || (magic != CONFIG_HEADER)
            || (version != 2)
            || (count != this->count())) {
        return false;
    }

    qint32 sortIndicator = -1;
    bool sortAscending = false;
    ds >> sortIndicator >> sortAscending;

    QVector<qint32> sizes;
    QVector<qint32> positions;
    QVector<bool> isHiddens;

    for (int i = 0; i < count; ++i) {
        qint32 size, position;
        bool isHidden;
        ds >> size >> position >> isHidden;

        sizes << size;
        positions << position;
        isHiddens << isHidden;
    }

    // sanity checks...
    if (ds.status() != QDataStream::Ok)
        return false;
    if ((sortIndicator < -1) || (sortIndicator >= count))
        return false;

    for (int i = 0; i < count; ++i) {
        if ((sizes.at(i) < 0)
                || (sizes.at(i) > 5000)
                || (positions.at(i) < 0)
                || (positions.at(i) >= count))
            return false;
    }

    // we need to move the columns into their (visual) place from left to right
    for (int vi = 0; vi < count; ++vi) {
        int li = positions.indexOf(vi);
        if (!isSectionInternal(li))
            setSectionHidden(li, isHiddens.at(li));
        moveSection(visualIndex(li), vi);
        resizeSection(li, sizes.at(li));
    }

    setSortIndicator(sortIndicator, sortAscending ? Qt::AscendingOrder : Qt::DescendingOrder);
    return true;
}

QByteArray HeaderView::saveLayout() const
{
    //TODO: we are missing the lastSortColumn[] history here, as this is kept in DocumentProxyModel

    QByteArray config;
    QDataStream ds(&config, QIODevice::WriteOnly);

    ds << CONFIG_HEADER << qint32(2) /*version*/
       << qint32(count())
       << qint32(sortIndicatorSection())
       << (sortIndicatorOrder() == Qt::AscendingOrder);

    for (int li = 0; li < count(); ++li ) {
        bool hidden = isSectionHidden(li);
        int size = hidden ? m_hiddenSizes.value(li) : sectionSize(li);

        ds << qint32(size)
           << qint32(visualIndex(li))
           << hidden;

//        qWarning("C%02d: @ %02d %c%c %d", li, visualIndex(li),
//                 hidden ? 'H' : ' ', isSectionAvailable(li) ? ' ' : 'X', size);
    }
    return config;
}



bool HeaderView::isSectionAvailable(int section) const
{
    return !m_unavailable.contains(section);
}

void HeaderView::setSectionAvailable(int section, bool avail)
{
    if (isSectionAvailable(section) == avail)
        return;

    if (!avail) {
        hideSection(section);
        m_unavailable.append(section);
    } else {
        m_unavailable.removeOne(section);
        showSection(section);
    }
}

bool HeaderView::isSectionInternal(int section) const
{
    return m_internal.contains(section);
}

void HeaderView::setSectionInternal(int section, bool internal)
{
    if (isSectionInternal(section) == internal)
        return;
    if (internal)
        m_internal.append(section);
    else
        m_internal.removeOne(section);
}

void HeaderView::setSectionHidden(int logicalIndex, bool hide)
{
    bool hidden = isSectionHidden(logicalIndex);

    if (hide && !hidden)
        m_hiddenSizes.insert(logicalIndex, sectionSize(logicalIndex));

    QHeaderView::setSectionHidden(logicalIndex, hide);

    if (!hide && hidden) {
        auto it = m_hiddenSizes.constFind(logicalIndex);
        if (it != m_hiddenSizes.constEnd()) {
            QHeaderView::resizeSection(logicalIndex, it.value());
            m_hiddenSizes.erase(it);
        }
    }
}

void HeaderView::resizeSection(int logicalIndex, int size)
{
    if (isSectionHidden(logicalIndex))
        m_hiddenSizes[logicalIndex] = size;
    else
        QHeaderView::resizeSection(logicalIndex, size);
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
    if (!isEnabled())
        return;

    QMenu m(this);

    m.addAction(tr("Configure columns..."))->setData(-1);
    m.addSeparator();

    QVector<QAction *> actions(count());
    for (int li = 0; li < count(); ++li) {
        QAction *a = nullptr;

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
        setSectionHidden(idx, !on);
    } else if (idx == -1) {
        SectionConfigDialog d(this);
        d.exec();
    } else {
        qWarning("HeaderView::menuActivated returned an invalid action");
    }
}

void HeaderView::sectionsRemoved(const QModelIndex &parent, int logicalFirst, int logicalLast)
{
    if (parent.isValid())
        return;

    for (int i = logicalFirst; i <= logicalLast; ++i) {
        m_unavailable.removeOne(i);
        m_internal.removeOne(i);
        m_hiddenSizes.remove(i);
    }
}

#include "headerview.moc"
#include "moc_headerview.cpp"
