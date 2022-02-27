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

#include <limits>

#include <QTextStream>
#include <QDebug>

#include "library.h"
#include "part.h"


namespace LDraw {

Element *Element::fromString(const QString &line, const QString &dir)
{
    Element *e = nullptr;

    static auto parseVectors = []<typename T, const int N>(const QStringList &list) {
        QVector3D v[N];

        for (int i = 0; i < N; ++i)
            v[i] = QVector3D(list[3*i + 1].toFloat(), list[3*i + 2].toFloat(), list[3*i + 3].toFloat());
        return T::create(list[0].toInt(), v);
    };

    static const int element_count_lut[] = {
         0,
        14,
         7,
        10,
        13,
        13,
    };

    auto list = line.simplified().split(u' ');

    if (!list.isEmpty()) {
        int t = list.at(0).toInt();
        list.removeFirst();

        if (t >= 0 && t <= 5) {
            int count = element_count_lut[t];
            if ((count == 0) || (list.size() == count)) {
                switch (t) {
                case 0:
                    e = CommentElement::create(line.mid(1).trimmed());
                    break;

                case 1: {
                    QMatrix4x4 m {
                        list[4].toFloat(), list[5].toFloat(), list[6].toFloat(), list[1].toFloat(),
                        list[7].toFloat(), list[8].toFloat(), list[9].toFloat(), list[2].toFloat(),
                        list[10].toFloat(), list[11].toFloat(), list[12].toFloat(), list[3].toFloat(),
                        0, 0, 0, 1
                    };
                    m.optimize();
                    e = PartElement::create(list[0].toInt(), m, list[13], dir);
                    break;
                }
                case 2:
                    e = parseVectors.template operator()<LineElement, 2>(list);
                    break;
                case 3:
                    e = parseVectors.template operator()<TriangleElement, 3>(list);
                    break;
                case 4:
                    e = parseVectors.template operator()<QuadElement, 4>(list);
                    break;
                case 5:
                    e = parseVectors.template operator()<CondLineElement, 4>(list);
                    break;
                }
            }
        }
    }
    return e;
}



CommentElement::CommentElement(const QString &text)
    : CommentElement(Type::Comment, text)
{ }

CommentElement::CommentElement(Element::Type t, const QString &text)
    : Element(t)
    , m_comment(text)
{ }

CommentElement *CommentElement::create(const QString &text)
{
    if (text.startsWith(u"BFC "))
        return new BfcCommandElement(text);
    else
        return new CommentElement(text);
}


BfcCommandElement::BfcCommandElement(const QString &text)
    : CommentElement(Type::BfcCommand, text)
{
    auto c = text.split(u' ');
    if ((c.count() >= 2) && (c.at(0) == u"BFC")) {
        for (int i = 1; i < c.length(); ++i) {
            QString bfcCommand = c.at(i);

            if (bfcCommand == u"INVERTNEXT")
                m_invertNext = true;
            else if (bfcCommand == u"CW")
                m_cw = true;
            else if (bfcCommand == u"CCW")
                m_ccw = true;
        }
    }
}

BfcCommandElement *BfcCommandElement::create(const QString &text)
{
    return new BfcCommandElement(text);
}


LineElement::LineElement(int color, const QVector3D *v)
    : Element(Type::Line), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

LineElement *LineElement::create(int color, const QVector3D *v)
{
    return new LineElement(color, v);
}


CondLineElement::CondLineElement(int color, const QVector3D *v)
    : Element(Type::CondLine), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

CondLineElement *CondLineElement::create(int color, const QVector3D *v)
{
    return new CondLineElement(color, v);
}


TriangleElement::TriangleElement(int color, const QVector3D *v)
    : Element(Type::Triangle), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

TriangleElement *TriangleElement::create(int color, const QVector3D *v)
{
    return new TriangleElement(color, v);
}


QuadElement::QuadElement(int color, const QVector3D *v)
    : Element(Type::Quad), m_color(color)
{
    memcpy(m_points, v, sizeof(m_points));
}

QuadElement *QuadElement::create(int color, const QVector3D *v)
{
    return new QuadElement(color, v);
}



PartElement::PartElement(int color, const QMatrix4x4 &matrix, Part *p)
    : Element(Type::Part), m_color(color), m_matrix(matrix), m_part(p)
{
    if (m_part)
        m_part->addRef();
}

PartElement::~PartElement()
{
    if (m_part)
        m_part->release();
}

PartElement *PartElement::create(int color, const QMatrix4x4 &matrix,
                                 const QString &filename, const QString &parentdir)
{
    PartElement *e = nullptr;
    if (Part *p = library()->findPart(filename, parentdir))
        e = new PartElement(color, matrix, p);
    return e;
}


Part::Part()
    : m_boundingCalculated(false)
{
}

Part::~Part()
{
    qDeleteAll(m_elements);
}

Part *Part::parse(const QByteArray &data, const QString &dir)
{
    Part *p = new Part();
    QTextStream ts(data);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ts.setCodec("UTF-8");
#endif

    QString line;
    int lineno = 0;
    while (!ts.atEnd()) {
        line = ts.readLine();
        lineno++;
        if (line.isEmpty())
            continue;
        if (Element *e = Element::fromString(line, dir)) {
            p->m_elements.append(e);
            p->m_cost += e->size();
        } else {
            //qCWarning(LogLDraw) << "Could not parse line" << lineno << ":" << line;
        }
    }

    if (p->m_elements.isEmpty()) {
        delete p;
        p = nullptr;
    }
    return p;
}

bool Part::boundingBox(QVector3D &vmin, QVector3D &vmax)
{
    if (!m_boundingCalculated) {
        QMatrix4x4 matrix;
        static constexpr auto fmin = std::numeric_limits<float>::min();
        static constexpr auto fmax = std::numeric_limits<float>::max();

        m_boundingMin = QVector3D(fmax, fmax, fmax);
        m_boundingMax = QVector3D(fmin, fmin, fmin);

        calculateBoundingBox(this, matrix, m_boundingMin, m_boundingMax);
        m_boundingCalculated = true;
    }
    vmin = m_boundingMin;
    vmax = m_boundingMax;
    return true;
}

uint Part::cost() const
{
    return m_cost;
}


void Part::calculateBoundingBox(const Part *part, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax)
{
    auto extend = [&matrix, &vmin, &vmax](int cnt, const QVector3D *v) {
        while (cnt--) {
            QVector3D vm = matrix.map(*v++);
            vmin = QVector3D(qMin(vmin.x(), vm.x()), qMin(vmin.y(), vm.y()), qMin(vmin.z(), vm.z()));
            vmax = QVector3D(qMax(vmax.x(), vm.x()), qMax(vmax.y(), vm.y()), qMax(vmax.z(), vm.z()));
        }
    };

    const auto elements = part->elements();
    for (const Element *e : elements) {
        switch (e->type()) {
        case Element::Type::Line:
            extend(2, static_cast<const LineElement *>(e)->points());
            break;

        case Element::Type::Triangle:
            extend(3, static_cast<const TriangleElement *>(e)->points());
            break;

        case Element::Type::Quad:
            extend(4, static_cast<const QuadElement *>(e)->points());
            break;

        case Element::Type::Part: {
            const auto *pe = static_cast<const PartElement *>(e);
            calculateBoundingBox(pe->part(), matrix * pe->matrix(), vmin, vmax);
            break;
        }
        default:
            break;
        }
    }
}

} // namespace LDraw
