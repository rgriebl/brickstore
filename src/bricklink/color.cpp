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

#include "utility/utility.h"
#include "bricklink/color.h"


namespace BrickLink {

QHash<uint, QImage> Color::s_colorImageCache;


QString Color::typeName(TypeFlag t)
{
    static const QMap<TypeFlag, QString> colortypes = {
        { Solid,       u"Solid"_qs },
        { Transparent, u"Transparent"_qs },
        { Glitter,     u"Glitter"_qs },
        { Speckle,     u"Speckle"_qs },
        { Metallic,    u"Metallic"_qs },
        { Chrome,      u"Chrome"_qs },
        { Pearl,       u"Pearl"_qs },
        { Milky,       u"Milky"_qs },
        { Modulex,     u"Modulex"_qs },
        { Satin,       u"Satin"_qs },
    };
    return colortypes.value(t);
}

const QImage Color::sampleImage(int w, int h) const
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
            qreal pw = std::max(1., w / 30.);

            p.fillRect(0, 0, w, h, Qt::white);
            p.setPen({ { Qt::darkGray }, pw });
            p.setBrush(QColor(255, 255, 255, 128));

            QRectF rf = QRectF(r).adjusted(pw / 2, pw / 2, -pw / 2, -pw / 2);
            p.drawRect(rf);
            p.drawLine(rf.topLeft(), rf.bottomRight());
            p.drawLine(rf.topRight(), rf.bottomLeft());
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

} // namespace BrickLink
