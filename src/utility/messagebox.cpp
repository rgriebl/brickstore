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
#include <QInputDialog>
#include <QDoubleSpinBox>

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
    if (qobject_cast<QApplication *>(qApp)) {
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
    QInputDialog dlg(parent, Qt::Sheet);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.setLabelText(text);
    dlg.setTextValue(value);

    bool b = (dlg.exec() == QDialog::Accepted);

    value = dlg.textValue();
    return b;
}

bool MessageBox::getDouble(QWidget *parent, const QString &text, const QString &unit, double &value, double minValue, double maxValue, int decimals)
{
    QInputDialog dlg(parent, Qt::Sheet);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setInputMode(QInputDialog::DoubleInput);
    dlg.setLabelText(text);
    dlg.setDoubleValue(value);
    dlg.setDoubleMinimum(minValue);
    dlg.setDoubleMaximum(maxValue);
    dlg.setDoubleDecimals(decimals);

    if (!unit.isEmpty()) {
        if (auto *sp = dlg.findChild<QDoubleSpinBox *>())
            sp->setSuffix(QLatin1Char(' ') + unit);
    }
    bool b = (dlg.exec() == QDialog::Accepted);

    value = dlg.doubleValue();
    return b;
}

bool MessageBox::getInteger(QWidget *parent, const QString &text, const QString &unit, int &value, int minValue, int maxValue)
{
    QInputDialog dlg(parent, Qt::Sheet);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setInputMode(QInputDialog::IntInput);
    dlg.setLabelText(text);
    dlg.setIntValue(value);
    dlg.setIntMinimum(minValue);
    dlg.setIntMaximum(maxValue);

    if (!unit.isEmpty()) {
        if (auto *sp = dlg.findChild<QSpinBox *>())
            sp->setSuffix(QLatin1Char(' ') + unit);
    }
    bool b = (dlg.exec() == QDialog::Accepted);

    value = dlg.intValue();
    return b;
}


#include "moc_messagebox.cpp"
