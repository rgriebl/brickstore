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

#include <QMessageBox>

class QValidator;


inline QString CMB_BOLD(const QString &str)
{
    return QLatin1String("<b>") + str + QLatin1String("</b>");
}


class MessageBox : public QMessageBox
{
    Q_OBJECT
public:
    static void setDefaultTitle(const QString &s);
    static QString defaultTitle();

    static StandardButton information(QWidget *parent, const QString & text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton question(QWidget *parent, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton warning(QWidget *parent, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton critical(QWidget *parent, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

    static bool getString(QWidget *parent, const QString &text, QString &value);
    static bool getDouble(QWidget *parent, const QString &text, const QString &unit, double &value, double minValue = -2147483647, double maxValue = 2147483647, int decimals = 1);
    static bool getInteger(QWidget *parent, const QString &text, const QString &unit, int &value, int minValue = -2147483647, int maxValue = 2147483647);

private:
    MessageBox();

    static StandardButton msgbox(QWidget *parent, const QString &msg, QMessageBox::Icon icon, StandardButtons buttons, StandardButton defaultButton);

private:
    static QString s_deftitle;
};
