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

#include <QDialog>

#include "bricklinkfwd.h"

class QTreeView;
class QComboBox;


class SelectColor : public QWidget
{
    Q_OBJECT
public:
    SelectColor(QWidget *parent = nullptr);

    void setWidthToContents(bool b);

    void setCurrentColor(const BrickLink::Color *color);
    const BrickLink::Color *currentColor() const;

signals:
    void colorSelected(const BrickLink::Color *, bool);

protected slots:
    void colorChanged();
    void colorConfirmed();
    void updateColorFilter(int filter);
    void languageChange();

protected:
    void changeEvent(QEvent *) override;
    void showEvent(QShowEvent *) override;

protected:
    QComboBox *w_filter;
    QTreeView *w_colors;
};
