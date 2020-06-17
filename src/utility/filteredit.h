/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#pragma once

#include <QLineEdit>

class QMenu;
class FilterEditButton;

class FilterEdit : public QLineEdit {
    Q_OBJECT

public:
    FilterEdit(QWidget *parent = nullptr);

    void setMenu(QMenu *);
    QMenu *menu() const;

protected:
    void resizeEvent(QResizeEvent *e);

private slots:
    void checkText(const QString &);

private:
    void doLayout();

    FilterEditButton *w_menu;
    FilterEditButton *w_clear;
    int               m_left;
    int               m_top;
    int               m_right;
    int               m_bottom;
};
