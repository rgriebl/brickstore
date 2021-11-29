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
#include <QHelpEvent>
#include <QMenu>
#include <QLayout>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QLabel>
#include <QDialog>
#include <QHeaderView>
#include <QDebug>
#include <QProxyStyle>
#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QToolTip>
#include <QPixmapCache>
#include <QStyleFactory>

#include "utility.h"
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


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


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

            it = new SectionItem();
            it->setText(header->model()->headerData(i, m_header->orientation(), Qt::DisplayRole).toString());
            it->setCheckState(header->isSectionHidden(i) ? Qt::Unchecked : Qt::Checked);
            it->setLogicalIndex(i);

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


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


HeaderView::HeaderView(Qt::Orientation o, QWidget *parent)
    : QHeaderView(o, parent)
{
    setProperty("multipleSortColumns", true);

    connect(this, &QHeaderView::sectionClicked,
            this, [this](int section) {
        auto firstSC = m_sortColumns.isEmpty() ? qMakePair(-1, Qt::AscendingOrder)
                                               : m_sortColumns.constFirst();
        if (!m_sortColumns.isEmpty() && !m_isSorted && (section == firstSC.first)) {
            m_isSorted = true;
            update();
            emit isSortedChanged(m_isSorted);
        } else {
            if (qApp->keyboardModifiers() == Qt::ShiftModifier) {
                bool found = false;
                for (int i = 0; i < m_sortColumns.size(); ++i) {
                    if (m_sortColumns.at(i).first == section) {
                        m_sortColumns[i].second = Qt::SortOrder(1 - m_sortColumns.at(i).second);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    m_sortColumns.append(qMakePair(section, firstSC.second));
            } else {
                if (firstSC.first == section)
                    m_sortColumns = { qMakePair(section, Qt::SortOrder(1 - firstSC.second)) };
                else
                    m_sortColumns = { qMakePair(section, Qt::AscendingOrder) };
            }
            if (!m_isSorted) {
                m_isSorted = true;
                emit isSortedChanged(m_isSorted);
            }
            emit sortColumnsChanged(m_sortColumns);
        }
    });

    connect(this, &QHeaderView::sectionMoved,
            this, [this](int li, int oldVi, int newVi) {
        if (!isSectionHidden(li) && (oldVi != newVi))
            emit visualColumnOrderChanged(visualColumnOrder());
    });
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
}

QVector<QPair<int, Qt::SortOrder> > HeaderView::sortColumns() const
{
    return m_sortColumns;
}

void HeaderView::setSortColumns(const QVector<QPair<int, Qt::SortOrder> > &sortColumns)
{
    if (sortColumns != m_sortColumns) {
        m_sortColumns = sortColumns;
        update();
        emit sortColumnsChanged(sortColumns);
    }
}

bool HeaderView::isSorted() const
{
    return m_isSorted;
}

void HeaderView::setSorted(bool b)
{
    if (m_isSorted != b) {
        m_isSorted = b;
        update();
        emit isSortedChanged(b);
    }
}

QVector<int> HeaderView::visualColumnOrder() const
{
    QVector<int> order;

    for (int vi = 0; vi < count(); ++vi) {
        int li = logicalIndex(vi);
        if (!isSectionHidden(li))
            order << li;
    }
    return order;
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
            || (version < 2) || (version > 3)) {
        return false;
    }

    m_sortColumns.clear();
    qint32 sortColumnCount = 1;
    if (version == 3)
        ds >> sortColumnCount;
    for (int i = 0; i < sortColumnCount; ++i) {
        qint32 sortIndicator = -1;
        bool sortAscending = false;
        ds >> sortIndicator >> sortAscending;

        if ((sortIndicator >= 0) && (sortIndicator < count)) {
            m_sortColumns.append(qMakePair(sortIndicator, sortAscending
                                           ? Qt::AscendingOrder : Qt::DescendingOrder));
        }
    }

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
        if (li >= this->count()) // ignore columns that we don't know about yet
            continue;
        setSectionHidden(li, isHiddens.at(li));
        moveSection(visualIndex(li), vi);
        resizeSection(li, sizes.at(li));
    }
    // hide columns that got added after this was saved
    for (int li = count; li < this->count(); ++li)
        setSectionHidden(li, true);

    emit sortColumnsChanged(m_sortColumns);
    return true;
}

QByteArray HeaderView::saveLayout() const
{
    QByteArray config;
    QDataStream ds(&config, QIODevice::WriteOnly);

    ds << CONFIG_HEADER << qint32(3) /*version*/
       << qint32(count())
       << qint32(m_sortColumns.size());

    for (int i = 0; i < m_sortColumns.size(); ++i) {
       ds << qint32(m_sortColumns.at(i).first)
          << (m_sortColumns.at(i).second == Qt::AscendingOrder);
    }

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
    if (hide != hidden)
        emit visualColumnOrderChanged(visualColumnOrder());
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
    case QEvent::ToolTip: {
        auto he = static_cast<QHelpEvent *>(e);
        int li = logicalIndexAt(he->pos());
        if (li >= 0) {
            QString t = model()->headerData(li, orientation()).toString() % "\n\n"_l1
                    % tr("Click to set as primary sort column.") % "\n"_l1
                    % tr("Shift-click to set as additional sort column.") % "\n"_l1
                    % tr("Right-click for context menu.") % "\n"_l1
                    % tr("Drag to reposition and resize.");
            QToolTip::showText(he->globalPos(), t, this);
        }
        e->accept();
        return true;
    }
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

        a = new QAction(&m);
        a->setText(model()->headerData(li, Qt::Horizontal, Qt::DisplayRole).toString());
        a->setCheckable(true);
        a->setChecked(!isSectionHidden(li));
        a->setData(li);

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
        m_hiddenSizes.remove(i);
    }
}

#include "headerview.moc"
#include "moc_headerview.cpp"
