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
#ifndef INCOMPLETEITEMDIALOG_H
#define INCOMPLETEITEMDIALOG_H

#include <QDialog>
#include <QMap>

#include "bricklinkfwd.h"
#include "ui_incompleteitemdialog.h"


class IncompleteItemDialog : public QDialog, private Ui::IncompleteItemDialog {
    Q_OBJECT

public:
    IncompleteItemDialog(BrickLink::InvItem *item, QWidget *parent = 0, Qt::WindowFlags f = 0);

private slots:
    void fixItem();
    void fixColor();

private:
    void checkFixed();
    static QString createDisplayString(BrickLink::InvItem *ii);
    static QString createDisplaySubString(const QString &what, const QString &realname, const QString &id, const QString &name);

private:
    BrickLink::InvItem *m_item;
};

#endif
