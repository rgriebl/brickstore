/* Copyright (C) 2013-2014 Patrick Brans.  All rights reserved.
**
** This file is part of BrickStock.
** BrickStock is based heavily on BrickStore (http://www.brickforge.de/software/brickstore/)
** by Robert Griebl, Copyright (C) 2004-2008.
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
#ifndef __CSELECTITEM_H__
#define __CSELECTITEM_H__

#include <qdialog.h>
#include <Q3ListViewItem>
#include <QToolTip>
#include <Q3MimeSourceFactory>
#include <QPainter>
//Added by qt3to4:
#include <QShowEvent>

#include "bricklink.h"
#include "clistview.h"
#include "cutility.h"

class Q3ListViewItem;
class Q3IconViewItem;

class CSelectItemPrivate;

class CSelectItem : public QWidget {
	Q_OBJECT
public:
	CSelectItem ( QWidget *parent, const char *name = 0, Qt::WFlags fl = 0 );
    virtual ~CSelectItem ( );

	bool setItemType ( const BrickLink::ItemType * );
	bool setItem ( const BrickLink::Item * );
	bool setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter );

	const BrickLink::Item *item ( ) const;

	void setOnlyWithInventory ( bool b );
	bool isOnlyWithInventory ( ) const;
	
	enum ViewMode { ViewMode_List, ViewMode_ListWithImages, ViewMode_Thumbnails };

	virtual QSize sizeHint ( ) const;

signals:
	void hasColors ( bool );
	void itemSelected ( const BrickLink::Item *, bool );

protected slots:
	void itemConfirmed ( );
	void itemChangedList ( );
	void itemChangedIcon ( );
	void itemTypeChanged ( );
	void categoryChanged ( );
	void applyFilter ( );
	void viewModeChanged ( int );
	void pictureUpdated ( BrickLink::Picture * );
	void findItem ( );
	void languageChange ( );
	void itemContextList ( Q3ListViewItem *, const QPoint & );
	void itemContextIcon ( Q3IconViewItem *, const QPoint & );

protected:
	virtual void showEvent ( QShowEvent * );

private:
	bool fillCategoryView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat = 0 );
	bool fillItemView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select = 0 );

	const BrickLink::Item *fillItemListView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select = 0 );
	const BrickLink::Item *fillItemIconView ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select = 0 );

	bool setViewMode ( ViewMode ivm, const BrickLink::ItemType *itt, const BrickLink::Category *cat, const BrickLink::Item *select = 0 );
	ViewMode checkViewMode ( ViewMode ivm, const BrickLink::ItemType *itt, const BrickLink::Category *cat );

	void ensureSelectionVisible ( );

	void itemContext ( const BrickLink::Item *item, const QPoint &pos );

protected:
	CSelectItemPrivate *d;
};

class CSelectItemDialog : public QDialog {
	Q_OBJECT
public:
	explicit CSelectItemDialog ( bool only_with_inventory, QWidget *parent = 0, const char *name = 0, bool modal = true, Qt::WFlags fl = 0 );

	bool setItemType ( const BrickLink::ItemType * );
	bool setItem ( const BrickLink::Item * );
	bool setItemTypeCategoryAndFilter ( const BrickLink::ItemType *itt, const BrickLink::Category *cat, const QString &filter );

	const BrickLink::Item *item ( ) const;

	virtual int exec ( const QRect &pos = QRect ( ));

private slots:
	void checkItem ( const BrickLink::Item *, bool );
	
private:
	CSelectItem *w_si;

	QPushButton *w_ok;
	QPushButton *w_cancel;
};

class ItemListItem : public CListViewItem {
public:
    ItemListItem ( CListView *lv, const BrickLink::Item *item, const CSelectItem::ViewMode &viewmode, const bool &invonly )
        : CListViewItem ( lv ), m_item ( item ), m_viewmode ( viewmode ), m_invonly ( invonly ), m_picture ( 0 )
    { }

    virtual ~ItemListItem ( )
    {
        if ( m_picture )
            m_picture-> release ( );
    }

    void pictureChanged ( )
    {
        if ( m_viewmode != CSelectItem::ViewMode_Thumbnails )
            repaint ( );
    }

    virtual QString text ( int col ) const
    {
        switch ( col ) {
            case  1: return m_item-> id ( );
            case  2: return m_item-> name ( );
            default: return QString::null;
        }
    }

    virtual const QPixmap *pixmap ( int col ) const
    {
        if (( col == 0 ) && ( m_viewmode == CSelectItem::ViewMode_ListWithImages )) {
            BrickLink::Picture *pic = picture ( );

            if ( pic && pic-> valid ( )) {
                static QPixmap val2ptr;

                val2ptr = pic-> pixmap ( );
                return &val2ptr;
            }
        }
        return 0;
    }

    BrickLink::Picture *picture ( ) const
    {
        if ( m_picture && (( m_picture-> item ( ) != m_item ) || ( m_picture-> color ( ) != m_item-> defaultColor ( )))) {
            m_picture-> release ( );
            m_picture = 0;
        }

        if ( !m_picture && m_item && m_item-> defaultColor ( )) {
            m_picture = BrickLink::inst ( )-> picture ( m_item, m_item-> defaultColor ( ), true );

            if ( m_picture )
                m_picture-> addRef ( );
        }
        return m_picture;
    }


    void setup ( )
    {
        CListViewItem::setup ( );

        if ( m_viewmode == CSelectItem::ViewMode_ListWithImages ) {
            int h = QMAX( 30, listView ( )-> fontMetrics ( ). height ( )) + 2 * listView ( )-> itemMargin ( );

            if ( h & 1 )
                h++;

            setHeight ( h );
        }
    }

    virtual void paintCell ( QPainter *p, const QColorGroup &cg, int col, int w, int align )
    {
        if ( m_viewmode != CSelectItem::ViewMode_ListWithImages ) {
            if ( !isSelected ( ) && m_invonly && m_item && !m_item-> hasInventory ( )) {
                QColorGroup _cg ( cg );
                _cg. setColor ( QColorGroup::Text, CUtility::gradientColor ( cg. base ( ), cg. text ( ), 0.5f ));

                CListViewItem::paintCell ( p, _cg, col, w, align );
            }
            else
                CListViewItem::paintCell ( p, cg, col, w, align );
            return;
        }

        int h = height ( );
        int margin = listView ( )-> itemMargin ( );

        const QPixmap *pix = pixmap ( col );
        QString str = text ( col );

        QColor bg;
        QColor fg;

        if ( CListViewItem::isSelected ( )) {
            bg = cg. highlight ( );
            fg = cg. highlightedText ( );
        }
        else {
            bg = backgroundColor ( );
            fg = cg. text ( );

            if ( m_invonly && m_item && !m_item-> hasInventory ( ))
                fg = CUtility::gradientColor ( fg, bg, 0.5f );
        }

        p-> fillRect ( 0, 0, w, h, bg );
        p-> setPen ( fg );

        int x = 0, y = 0;

        if ( pix && !pix-> isNull ( )) {
            // clip the pixmap here .. this is cheaper than a cliprect

            int rw = w - 2 * margin;
            int rh = h - 2 * margin;

            int sw, sh;

            if ( pix-> height ( ) <= rh ) {
                sw = QMIN( rw, pix-> width ( ));
                sh = QMIN( rh, pix-> height ( ));
            }
            else {
                sw = pix-> width ( ) * rh / pix-> height ( );
                sh = rh;
            }

            if ( pix-> height ( ) <= rh )
                p-> drawPixmap ( x + margin, y + margin, *pix, 0, 0, sw, sh );
            else
                p-> drawPixmap ( QRect ( x + margin, y + margin, sw, sh ), *pix );

            x += ( sw + margin );
            w -= ( sw + margin );
        }
        if ( !str. isEmpty ( )) {
            int rw = w - 2 * margin;

            if ( !( align & Qt::AlignTop || align & Qt::AlignBottom ))
                align |= Qt::AlignVCenter;

            const QFontMetrics &fm = p-> fontMetrics ( );

            if ( fm. width ( str ) > rw )
                str = CUtility::ellipsisText ( str, fm, w, align );

            p-> drawText ( x + margin, y, rw, h, align, str );
        }
    }

    QString toolTip ( ) const
    {
        QString str = "<table><tr><td rowspan=\"2\">%1</td><td><b>%2</b></td></tr><tr><td>%3</td></tr></table>";
        QString left_cell;

        BrickLink::Picture *pic = picture ( );

        if ( pic && pic-> valid ( )) {
            Q3MimeSourceFactory::defaultFactory ( )-> setPixmap ( m_item-> name ( ), pic-> pixmap ( ));

            left_cell = "<img src=\"" + QString(m_item-> name ( )) + "\" />";
        }
        else if ( pic && ( pic-> updateStatus ( ) == BrickLink::Updating )) {
            left_cell = "<i>" + CSelectItem::tr( "[Image is loading]" ) + "</i>";
        }

        return str. arg( left_cell ). arg( m_item-> id ( )). arg( m_item-> name ( ));
    }

    const BrickLink::Item *item ( ) const
    {
        return m_item;
    }

private:
    const BrickLink::Item *       m_item;
    const CSelectItem::ViewMode & m_viewmode;
    const bool &                  m_invonly;
    mutable BrickLink::Picture *  m_picture;

    friend class ItemListToolTip;
};

class ItemListToolTip : public QObject/*, public QToolTip*/ {
    Q_OBJECT
public:
    ItemListToolTip ( CListView *list )
        : QObject ( list-> viewport ( )), //QToolTip ( list-> viewport ( ), new QToolTipGroup ( list-> viewport ( ))),
          m_list ( list ), m_tip_item ( 0 )
    {
        //connect ( group ( ), SIGNAL( removeTip ( )), this, SLOT( tipHidden ( )));

        connect ( BrickLink::inst ( ), SIGNAL( pictureUpdated ( BrickLink::Picture * )), this, SLOT( pictureUpdated ( BrickLink::Picture * )));
    }

