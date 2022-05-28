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

} // namespace BrickLink
