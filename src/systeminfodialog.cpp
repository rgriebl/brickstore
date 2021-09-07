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
#include <QPushButton>
#include <QClipboard>
#include <QGuiApplication>

#include "version.h"
#include "utility/utility.h"
#include "ldraw/ldraw.h"
#include "utility/systeminfo.h"
#include "systeminfodialog.h"
#include "ui_systeminfodialog.h"

SystemInfoDialog::SystemInfoDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SystemInfoDialog)
{
    ui->setupUi(this);

    ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Copy to clipboard"));

    // sentry crash handler test
    ui->buttons->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->buttons, &QDialogButtonBox::customContextMenuRequested,
            this, []() { static_cast<int *>(0)[0] = 1; });

    connect(ui->buttons, &QDialogButtonBox::accepted,
            this, [=]() {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        QString text = ui->text->toPlainText();
#else
        QString text = ui->text->toMarkdown();
#endif
        QGuiApplication::clipboard()->setText(text);
    });

    QString text = "### BrickStore " BRICKSTORE_VERSION " (build: " BRICKSTORE_BUILD_NUMBER ")\n\n"_l1;
    auto sysInfo = SystemInfo::inst()->asMap();
    sysInfo.remove("os.type"_l1);
    sysInfo.remove("os.version"_l1);
    sysInfo.remove("hw.gpu.arch"_l1);
    sysInfo.remove("hw.memory"_l1);
    sysInfo.remove("brickstore.version"_l1);
    sysInfo["brickstore.ldraw"_l1] = bool(LDraw::core());

    for (auto it = sysInfo.cbegin(); it != sysInfo.cend(); ++it) {
        text = text % " * **"_l1 % it.key() % "**: "_l1 % it.value().toString() % "\n"_l1;
    }

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    ui->text->setPlainText(text.remove("**"_l1));
#else
    ui->text->setMarkdown(text);
#endif
}

SystemInfoDialog::~SystemInfoDialog()
{
    delete ui;
}
