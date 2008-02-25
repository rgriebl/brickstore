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

#include "cmessagebox.h"
#include "cutility.h"
#include "cconfig.h"
#include "version.h"

#include "dregistration.h"


DRegistration::DRegistration(bool initial, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    setupUi(this);

    switch (CConfig::inst()->registration()) {
    case CConfig::None:
    case CConfig::Personal:
        w_personal->setChecked(true);
        break;

    case CConfig::Demo:
        w_demo->setChecked(true);
        break;

    case CConfig::Full: {
        w_full->setChecked(true);
        w_full_name->setText(CConfig::inst()->registrationName());
        QString s = CConfig::inst()->registrationKey();
        if (s.isEmpty())
            s = "1234-5678-9ABC";
        w_full_key->setText(s);

        w_demo->setEnabled(false);
        w_personal->setEnabled(false);
        break;
    }
    case CConfig::OpenSource:
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

DRegistration::~DRegistration()
{ }

void DRegistration::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
    else
        QDialog::changeEvent(e);
}

void DRegistration::accept()
{
    QString name;
    QString key = 0;
    bool ok = true;

    if (w_personal->isChecked()) {
        name = "PERSONAL";
    }
    else if (w_demo->isChecked()) {
        name = "DEMO";
    }
    else if (w_full->isChecked()) {
        name = w_full_name->text();
        key = w_full_key->text();
        ok = CConfig::inst()->checkRegistrationKey(name, key);
    }

    hide();

    if (!ok) {
        CMessageBox::information(this, tr("Sorry - the registration key you entered is not valid!"));
        reject();
    }
    else {
        CConfig::inst()->setRegistration(name, key);
        QDialog::accept();
    }
}

void DRegistration::reject()
{
    if (w_buttons->button(QDialogButtonBox::Cancel)->isEnabled())
        QDialog::reject();
}
