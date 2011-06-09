/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#ifndef __SELECTCOLOR_H__
#define __SELECTCOLOR_H__

#include <QDialog>

#include "bricklinkfwd.h"

class QTreeView;
class QComboBox;

class SelectColor : public QWidget {
    Q_OBJECT
public:
    SelectColor(QWidget *parent = 0, Qt::WindowFlags f = 0);

    void setWidthToContents(bool b);

    void setCurrentColor(const BrickLink::Color *);
    const BrickLink::Color *currentColor() const;

signals:
    void colorSelected(const BrickLink::Color *, bool);

protected slots:
    void colorChanged();
    void colorConfirmed();
    void updateColorFilter(int filter);
    void languageChange();

protected:
    virtual void changeEvent(QEvent *);
    virtual void showEvent(QShowEvent *);

protected:
    QComboBox *w_filter;
    QTreeView *w_colors;

//    friend class SelectColor;Dialog
};

#endif
