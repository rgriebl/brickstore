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
#ifndef __DREGISTRATION_H__
#define __DREGISTRATION_H__

#include "bricklinkfwd.h"

#include "ui_registration.h"

class DRegistration : public QDialog, private Ui::Registration {
public:
    DRegistration(bool initial, QWidget *parent = 0, Qt::WindowFlags f = 0);
    ~DRegistration();

protected:
    virtual void accept();
    virtual void reject();
    virtual void changeEvent(QEvent *e);
};

#endif
