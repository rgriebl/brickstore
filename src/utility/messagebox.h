/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#include <QPointer>

#include "utility.h"

QT_FORWARD_DECLARE_CLASS(QValidator)


inline QString CMB_BOLD(const QString &str)
{
    return "<b>"_l1 + str + "</b>"_l1;
}


class MessageBox : public QMessageBox
{
    Q_OBJECT
public:
    static QString defaultTitle();

    static void setDefaultParent(QWidget *parent);
    static QWidget *defaultParent();

    static StandardButton information(QWidget *parent, const QString &title, const QString &text,
                                      StandardButtons buttons = Ok,
                                      StandardButton defaultButton = NoButton);
    static StandardButton question(QWidget *parent, const QString &title, const QString &text,
                                   StandardButtons buttons = Yes | No,
                                   StandardButton defaultButton = NoButton);
    static StandardButton warning(QWidget *parent, const QString &title, const QString &text,
                                  StandardButtons buttons = Ok,
                                  StandardButton defaultButton = NoButton);
    static StandardButton critical(QWidget *parent, const QString &title, const QString &text,
                                   StandardButtons buttons = Ok,
                                   StandardButton defaultButton = NoButton);

    static bool getString(QWidget *parent, const QString &title, const QString &text,
                          QString &value, const QRect &positionRelativeToRect = { });
    static bool getDouble(QWidget *parent, const QString &title, const QString &text,
                          const QString &unit, double &value, double minValue = -2147483647,
                          double maxValue = 2147483647, int decimals = 1,
                          const QRect &positionRelativeToRect = { });
    static bool getInteger(QWidget *parent, const QString &title, const QString &text,
                           const QString &unit, int &value,
                           int minValue = -2147483647, int maxValue = 2147483647,
                           const QRect &positionRelativeToRect = { });

private:
    MessageBox();

    static StandardButton msgbox(QWidget *parent, const QString &title, const QString &msg,
                                 QMessageBox::Icon icon, StandardButtons buttons,
                                 StandardButton defaultButton);

private:
    static QPointer<QWidget> s_defaultParent;
};
