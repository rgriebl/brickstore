// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QtCore/QMap>
#include <QtGui/QPainter>

#include "utility/utility.h"
#include "bricklink/color.h"
#include "bricklink/core.h"


namespace BrickLink {

QHash<uint, QImage> Color::s_colorImageCache;


const QVector<ColorTypeFlag> &Color::allColorTypes()
{
    static QVector<ColorTypeFlag> all;
    if (all.isEmpty()) [[unlikely]] {
        for (int ct = int(ColorTypeFlag::Solid); ct & int(ColorTypeFlag::Mask); ct <<= 1)
            all.append(ColorTypeFlag(ct));  // clazy:exclude=reserve-candidates
    }
    return all;
}

QString Color::typeName(ColorTypeFlag t)
{
    static const QMap<ColorTypeFlag, QString> colortypes = {
        { ColorTypeFlag::Solid,       u"Solid"_qs },
        { ColorTypeFlag::Transparent, u"Transparent"_qs },
        { ColorTypeFlag::Glitter,     u"Glitter"_qs },
        { ColorTypeFlag::Speckle,     u"Speckle"_qs },
        { ColorTypeFlag::Metallic,    u"Metallic"_qs },
        { ColorTypeFlag::Chrome,      u"Chrome"_qs },
        { ColorTypeFlag::Pearl,       u"Pearl"_qs },
        { ColorTypeFlag::Milky,       u"Milky"_qs },
        { ColorTypeFlag::Modulex,     u"Modulex"_qs },
        { ColorTypeFlag::Satin,       u"Satin"_qs },
    };
    return colortypes.value(t);
}

uint Color::index() const
{
    return uint(this - core()->colors().data());
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
