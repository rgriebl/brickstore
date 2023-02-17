// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include "common/application.h"
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
