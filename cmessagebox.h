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
#ifndef __CMESSAGEBOX_H__
#define __CMESSAGEBOX_H__

#include <QMessageBox>

inline QString CMB_BOLD(const QString &str)
{
    return "<b>" + str + "</b>";
}

class QValidator;

class CMessageBox : public QMessageBox {
    Q_OBJECT

public:
    static void setDefaultTitle(const QString &s);
    static QString defaultTitle();

    static StandardButton information(QWidget *parent, const QString & text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton question(QWidget *parent, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton warning(QWidget *parent, const QString & text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton critical(QWidget *parent, const QString & text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

    static bool getString(QWidget *parent, const QString &text, const QString &unit, QString &value, QValidator *validate = 0);
    static bool getString(QWidget *parent, const QString &text, QString &value);
    static bool getDouble(QWidget *parent, const QString &text, const QString &unit, double &value, QValidator *validate = 0);
    static bool getInteger(QWidget *parent, const QString &text, const QString &unit, int &value, QValidator *validate = 0);


private:
    CMessageBox();

    static StandardButton msgbox(QWidget *parent, const QString &msg, QMessageBox::Icon icon, StandardButtons buttons, StandardButton defaultButton);

private:
    static QString s_deftitle;
};

#endif
