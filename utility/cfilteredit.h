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
#ifndef __CFILTEREDIT_H__
#define __CFILTEREDIT_H__

#include <QWidget>

class QMenu;

class CFilterEditPrivate;

class CFilterEdit : public QWidget {
    Q_OBJECT
public:
    CFilterEdit(QWidget *parent = 0);
    void setMenu(QMenu *menu);
    QMenu *menu() const;

    void setMenuIcon(const QIcon &icon);
    void setClearIcon(const QIcon &icon);

    void setIdleText(const QString &str);
    
    virtual QSize sizeHint() const; 
   virtual QSize minimumSizeHint() const; 

    QString text() const;
    void setText(const QString &);

signals:
    void textChanged(const QString &);

protected:
    virtual void resizeEvent(QResizeEvent *e);
    
private:
    friend class CFilterEditPrivate;
    CFilterEditPrivate *d;
};

#endif
