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
#ifndef __SETTINGSDIALOG_H__
#define __SETTINGSDIALOG_H__

#include <QDialog>

#include "ui_settingsdialog.h"

class QHttp;
class QBuffer;


class SettingsDialog : public QDialog, private Ui::SettingsDialog {
    Q_OBJECT

public:
    SettingsDialog(const QString &goto_page, QWidget *parent = 0, Qt::WindowFlags f = 0);

public slots:
    virtual void accept();

protected slots:
    void selectDocDir();
    void resetUpdateIntervals();
    void currenciesUpdated();
    void currentCurrencyChanged(const QString &);

protected:
    void load();
    void save();

private:
    QString m_preferedCurrency;
};

#endif