    virtual ~ItemListToolTip ( )
    { }

protected:
    bool eventFilter( QObject *, QEvent *e )
    {
        bool res = false;
        if ( !(QWidget *)QObject::parent() || !m_list )
            res = false;

        if (e->type() == QEvent::ToolTip) {
            QToolTip::remove ( (QWidget *)QObject::parent());
            QHelpEvent *helpEvent = static_cast<QHelpEvent *>(e);
            QPoint pos = helpEvent->pos();

            ItemListItem *item = static_cast <ItemListItem *> ( m_list-> itemAt ( pos ));

            if ( item ) {
                m_tip_item = item;
                QToolTip::showText(helpEvent->globalPos(), item-> toolTip ( ), (QWidget *)QObject::parent(), m_list-> itemRect ( item ));
            }

            res = true;
        }

        return res;// ? true : Q3ListView::eventFilter ( o, e );
        //return false;
    }

private slots:
    void pictureUpdated ( BrickLink::Picture *pic )
    {
        if ( m_tip_item && m_tip_item-> m_picture == pic ) {
            QHelpEvent *e = new QHelpEvent(QEvent::ToolTip, ((QWidget *)QObject::parent())-> mapFromGlobal ( QCursor::pos ( )), QCursor::pos ( ));
            eventFilter(((QWidget *)QObject::parent()), e);
        }
    }

private:
    CListView *m_list;
    ItemListItem *m_tip_item;
};

#endif
