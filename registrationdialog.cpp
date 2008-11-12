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
#include <QDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QLineEdit>
#include <QLabel>
#include <QToolTip>

#include "messagebox.h"
#include "utility.h"
#include "config.h"
#include "version.h"

#include "registrationdialog.h"


RegistrationDialog::RegistrationDialog(bool initial, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    switch (Config::inst()->registration()) {
    case Config::None:
    case Config::Personal:
        w_personal->setChecked(true);
        break;

    case Config::Demo:
        w_demo->setChecked(true);
        break;

    case Config::Full: {
        w_full->setChecked(true);
        w_full_name->setText(Config::inst()->registrationName());
        QString s = Config::inst()->registrationKey();
        if (s.isEmpty())
            s = QLatin1String("1234-5678-9ABC");
        w_full_key->setText(s);

        w_demo->setEnabled(false);
        w_personal->setEnabled(false);
        break;
    }
    case Config::OpenSource:
        // we shouldn't be here at all
        break;
    }

    if (!initial) {
        w_initial_hint->hide();
    }
    else {
        w_initial_hint->setPalette(QToolTip::palette());
        w_buttons->button(QDialogButtonBox::Cancel)->setEnabled(false);
    }

    resize(sizeHint());
}

RegistrationDialog::~RegistrationDialog()
{ }

void RegistrationDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
    else
        QDialog::changeEvent(e);
}

void RegistrationDialog::accept()
{
    QString name;
    QString key = 0;
    bool ok = true;

    if (w_personal->isChecked()) {
        name = QLatin1String("PERSONAL");
    }
    else if (w_demo->isChecked()) {
        name = QLatin1String("DEMO");
    }
    else if (w_full->isChecked()) {
        name = w_full_name->text();
        key = w_full_key->text();
        ok = Config::inst()->checkRegistrationKey(name, key);
    }

    hide();

    if (!ok) {
        MessageBox::information(this, tr("Sorry - the registration key you entered is not valid!"));
        reject();
    }
    else {
        Config::inst()->setRegistration(name, key);
        QDialog::accept();
    }
}

void RegistrationDialog::reject()
{
    if (w_buttons->button(QDialogButtonBox::Cancel)->isEnabled())
        QDialog::reject();
}
