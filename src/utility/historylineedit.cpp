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
#include <QDataStream>
#include <QCompleter>
#include <QAction>
#include <QAbstractProxyModel>
#include <QAbstractItemView>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QDebug>

#include "historylineedit.h"


class HistoryViewDelegate : public QStyledItemDelegate
{
public:
    HistoryViewDelegate(HistoryLineEdit *filter);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    HistoryLineEdit *m_edit;
};

HistoryViewDelegate::HistoryViewDelegate(HistoryLineEdit *filter)
    : QStyledItemDelegate(filter)
    , m_edit(filter)
{ }

void HistoryViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    QRect r = option.rect;
    r.setLeft(r.left() + r.width() - r.height());
    const int margin = 2;
    r.adjust(margin, margin, -margin, -margin);
    m_edit->m_deleteIcon.paint(painter, r);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


HistoryLineEdit::HistoryLineEdit(QWidget *parent)
    : QLineEdit(parent)
    , m_deleteIcon(QIcon::fromTheme("window-close"))
{
    auto *comp = new QCompleter(&m_filterModel, this);

    comp->setCaseSensitivity(Qt::CaseInsensitive);
    comp->setCompletionMode(QCompleter::PopupCompletion);
    setCompleter(comp);
    auto view = completer()->popup();
    view->viewport()->installEventFilter(this);
    view->setItemDelegate(new HistoryViewDelegate(this));
    setClearButtonEnabled(true);

    connect(this, &QLineEdit::returnPressed, this, [this]() {
        QString t = text().trimmed();
        if (t.isEmpty())
            return;

        QStringList sl = m_filterModel.stringList();
        sl.removeAll(t);
        sl.append(t);
        m_filterModel.setStringList(sl);

    }, Qt::QueuedConnection);

    // Adding a menuAction() to a QLineEdit leads to a strange activation behvior:
    // only the right side of the icon will react to mouse clicks
    QPixmap filterPix(QIcon::fromTheme("view-filter").pixmap(style()->pixelMetric(QStyle::PM_SmallIconSize)));
    {
        QPainter p(&filterPix);
        QStyleOption so;
        so.initFrom(this);
        so.rect = filterPix.rect();
        int mbi = style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &so, this);
#if defined(Q_OS_MACOS)
        mbi += 2;
#else
        mbi -= 6;
#endif
        so.rect = QRect(0, so.rect.bottom() - mbi, mbi, mbi);
        style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &so, &p, this);
    }

    auto *a = addAction(QIcon(filterPix), QLineEdit::LeadingPosition);
    connect(a, &QAction::triggered, this, [this]() {
        completer()->setCompletionPrefix(QString());
        completer()->complete();
    });
}

QByteArray HistoryLineEdit::saveState() const
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray("FH") << qint32(3);
    ds << text();
    ds << m_filterModel.stringList();

    return ba;
}

bool HistoryLineEdit::restoreState(const QByteArray &ba)
{
    QDataStream ds(ba);
    QByteArray tag;
    qint32 version;
    ds >> tag >> version;
    if ((ds.status() != QDataStream::Ok) || (tag != "FH") || (version != 3))
        return false;

    QString text;
    QStringList list;
    ds >> text >> list;

    if (ds.status() != QDataStream::Ok)
        return false;

    m_filterModel.setStringList(list);
    setText(text);
    return true;
}

void HistoryLineEdit::keyPressEvent(QKeyEvent *ke)
{
    if (ke->key() == Qt::Key_Down && !ke->modifiers()) {
        if (text().isEmpty())
            completer()->setCompletionPrefix(QString());
        completer()->complete();
    }
    return QLineEdit::keyPressEvent(ke);
}

bool HistoryLineEdit::eventFilter(QObject *o, QEvent *e)
{
    auto view = completer()->popup();

    if ((o == view->viewport()) && (e->type() == QEvent::MouseButtonPress)) {
        auto me = static_cast<QMouseEvent *>(e);
        QModelIndex idx = view->indexAt(me->pos());

        if (idx.isValid()) {
            int h = view->visualRect(idx).height();
            if (me->x() >= (view->viewport()->width() - h)) {
                if (auto proxy = qobject_cast<const QAbstractProxyModel *>(idx.model()))
                    idx = proxy->mapToSource(idx);
                m_filterModel.removeRow(idx.row());
                return true;
            }
        }
    }
    return QLineEdit::eventFilter(o, e);
}
