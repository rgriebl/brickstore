/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __CITEMVIEW_H__
#define __CITEMVIEW_H__

#include "clistview.h"
#include "cdocument.h"
#include "bricklink.h"


class QValidator;

class CItemViewItem;
class CItemViewPrivate;


class CItemView : public CListView {
    Q_OBJECT
public:
    CItemView(CDocument *doc, QWidget *parent = 0, const char *name = 0);
    virtual ~CItemView();

    CDocument *document() const;

    enum Filter {
        All        = -1,

        Prices     = -2,
        Texts      = -3,
        Quantities = -4,

        FilterCountSpecial = 4
    };


    class EditComboItem {
    public:
        EditComboItem(const QString &str = QString::null)
                : m_text(str), m_id(-1) { }
        EditComboItem(const QString &str, const QColor &fg, const QColor &bg, int id)
                : m_text(str), m_fg(fg), m_bg(bg), m_id(id) { }

    protected:
        QString m_text;
        QColor  m_fg;
        QColor  m_bg;
        int     m_id;

        friend class CItemView;
    };

    void editWithLineEdit(CItemViewItem *ivi, int col, const QString &text, const QString &mask = QString::null, QValidator *valid = 0, const QString &empty_value = QString());

    bool isDifferenceMode() const;
    bool isSimpleMode() const;

public slots:
    void setDifferenceMode(bool b);
    void setSimpleMode(bool b);

    void cancelEdit();
    void terminateEdit(bool commit);
    void applyFilter(const QString &filter, int field, bool is_regex);

    void loadDefaultLayout();
    void saveDefaultLayout();

signals:
    void editDone(CItemViewItem *ivi, int col, const QString &text, bool valid);
    void editCanceled(CItemViewItem *ivi, int col);

protected:
    void edit(CItemViewItem *ivi, int col, QWidget *editor);
    bool eventFilter(QObject *o, QEvent *e);

protected slots:
    virtual void listItemDoubleClicked(QListViewItem *, const QPoint &, int);
    void languageChange();

protected:
    static QString statusLabel(BrickLink::InvItem::Status status);
    static QString conditionLabel(BrickLink::Condition cond);

private:
    CItemViewPrivate *d;

    friend class CItemViewItem;
};


class CItemViewItem : public CListViewItem {
public:
    CItemViewItem(CDocument::Item *item, QListViewItem *parent, QListViewItem *after);
    CItemViewItem(CDocument::Item *item, QListView *parent, QListViewItem *after);

    virtual ~CItemViewItem();

    enum { RTTI = 1000 };

    virtual int width(const QFontMetrics &fm, const QListView *lv, int c) const;
    virtual QString text(int column) const;
    virtual const QPixmap *pixmap(int column) const;
    virtual QString key(int column, bool ascending) const;
    virtual int compare(QListViewItem * i, int col, bool ascending) const;
    virtual void setup();
    virtual void paintCell(QPainter * p, const QColorGroup & cg, int column, int width, int align);
    virtual int rtti() const;
    virtual QString toolTip(int column) const;

    CDocument::Item *item() const;
    BrickLink::Picture *picture() const;

    virtual void doubleClicked(const QPoint &p, int col);

    virtual CItemView *listView() const;

    virtual void editDone(int col, const QString &result, bool valid);

    QRect globalRect(int col) const;

protected:
    QColor shadeColor(int index) const;

private:
    void init(CDocument::Item *item);

    CDocument::Item *           m_item;
    mutable BrickLink::Picture *m_picture;
    Q_UINT64                    m_truncated;

    friend class CItemViewToolTip;
};


#endif

