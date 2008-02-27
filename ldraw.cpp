#include <QFile>
#include <QStringList>
#include <QTextStream>

#include "ldraw.h"


LDraw::Element *LDraw::Element::fromString(const QString &line, ParseContext *cache)
{
    Element *e = 0;
    int t = -1;

    static const int element_count_lut[] = {
        -1,
        14,
         7,
        10,
        13,
        13,
    };

    QStringList sl = line.simplified().split(' ');

    if (!sl.isEmpty()) {
        t = sl[0].toInt();
        sl.removeFirst();

        if (t >= 0 && t <= 5) {
            if (!t || element_count_lut[2 * t] == sl.size()) {
                switch (t) {
                case 0: {
                    e = CommentElement::create(sl.join(QLatin1String(" ")));
                    break;
                }
                case 1: {
                    int color = sl[0].toInt();
                    qreal m[16];
                    m[0] = sl[4].toDouble();
                    m[1] = sl[5].toDouble();
                    m[2] = sl[6].toDouble();
                    m[3] = 0;
                    m[4] = sl[7].toDouble();
                    m[5] = sl[8].toDouble();
                    m[6] = sl[9].toDouble();
                    m[7] = 0;
                    m[8] = sl[10].toDouble();
                    m[9] = sl[11].toDouble();
                    m[10] = sl[12].toDouble();
                    m[11] = 0;
                    m[12] = sl[1].toDouble();
                    m[13] = sl[2].toDouble();
                    m[14] = sl[3].toDouble();
                    m[15] = 1;

                    e = PartElement::create(color, m, sl[13], cache);
                    break;
                }
                case 2: {
                    int color = sl[0].toInt();
                    qreal points[6];
                    for (int i = 0; i < 6; i++)
                        points[i] = sl[i+1].toDouble();
                    e = LineElement::create(color, points);
                    break;
                }
                case 3: {
                    int color = sl[0].toInt();
                    qreal points[9];
                    for (int i = 0; i < 9; i++)
                        points[i] = sl[i+1].toDouble();
                    e = TriangleElement::create(color, points);
                    break;
                }
                case 4: {
                    int color = sl[0].toInt();
                    qreal points[12];
                    for (int i = 0; i < 12; i++)
                        points[i] = sl[i+1].toDouble();
                    e = QuadElement::create(color, points);
                    break;
                }
                case 5: {
                    int color = sl[0].toInt();
                    qreal points[12];
                    for (int i = 0; i < 12; i++)
                        points[i] = sl[i+1].toDouble();
                    e = LineElement::create(color, points);
                    break;
                }
                default: break;
                }
            }
        }
    }
    return e;
}



LDraw::CommentElement::CommentElement(const QString &text)
    : Element(Comment), m_comment(text)
{ }

LDraw::CommentElement *LDraw::CommentElement::create(const QString &text)
{
    return new CommentElement(text);
}



LDraw::LineElement::LineElement(int color, const qreal *points)
    : Element(Line), m_color(color)
{
    memcpy(m_points, points, sizeof(m_points));
}

LDraw::LineElement *LDraw::LineElement::create(int color, const qreal *points)
{
    return new LineElement(color, points);
}



LDraw::CondLineElement::CondLineElement(int color, const qreal *points)
    : Element(CondLine), m_color(color)
{
    memcpy(m_points, points, sizeof(m_points));
}

LDraw::CondLineElement *LDraw::CondLineElement::create(int color, const qreal *points)
{
    return new CondLineElement(color, points);
}



LDraw::TriangleElement::TriangleElement(int color, const qreal *points)
    : Element(Triangle), m_color(color)
{
    memcpy(m_points, points, sizeof(m_points));
}

LDraw::TriangleElement *LDraw::TriangleElement::create(int color, const qreal *points)
{
    return new TriangleElement(color, points);
}



LDraw::QuadElement::QuadElement(int color, const qreal *points)
    : Element(Quad), m_color(color)
{
    memcpy(m_points, points, sizeof(m_points));
#if 0
    //fix CCW order
    // getting vectors from points
    a10 = p0 - p1;
    a12 = p2 - p1;
    a13 = p3 - p1;
    // building normals (with cross)
    n0 = a12 * a10;
    n1 = a10 * n0;
    n2 = n0 * a12;
    // checking skalar
    s1=a13.dot(n1);
    s2=a13.dot(n2);
    if (s1 < 0) switch(p1,p2); // switch 1-2
    if (s2 < 0) switch(p2,p3); // switch 2-3
#endif
}

LDraw::QuadElement *LDraw::QuadElement::create(int color, const qreal *points)
{
    return new QuadElement(color, points);
}



LDraw::PartElement::PartElement(int color, const qreal *matrix, const QString &filename)
    : Element(Part), m_color(color), m_file(filename), m_no_partcache(true)
{
    memcpy(m_matrix, matrix, sizeof(m_matrix));
}

LDraw::PartElement::~PartElement()
{
    if (m_part && m_no_partcache)
        delete m_part;
}

LDraw::PartElement *LDraw::PartElement::create(int color, const qreal *matrix, const QString &filename, ParseContext *cache)
{
    PartElement *e = new PartElement(color, matrix, filename);
    e->m_no_partcache = (cache == 0);
    e->m_part = Part::parse(filename, cache);
    if (!e->m_part) {
        delete e;
        e = 0;
    }
    return e;
}



LDraw::Part *LDraw::Part::fromFile(const QString &file, ParseContext *cache)
{
    return parse(file, cache);
}


LDraw::Part *LDraw::Part::parse(const QString &filename, ParseContext *cache)
{
    Part *p = cache ? cache->value(filename) : 0;

    if (!p) {
        QFile f(filename);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            p = new Part();

            QString line;
            while (!ts.atEnd()) {
                line = ts.readLine();
                Element *e = Element::fromString(line, cache);
                if (e)
                    p->m_elements.append(e);
            }

            if (p->m_elements.isEmpty()) {
                delete p;
                p = 0;
            }
        }
        if (p && cache)
            cache->insert(filename, p);

    }
    return p;
}



LDraw::Model::Model()
{ }

LDraw::Model *LDraw::Model::fromFile(const QString &file)
{
    Model *m = new Model();

    m->m_root = Part::parse(file, &m->m_parts);

    if (!m->m_root) {
        delete m;
        m = 0;
    }
    return m;
}

LDraw::Model::~Model()
{
    qDeleteAll(m_parts);
}

QVector<LDraw::Part *> LDraw::Model::parts() const
{
    QVector<Part *> v;
    v.reserve(m_parts.size());
    int idx = 0;
    for (QHashIterator<QString, Part *> it(m_parts); it.hasNext(); )
        v[idx++] = it.next().value();
    return v;
}
