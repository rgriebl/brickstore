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

#include "smartvalidator.h"

class DotCommaFilter : public QObject
{
public:
    static void install()
    {
        static bool once = false;
        if (!once) {
            once = true;
            qApp->installEventFilter(new DotCommaFilter(qApp));
        }
    }

private:
    explicit DotCommaFilter(QObject *parent)
        : QObject(parent)
    { }

protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (((e->type() == QEvent::KeyPress) || (e->type() == QEvent::KeyRelease)) && qobject_cast<QLineEdit *>(o)) {
            const auto *val = qobject_cast<const SmartDoubleValidator *>(static_cast<QLineEdit *>(o)->validator());

            if (val) {
                auto *ke = static_cast<QKeyEvent *>(e);

                QString text = ke->text();
                bool fixed = false;

                for (int i = 0; i < text.length(); ++i) {
                    QCharRef ir = text[i];
                    if (ir == QLatin1Char('.') || ir == QLatin1Char(',')) {
                        ir = val->locale().decimalPoint();
                        fixed = true;
                    }
                }

                if (fixed)
                    *ke = QKeyEvent(ke->type(), ke->key(), ke->modifiers(), text, ke->isAutoRepeat(), ushort(ke->count()));
            }
        }
        return false;
    }
};


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
