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

#include <QtCore/QMap>
#include <QtGui/QPainter>

#include "bricklink/color.h"
#include "bricklink/core.h"
#include "utility/utility.h"


namespace BrickLink {

QHash<uint, QImage> Color::s_colorImageCache;


QString Color::typeName(TypeFlag t)
{
    static const QMap<TypeFlag, QString> colortypes = {
        { Solid,       "Solid"_l1 },
        { Transparent, "Transparent"_l1 },
        { Glitter,     "Glitter"_l1 },
        { Speckle,     "Speckle"_l1 },
        { Metallic,    "Metallic"_l1 },
        { Chrome,      "Chrome"_l1 },
        { Pearl,       "Pearl"_l1 },
        { Milky,       "Milky"_l1 },
        { Modulex,     "Modulex"_l1 },
        { Satin,       "Satin"_l1 },
    };
    return colortypes.value(t);
}

const QImage Color::image(int w, int h) const
{
    if (w <= 0 || h <= 0)
        return QImage();

    uint key = uint(id() << 22) | uint(w << 11) | uint(h);
    QImage img = s_colorImageCache.value(key);

    if (img.isNull()) {
        QColor c = color();

        img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&img);
        p.setRenderHints(QPainter::Antialiasing);
        QRect r = img.rect();

        QBrush brush;

        if (isGlitter()) {
            brush = QBrush(m_particleColor, Qt::Dense6Pattern);
        } else if (isSpeckle()) {
            brush = QBrush(m_particleColor, Qt::Dense7Pattern);
        } else if (isMetallic()) {
            QColor c2 = Utility::gradientColor(c, Qt::black);

            QLinearGradient gradient(0, 0, 0, r.height());
            gradient.setColorAt(0,   c2);
            gradient.setColorAt(0.3, c);
            gradient.setColorAt(0.7, c);
            gradient.setColorAt(1,   c2);
            brush = gradient;
        } else if (isChrome()) {
            QColor c2 = Utility::gradientColor(c, Qt::black);

            QLinearGradient gradient(0, 0, r.width(), 0);
            gradient.setColorAt(0,   c2);
            gradient.setColorAt(0.3, c);
            gradient.setColorAt(0.7, c);
            gradient.setColorAt(1,   c2);
            brush = gradient;
        }

        if (c.isValid()) {
            p.fillRect(r, c);
            p.fillRect(r, brush);
        } else {
            p.fillRect(0, 0, w, h, Qt::white);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::darkGray);
            p.setBrush(QColor(255, 255, 255, 128));
            p.drawRect(r);
            p.drawLine(0, 0, w, h);
            p.drawLine(0, h, w, 0);
        }

        if (isTransparent()) {
            QLinearGradient gradient(0, 0, r.width(), r.height());
            gradient.setColorAt(0,   Qt::transparent);
            gradient.setColorAt(0.4, Qt::transparent);
            gradient.setColorAt(0.6, QColor(255, 255, 255, 100));
            gradient.setColorAt(1,   QColor(255, 255, 255, 100));

            p.fillRect(r, gradient);
        }
        p.end();

        s_colorImageCache.insert(key, img);
    }
    return img;
}



///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


/*! \qmltype Color
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This value type represents a BrickLink color.

    Each color in the BrickLink catalog is available as a Color object.

    You cannot create Color objects yourself, but you can retrieve a Color object given the
    id via the various BrickLink::color() overloads and BrickLink::colorFromLDrawId().

    See \l https://www.bricklink.com/catalogColors.asp
*/
/*! \qmlproperty bool Color::isNull
    \readonly
    Returns whether this Color is \c null. Since this type is a value wrapper around a C++
    object, we cannot use the normal JavaScript \c null notation.
*/
/*! \qmlproperty int Color::id
    \readonly
    The BrickLink id of this color.
*/
/*! \qmlproperty string Color::name
    \readonly
    The BrickLink name of this color.
*/
/*! \qmlproperty color Color::color
    \readonly
    Returns the RGB value of this BrickLink color as a basic QML color type.
*/
/*! \qmlproperty int Color::ldrawId
    \readonly
    The LDraw id of this color, or \c -1 if there is no match or if a LDraw installation isn't
    available.
*/
/*! \qmlproperty bool Color::solid
    \readonly
    Returns \c true if this color is a solid color, or \c false otherwise.
*/
/*! \qmlproperty bool Color::transparent
    \readonly
    Returns \c true if this color is transparent, or \c false otherwise.
*/
/*! \qmlproperty bool Color::glitter
    \readonly
    Returns \c true if this color is glittery, or \c false otherwise.
*/
/*! \qmlproperty bool Color::speckle
    \readonly
    Returns \c true if this color is speckled, or \c false otherwise.
*/
/*! \qmlproperty bool Color::metallic
    \readonly
    Returns \c true if this color is metallic, or \c false otherwise.
*/
/*! \qmlproperty bool Color::chrome
    \readonly
    Returns \c true if this color is chrome-like, or \c false otherwise.
*/
/*! \qmlproperty bool Color::milky
    \readonly
    Returns \c true if this color is milky, or \c false otherwise.
*/
/*! \qmlproperty bool Color::modulex
    \readonly
    Returns \c true if this color is a Modulex color, or \c false otherwise.
*/
/*! \qmlproperty real Color::popularity
    \readonly
    Returns the popularity of this color, normalized to the range \c{[0 .. 1]}.
    The raw popularity value is derived from summing up the counts of the \e Parts, \e{In Sets},
    \e Wanted and \e{For Sale} columns in the \l{https://www.bricklink.com/catalogColors.asp}
    {BrickLink Color Guide table}.
*/
/*! \qmlmethod image Color::image(int width, int height)
    \readonly
    Returns an image of this color, sized \a width x \a height.
*/

QmlColor::QmlColor(const Color *col)
    : QmlWrapperBase(col)
{ }

QImage QmlColor::image(int width, int height) const
{
    return wrapped->image(width, height);
}

} // namespace BrickLink
