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
#include "ldraw.h"
#include "ldraw/renderwidget.h"
#include "version.h"
#include "config.h"
#include "utility.h"
#include "aboutdialog.h"
#include "ui_aboutdialog.h"


AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    int iconSize = fontMetrics().height() * 7;
    ui->icon->setFixedSize(iconSize, iconSize);

    if (LDraw::Part *part = LDraw::core() ? LDraw::core()->partFromId("3833") : nullptr) {
        auto ldraw_icon = new LDraw::RenderWidget();
        ldraw_icon->setPartAndColor(part, 321);
        ldraw_icon->startAnimation();
        auto layout = new QHBoxLayout(ui->icon);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(ldraw_icon);
        ui->icon->setPixmap(QPixmap());
    }

    ui->version->setText(ui->version->text().arg(QLatin1String(BRICKSTORE_VERSION),
                                             QLatin1String(BRICKSTORE_BUILD_NUMBER)));
    ui->copyright->setText(ui->copyright->text().arg(QLatin1String(BRICKSTORE_COPYRIGHT)));
    ui->visit->setText(ui->visit->text().arg(QLatin1String(R"(<a href="https://)" BRICKSTORE_URL R"(">)" BRICKSTORE_URL R"(</a>)")));

    QString transTable;
    const QString transRow = R"(<tr><td>%1:</td><td width="2em"></td><td>%2 &lt;<a href="mailto:%3">%4</a>&gt;</td></tr>)"_l1;
    const auto translations = Config::inst()->translations();
    for (const Config::Translation &trans : translations) {
        if (trans.language != "en"_l1) {
            QString langname = trans.languageName.value(Config::inst()->language(),
                                                        trans.languageName["en"_l1]);
            transTable += transRow.arg(langname, trans.author, trans.authorEMail, trans.authorEMail);
        }
    }
    transTable = R"(<table border="0">)"_l1 % transTable % R"(</table>)"_l1;
    ui->translators->setText(transTable);

    setFixedSize(sizeHint());
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

#include "moc_aboutdialog.cpp"
