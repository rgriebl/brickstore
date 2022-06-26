/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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

#include "common/application.h"
#include "common/config.h"
#include "utility/utility.h"
#include "aboutdialog.h"
#include "ui_aboutdialog.h"


AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    int iconSize = fontMetrics().height() * 7;
    ui->icon->setFixedSize(iconSize, iconSize);

    const auto about = Application::inst()->about();

    ui->header->setText(about[u"header"_qs].toString());
    ui->license->setText(about[u"license"_qs].toString());
    ui->translators->setText(about[u"translators"_qs].toString());

    setFixedSize(sizeHint());
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

#include "moc_aboutdialog.cpp"
