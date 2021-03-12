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
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDebug>

#include "smartvalidator.h"


void DotCommaFilter::install()
{
    static bool once = false;
    if (!once) {
        once = true;
        qApp->installEventFilter(new DotCommaFilter(qApp));
    }
}

DotCommaFilter::DotCommaFilter(QObject *parent)
    : QObject(parent)
{ }

bool DotCommaFilter::eventFilter(QObject *o, QEvent *e)
{
    if ((e->type() == QEvent::KeyPress) || (e->type() == QEvent::KeyRelease)) {
        if (qobject_cast<QDoubleSpinBox *>(o)
                || (qobject_cast<QLineEdit *>(o)
                    && qobject_cast<const QDoubleValidator *>(
                        static_cast<QLineEdit *>(o)->validator()))) {

            auto *ke = static_cast<QKeyEvent *>(e);

            QString text = ke->text();
            bool fixed = false;

            for (int i = 0; i < text.length(); ++i) {
                auto &ir = text[i];
                if (ir == QLatin1Char('.') || ir == QLatin1Char(',')) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    ir = QLocale::system().decimalPoint();
#else
                    ir = QLocale::system().decimalPoint().at(0);
#endif
                    fixed = (text != ke->text());
                }
            }

            if (fixed) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                *ke = QKeyEvent(ke->type(), ke->key(), ke->modifiers(), text,
                                ke->isAutoRepeat(), ushort(ke->count()));
#else
                // send new event, eat this one.
                auto nke = new QKeyEvent(ke->type(), ke->key(), ke->modifiers(), text,
                                         ke->isAutoRepeat(), ushort(ke->count()));
                QCoreApplication::postEvent(o, nke);
                return true;
#endif
            }
        }
    }
    return false;
}


SmartDoubleValidator::SmartDoubleValidator(QObject *parent)
    : QDoubleValidator(parent)
    , m_empty(0)
{
    DotCommaFilter::install();
}

SmartDoubleValidator::SmartDoubleValidator(double bottom, double top, int decimals, double empty, QObject *parent)
    : QDoubleValidator(bottom, top, decimals, parent)
    , m_empty(empty)
{
    DotCommaFilter::install();
}

void SmartDoubleValidator::fixup(QString &str) const
{
    if (str.isEmpty())
        str = locale().toString(m_empty);
}

bool SmartDoubleValidator::event(QEvent *e)
{
    if (e && (e->type() == QEvent::LanguageChange)) {
        setLocale(QLocale());
        emit changed();
    }
    return QDoubleValidator::event(e);
}


SmartIntValidator::SmartIntValidator(QObject *parent)
    : QIntValidator(parent)
    , m_empty(0)
{ }

SmartIntValidator::SmartIntValidator(int bottom, int top, int empty, QObject *parent)
    : QIntValidator(bottom, top, parent)
    , m_empty(empty)
{ }

void SmartIntValidator::fixup(QString &str) const
{
    if (str.isEmpty())
        str = locale().toString(m_empty);
}

bool SmartIntValidator::event(QEvent *e)
{
    if (e && (e->type() == QEvent::LanguageChange)) {
        setLocale(QLocale());
        emit changed();
    }
    return QIntValidator::event(e);
}

#include "moc_smartvalidator.cpp"
