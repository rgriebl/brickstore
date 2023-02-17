// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QPushButton>
#include <QClipboard>
#include <QGuiApplication>
#include <QMouseEvent>

#include "common/application.h"
#include "common/eventfilter.h"
#include "utility/exception.h"
#include "ldraw/library.h"
#include "common/systeminfo.h"
#include "systeminfodialog.h"
#include "ui_systeminfodialog.h"


SystemInfoDialog::SystemInfoDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SystemInfoDialog)
{
    ui->setupUi(this);

    ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Copy to clipboard"));

    // sentry crash handler test
    new EventFilter(ui->buttons->button(QDialogButtonBox::Ok), { QEvent::MouseButtonPress },
                    [](QObject *, QEvent *e) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::RightButton)
            static_cast<int *>(nullptr)[0] = 1;
        else if (me->button() == Qt::MiddleButton)
            throw Exception("Test exception");
        return EventFilter::ContinueEventProcessing;
    });

    connect(ui->buttons, &QDialogButtonBox::accepted,
            this, [this]() {
        QString text = ui->text->toMarkdown();
        QGuiApplication::clipboard()->setText(text);
    });

    QString text = u"### BrickStore " + QCoreApplication::applicationVersion()
            + u" (build: " + Application::inst()->buildNumber() + u")\n\n";
    auto sysInfo = SystemInfo::inst()->asMap();
    sysInfo.remove(u"os.type"_qs);
    sysInfo.remove(u"os.version"_qs);
    sysInfo.remove(u"hw.memory"_qs);
    sysInfo.remove(u"brickstore.version"_qs);
    sysInfo[u"brickstore.ldraw"_qs] = LDraw::library()->lastUpdated().toString(Qt::RFC2822Date);

    for (auto it = sysInfo.cbegin(); it != sysInfo.cend(); ++it) {
        text = text + u" * **" + it.key() + u"**: " + it.value().toString() + u"\n";
    }

    ui->text->setMarkdown(text);
}

SystemInfoDialog::~SystemInfoDialog()
{
    delete ui;
}

#include "moc_systeminfodialog.cpp"
