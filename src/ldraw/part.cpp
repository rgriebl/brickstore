// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


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
                case 0: {
                    const QString cmd = line.mid(1).trimmed();
                    if (cmd.startsWith(u"PE_TEX_")) // Stud.io textures do not have fallbacks
                        break;
                    e = CommentElement::create(cmd);
                    break;
                }
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
            const QString &bfcCommand = c.at(i);

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
    Q_ASSERT(color >= 0);
    memcpy(m_points, v, sizeof(m_points));
}

TriangleElement *TriangleElement::create(int color, const QVector3D *v)
{
    return new TriangleElement(color, v);
}


QuadElement::QuadElement(int color, const QVector3D *v)
    : Element(Type::Quad), m_color(color)
{
    Q_ASSERT(color >= 0);
    memcpy(m_points, v, sizeof(m_points));
}

QuadElement *QuadElement::create(int color, const QVector3D *v)
{
    return new QuadElement(color, v);
}



PartElement::PartElement(int color, const QMatrix4x4 &matrix, Part *p)
    : Element(Type::Part), m_matrix(matrix), m_part(p), m_color(color)
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


Part::~Part()
{
    qDeleteAll(m_elements);
}

Part *Part::parse(const QByteArray &data, const QString &dir)
{
    Part *p = new Part();
    QTextStream ts(data);

    QString line;
    int lineno = 0;
    while (!ts.atEnd()) {
        line = ts.readLine();
        lineno++;
        if (line.isEmpty())
            continue;
        if (Element *e = Element::fromString(line, dir)) {
            p->m_elements.append(e);
            p->m_cost += int(e->size());
        } else {
            qCWarning(LogLDraw) << "Could not parse line" << lineno << ":" << line;
            delete p;
            return nullptr;
        }
    }

    if (p->m_elements.isEmpty()) {
        delete p;
        p = nullptr;
    }
    return p;
}

int Part::cost() const
{
    return m_cost;
}

} // namespace LDraw
