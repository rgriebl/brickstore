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

#include "utility.h"
#include "messagebox.h"


class InputDialog : public QInputDialog
{
public:
    InputDialog(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags())
        : QInputDialog(parent, flags)
    { }

    void positionRelativeTo(const QRect &pos)
    {
        m_pos = pos;
    }

protected:
    void showEvent(QShowEvent *e) override;

private:
    QRect m_pos;
};

void InputDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);

    if (m_pos.isValid()) {
        Utility::setPopupPos(this, m_pos);
        m_pos = { };
    }
}


QPointer<QWidget> MessageBox::s_defaultParent;


MessageBox::MessageBox()
    : QMessageBox()
{ }

QString MessageBox::defaultTitle()
{
    return QCoreApplication::applicationName();
}

void MessageBox::setDefaultParent(QWidget *parent)
{
    s_defaultParent = parent;
}

QWidget *MessageBox::defaultParent()
{
    return s_defaultParent.data();
}

QMessageBox::StandardButton MessageBox::msgbox(QWidget *parent, const QString &title, const QString &msg,
                                               QMessageBox::Icon icon, StandardButtons buttons,
                                               StandardButton defaultButton)
{
    if (qobject_cast<QApplication *>(qApp)) {
        QMessageBox *mb = new QMessageBox(icon, !title.isEmpty() ? title : defaultTitle(), msg,
                                          NoButton, parent ? parent : defaultParent());
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

QMessageBox::StandardButton MessageBox::information(QWidget *parent, const QString &title,
                                                    const QString &text, StandardButtons buttons,
                                                    StandardButton defaultButton)
{
    return msgbox(parent, title, text, QMessageBox::Information, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBox::question(QWidget *parent, const QString &title,
                                                 const QString &text, StandardButtons buttons,
                                                 StandardButton defaultButton)
{
    return msgbox(parent, title, text, Question, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBox::warning(QWidget *parent, const QString &title,
                                                const QString &text, StandardButtons buttons,
                                                StandardButton defaultButton)
{
    return msgbox(parent, title, text, Warning, buttons, defaultButton);
}

QMessageBox::StandardButton MessageBox::critical(QWidget *parent, const QString &title,
                                                 const QString &text, StandardButtons buttons,
                                                 StandardButton defaultButton)
{
    return msgbox(parent, title, text, Critical, buttons, defaultButton);
}

bool MessageBox::getString(QWidget *parent, const QString &title, const QString &text,
                           QString &value, const QRect &positionRelativeToRect)
{
    InputDialog dlg(parent ? parent : defaultParent(), Qt::Sheet);
    dlg.positionRelativeTo(positionRelativeToRect);
    dlg.setWindowTitle(!title.isEmpty() ? title : defaultTitle());
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.setLabelText(text);
    dlg.setTextValue(value);

    bool b = (dlg.exec() == QDialog::Accepted);

    value = dlg.textValue();
    return b;
}

bool MessageBox::getDouble(QWidget *parent, const QString &title, const QString &text,
                           const QString &unit, double &value, double minValue, double maxValue,
                           int decimals, const QRect &positionRelativeToRect)
{
    InputDialog dlg(parent ? parent : defaultParent(), Qt::Sheet);
    dlg.positionRelativeTo(positionRelativeToRect);
    dlg.setWindowTitle(!title.isEmpty() ? title : defaultTitle());
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setInputMode(QInputDialog::DoubleInput);
    dlg.setLabelText(text);
    dlg.setDoubleValue(value);
    dlg.setDoubleMinimum(minValue);
    dlg.setDoubleMaximum(maxValue);
    dlg.setDoubleDecimals(decimals);

    if (auto *sp = dlg.findChild<QDoubleSpinBox *>()) {
        if (!unit.isEmpty())
            sp->setSuffix(QLatin1Char(' ') + unit);
        sp->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sp->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
        sp->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        sp->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    }
    bool b = (dlg.exec() == QDialog::Accepted);

    value = dlg.doubleValue();
    return b;
}

bool MessageBox::getInteger(QWidget *parent, const QString &title, const QString &text,
                            const QString &unit, int &value, int minValue, int maxValue,
                            const QRect &positionRelativeToRect)
{
    InputDialog dlg(parent ? parent : defaultParent(), Qt::Sheet);
    dlg.positionRelativeTo(positionRelativeToRect);
    dlg.setWindowTitle(!title.isEmpty() ? title : defaultTitle());
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setInputMode(QInputDialog::IntInput);
    dlg.setLabelText(text);
    dlg.setIntValue(value);
    dlg.setIntMinimum(minValue);
    dlg.setIntMaximum(maxValue);

    if (auto *sp = dlg.findChild<QSpinBox *>()) {
        if (!unit.isEmpty())
            sp->setSuffix(QLatin1Char(' ') + unit);
        sp->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sp->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
        sp->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        sp->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    }
    bool b = (dlg.exec() == QDialog::Accepted);

    value = dlg.intValue();
    return b;
}


#include "moc_messagebox.cpp"
