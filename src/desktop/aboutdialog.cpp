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
#include "ldraw/ldraw.h"
#include "ldraw/renderwidget.h"
#include "utility/utility.h"
#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "version.h"


AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    int iconSize = fontMetrics().height() * 7;
    ui->icon->setFixedSize(iconSize, iconSize);

    if (LDraw::Part *part = LDraw::library()->partFromId("3833")) {
        auto ldraw_icon = new LDraw::RenderWidget();
        ldraw_icon->setPartAndColor(part, 321);
        ldraw_icon->startAnimation();
        auto layout = new QHBoxLayout(ui->icon);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(ldraw_icon);
        ui->icon->setPixmap(QPixmap());
    }

    const auto about = Application::inst()->about();

    ui->header->setText(about["header"_l1].toString());
    ui->license->setText(about["license"_l1].toString());
    ui->translators->setText(about["translators"_l1].toString());

    setFixedSize(sizeHint());
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

#include "moc_aboutdialog.cpp"
