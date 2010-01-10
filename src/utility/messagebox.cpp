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
#include <climits>
#include <cfloat>

#include <QApplication>
#include <QValidator>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QLayout>
#include <QDialog>
#include <QTextDocument>
#include <QByteArray>

#include "messagebox.h"


MessageBox::MessageBox()
    : QMessageBox()
{ }

QString MessageBox::s_deftitle;

void MessageBox::setDefaultTitle(const QString &s)
{
    s_deftitle = s;
}

QString MessageBox::defaultTitle()
{
    return s_deftitle;
}

QMessageBox::StandardButton MessageBox::msgbox(QWidget *parent, const QString &msg, QMessageBox::Icon icon, StandardButtons buttons, StandardButton defaultButton)
{
    if (qApp && qApp->type() != QApplication::Tty) {
        QMessageBox *mb = new QMessageBox(icon, s_deftitle, msg, NoButton, parent);
        mb->setAttribute(Qt::WA_DeleteOnClose);
        mb->setObjectName("messagebox");
        mb->setStandardButtons(buttons);
        mb->setDefaultButton(defaultButton);
        mb->setTextFormat(Qt::RichText);

        return static_cast<StandardButton>(mb->exec());
    }
    else {
        QByteArray out;
        if (msg.contains(QLatin1Char('<'))) {
            QTextDocument doc;
            doc.setHtml(msg);
            out = doc.toPlainText().toLocal8Bit();
        }
        else
            out = msg.toLocal8Bit();

        if (icon == Critical)
            qCritical("%s", out.constData());
        else
            qWarning("%s", out.constData());

        return defaultButton;
    }
}

QMessageBox::StandardButton MessageBox::information(QWidget *parent, const QString &text, StandardButtons buttons, StandardButton defaultButton)
{
    return msgbox(parent, text, QMessageBox::Information, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBox::question(QWidget *parent, const QString &text, StandardButtons buttons, StandardButton defaultButton)
{
    return msgbox(parent, text, Question, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBox::warning(QWidget *parent, const QString &text, StandardButtons buttons, StandardButton defaultButton)
{
    return msgbox(parent, text, Warning, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBox::critical(QWidget *parent, const QString &text, StandardButtons buttons, StandardButton defaultButton)
{
    return msgbox(parent, text, Critical, buttons, defaultButton);
}

bool MessageBox::getString(QWidget *parent, const QString &text, QString &value)
{
    return getString(parent, text, QString::null, value, 0);
}

bool MessageBox::getDouble(QWidget *parent, const QString &text, const QString &unit, double &value, QValidator *validate)
{
    QString str_value = QString::number(value);
    bool b = getString(parent, text, unit, str_value, validate ? validate : new QDoubleValidator(-DBL_MAX, DBL_MAX, 3, 0));

    if (b)
        value = str_value.toDouble();
    return b;
}

bool MessageBox::getInteger(QWidget *parent, const QString &text, const QString &unit, int &value, QValidator *validate)
{
    QString str_value = QString::number(value);
    bool b = getString(parent, text, unit, str_value, validate ? validate : new QIntValidator(INT_MIN, INT_MAX, 0));

    if (b)
        value = str_value.toInt();
    return b;
}

bool MessageBox::getString(QWidget *parent, const QString &text, const QString &unit, QString &value, QValidator *validate)
{
    bool b = false;

    QDialog *d = new QDialog(parent);
    d->setObjectName("getbox");
    d->setWindowTitle(s_deftitle);

    QLabel *wlabel = new QLabel(text, d);
    wlabel->setTextFormat(Qt::RichText);
    QLabel *wunit = unit.isEmpty() ? 0 : new QLabel(unit, d);
    QFrame *wline = new QFrame(d);
    wline->setFrameStyle(QFrame::HLine | QFrame::Sunken);

    QLineEdit *wedit = new QLineEdit(value, d);
    wedit->setValidator(validate);

    QFontMetrics wefm(wedit->font());
    wedit->setMinimumWidth(20 + qMax(wefm.width("Aa0") * 3, wefm.width(value)));
    wedit->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));

    QPushButton *wok = new QPushButton(tr("&OK"), d);
    wok->setAutoDefault(true);
    wok->setDefault(true);

    QPushButton *wcancel = new QPushButton(tr("&Cancel"), d);
    wcancel->setAutoDefault(true);

    QBoxLayout *toplay = new QVBoxLayout(d);    //, 11, 6 );
    toplay->addWidget(wlabel);

    QBoxLayout *midlay = new QHBoxLayout();
    toplay->addLayout(midlay);
    toplay->addWidget(wline);

    QBoxLayout *botlay = new QHBoxLayout();
    toplay->addLayout(botlay);

    midlay->addWidget(wedit);
    if (wunit) {
        midlay->addWidget(wunit);
        midlay->addStretch();
    }

    botlay->addSpacing(QFontMetrics(d->font()).width("Aa0") * 6);
    botlay->addStretch();
    botlay->addWidget(wok);
    botlay->addWidget(wcancel);

    connect(wedit, SIGNAL(returnPressed()), wok, SLOT(animateClick()));
    connect(wok, SIGNAL(clicked()), d, SLOT(accept()));
    connect(wcancel, SIGNAL(clicked()), d, SLOT(reject()));

    d->setSizeGripEnabled(true);
    d->setMinimumSize(d->minimumSizeHint());
    d->resize(d->sizeHint());

    if (d->exec() == QDialog::Accepted) {
        b = true;
        value = wedit->text();
    }

    delete d;
    delete validate;
    return b;
}
