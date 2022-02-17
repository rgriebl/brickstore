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
#include <QPushButton>
#include <QClipboard>
#include <QGuiApplication>
#include <QMouseEvent>

#include "version.h"
#include "utility/utility.h"
#include "utility/eventfilter.h"
#include "utility/exception.h"
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
    new EventFilter(ui->buttons->button(QDialogButtonBox::Ok), [](QObject *, QEvent *e) -> bool {
        if (e->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::RightButton)
                static_cast<int *>(nullptr)[0] = 1;
            else if (me->button() == Qt::MiddleButton)
                throw Exception("Test exception");
        }
        return false;
    });

    connect(ui->buttons, &QDialogButtonBox::accepted,
            this, [this]() {
        QString text = ui->text->toMarkdown();
        QGuiApplication::clipboard()->setText(text);
    });

    QString text = "### BrickStore " BRICKSTORE_VERSION " (build: " BRICKSTORE_BUILD_NUMBER ")\n\n"_l1;
    auto sysInfo = SystemInfo::inst()->asMap();
    sysInfo.remove("os.type"_l1);
    sysInfo.remove("os.version"_l1);
    sysInfo.remove("hw.gpu.arch"_l1);
    sysInfo.remove("hw.memory"_l1);
    sysInfo.remove("brickstore.version"_l1);
    sysInfo["brickstore.ldraw"_l1] = LDraw::library()->lastUpdated().toString(Qt::RFC2822Date);

    for (auto it = sysInfo.cbegin(); it != sysInfo.cend(); ++it) {
        text = text % " * **"_l1 % it.key() % "**: "_l1 % it.value().toString() % "\n"_l1;
    }

    ui->text->setMarkdown(text);
}

SystemInfoDialog::~SystemInfoDialog()
{
    delete ui;
}

#include "moc_systeminfodialog.cpp"
