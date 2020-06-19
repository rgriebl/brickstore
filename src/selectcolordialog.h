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
#include "ui_selectcolordialog.h"


class SelectColorDialog : public QDialog, private Ui::SelectColorDialog
{
    Q_OBJECT
public:
    SelectColorDialog(QWidget *parent = nullptr);

    void setColor(const BrickLink::Color *);
    const BrickLink::Color *color() const;

    int execAtPosition(const QRect &pos = QRect());

protected:
    void showEvent(QShowEvent *) override;

private slots:
    void checkColor(const BrickLink::Color *, bool);

private:
    QRect m_pos;
};
