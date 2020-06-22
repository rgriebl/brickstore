/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>

#include "informationdialog.h"

InformationDialog::InformationDialog(const QString &title, const QMap<QString, QString> &pages, QWidget *parent)
    : QDialog(parent), m_pages(pages)
{
    setupUi(this);
    setWindowTitle(title);

    gotoPage("index");
    connect(w_browser, &QLabel::linkActivated,
            this, &InformationDialog::gotoPage);

    setFixedSize(sizeHint());
}

void InformationDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
    else
        QDialog::changeEvent(e);
}

void InformationDialog::enableOk()
{
    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void InformationDialog::reject()
{
    if (w_buttons->button(QDialogButtonBox::Ok)->isEnabled())
        QDialog::reject();
}

void InformationDialog::closeEvent(QCloseEvent *ce)
{
    ce->setAccepted(w_buttons->button(QDialogButtonBox::Ok)->isEnabled());
}

void InformationDialog::gotoPage(const QString &url)
{
    if (m_pages.contains(url)) {
        w_browser->setText(m_pages [url]);
        // w_browser->document()->setTextWidth(500);
        //w_browser->document()->adjustSize();
//  w_browser->setFixedSize(w_browser->document()->size().toSize());
        layout()->activate();
    }
    else {
        QDesktopServices::openUrl(url);
    }
}

#include "moc_informationdialog.cpp"
