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
#ifndef __CAPPEARSINWIDGET_H__
#define __CAPPEARSINWIDGET_H__

#include <q3mimefactory.h>
#include <qtooltip.h>

#include "clistview.h"
#include "bricklink.h"

class CAppearsInWidgetPrivate;
class QAction;

class CAppearsInWidget : public CListView {
	Q_OBJECT

public:
	CAppearsInWidget ( QWidget *parent = 0, const char *name = 0, Qt::WFlags fl = 0 );
	virtual ~CAppearsInWidget ( );
	
	void setItem ( const BrickLink::Item *item, const BrickLink::Color *color = 0 );
    void setItem ( const BrickLink::InvItemList &list );

	virtual QSize minimumSizeHint ( ) const;
	virtual QSize sizeHint ( ) const;

protected:
    virtual void contentsMouseDoubleClickEvent(QMouseEvent *e);

protected slots:
	void viewLargeImage ( );
	void showBLCatalogInfo ( );
	void showBLPriceGuideInfo ( );
	void showBLLotsForSale ( );
	void languageChange ( );

private slots:
	void showContextMenu ( Q3ListViewItem *, const QPoint & );
	void partOut ( );

private:
	CAppearsInWidgetPrivate *d;
};

class AppearsInListItem : public CListViewItem {

public:
    AppearsInListItem ( CListView *lv, int qty, const BrickLink::Item *item )
        : CListViewItem ( lv ), m_qty ( qty ), m_item ( item ), m_picture ( 0 )
    {}

    virtual ~AppearsInListItem ( )
    {
        if ( m_picture )
            m_picture-> release ( );
    }

    virtual QString text ( int col ) const
    {
        switch ( col ) {
            case  0: return ( m_qty > 0  ) ? QString::number ( m_qty ) : QString ( "-" );
            case  1: return m_item-> id ( );
            case  2: return m_item-> name ( );
            default: return QString ( );
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
            left_cell = "<i>" + CAppearsInWidget::tr( "[Image is loading]" ) + "</i>";
        }

        return str. arg( left_cell ). arg( m_item-> id ( )). arg( m_item-> name ( ));
    }

    virtual int compare ( Q3ListViewItem *i, int col, bool ascending ) const
    {
        if ( col == 0 )
            return ( m_qty - static_cast <AppearsInListItem *> ( i )-> m_qty );
        else
            return CListViewItem::compare ( i, col, ascending );
    }

    const BrickLink::Item *item ( ) const
    { return m_item; }

    BrickLink::Picture *picture ( ) const
    {
        if ( !m_picture && m_item) {
            m_picture = BrickLink::inst ( )-> picture ( m_item, m_item-> defaultColor ( ), true );

            if ( m_picture )
                m_picture-> addRef ( );
        }
        return m_picture;
    }

private:
    int                         m_qty;
    const BrickLink::Item *     m_item;
    mutable BrickLink::Picture *m_picture;
};

class AppearsInToolTip : public QObject {
    Q_OBJECT

public:
    AppearsInToolTip ( QWidget *parent, CAppearsInWidget *aiw )
        : QObject ( parent),// QToolTip ( parent, new QToolTipGroup ( parent )),
          m_aiw ( aiw ), m_tip_item ( 0 )
    {
        //connect ( group ( ), SIGNAL( removeTip ( )), this, SLOT( tipHidden ( )));

        connect ( BrickLink::inst ( ), SIGNAL(pictureUpdated(BrickLink::Picture*)), this, SLOT(pictureUpdated(BrickLink::Picture*)));
    }

    virtual ~AppearsInToolTip ( )
    { }

protected:
    bool eventFilter( QObject *, QEvent *e )
    {
        bool res = false;
        if ( !(QWidget *)QObject::parent() || !m_aiw /*|| !m_aiw-> showToolTips ( )*/ )
            res = false;

        if (e->type() == QEvent::ToolTip) {
            QToolTip::remove ( (QWidget *)QObject::parent());
            QHelpEvent *helpEvent = static_cast<QHelpEvent *>(e);
            QPoint pos = helpEvent->pos();

            AppearsInListItem *item = static_cast <AppearsInListItem *> ( m_aiw-> itemAt ( pos ));

            if ( item ) {
                m_tip_item = item;
                QToolTip::showText(helpEvent->globalPos(), item-> toolTip ( ), (QWidget *)QObject::parent(), m_aiw-> itemRect ( item ));
            }

            res = true;
        }

        return res;// ? true : Q3ListView::eventFilter ( o, e );
        //return false;
    }

private slots:
    void pictureUpdated ( BrickLink::Picture *pic )
    {
        if ( m_tip_item && m_tip_item-> picture ( ) == pic ) {
            QHelpEvent *e = new QHelpEvent(QEvent::ToolTip, ((QWidget *)QObject::parent())-> mapFromGlobal ( QCursor::pos ( )), QCursor::pos ( ));
            eventFilter(((QWidget *)QObject::parent()), e);
        }
    }

private:
    CAppearsInWidget *m_aiw;
    AppearsInListItem *m_tip_item;
};

#endif
