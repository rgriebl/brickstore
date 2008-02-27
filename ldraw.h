/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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
#ifndef __LDRAW_H__
#define __LDRAW_H__

#include <QHash>
#include <QString>
#include <QVector>

namespace LDraw {

class Part;
class Element;
class PartElement;
typedef QHash<QString, Part *> ParseContext;


class Model {
public:
    static Model *fromFile(const QString &file);

    virtual ~Model();

    Part *root() const  { return m_root; }
    QVector<Part *> parts() const;

protected:
    Model();

    Part *m_root;
    QHash<QString, Part *> m_parts;
};


class Part {
public:
    static Part *fromFile(const QString &file, ParseContext *cache = 0);

    const QVector<Element *> &elements() const  { return m_elements; }

protected:
    static Part *parse(const QString &file, ParseContext *cache);
    friend class PartElement;
    friend class Model;

    QVector<Element *> m_elements;
};


class Element {
public:
    enum Type {
        Comment,
        Line,
        Part,
        Triangle,
        Quad,
        CondLine
    };

    static Element *fromString(const QString &line, ParseContext *cache = 0);

    Type type() const  { return m_type; }

    virtual ~Element() { };

protected:
    Element(Type t)
        : m_type(t) { }

private:
    Type m_type;
};


class CommentElement : public Element {
public:
    QString comment() const  { return m_comment; }

    static CommentElement *create(const QString &text);

protected:
    CommentElement(const QString &);

    QString m_comment;
};


class LineElement : public Element {
public:
    int color() const            { return m_color; }
    const qreal *points() const  { return m_points;}

    static LineElement *create(int color, const qreal *points);

protected:
    LineElement(int color, const qreal *points);

    int   m_color;
    qreal m_points[6];
};


class CondLineElement : public Element {
public:
    int color() const            { return m_color; }
    const qreal *points() const  { return m_points;}

    static CondLineElement *create(int color, const qreal *points);

protected:
    CondLineElement(int color, const qreal *points);

    int   m_color;
    qreal m_points[12];
};


class TriangleElement : public Element {
public:
    int color() const            { return m_color; }
    const qreal *points() const  { return m_points;}

    static TriangleElement *create(int color, const qreal *points);

protected:
    TriangleElement(int color, const qreal *points);

    int   m_color;
    qreal m_points[9];
};


class QuadElement : public Element {
public:
    int color() const            { return m_color; }
    const qreal *points() const  { return m_points;}

    static QuadElement *create(int color, const qreal *points);

protected:
    QuadElement(int color, const qreal *points);

    int   m_color;
    qreal m_points[12];
};


class PartElement : public Element {
public:
    int color() const            { return m_color; }
    QString fileName() const     { return m_file; }
    const qreal *matrix() const  { return m_matrix; }

    static PartElement *create(int color, const qreal *matrix, const QString &filename, ParseContext *cache);

    virtual ~PartElement();

protected:
    PartElement(int color, const qreal *matrix, const QString &filename);

    int     m_color;
    QString m_file;
    qreal   m_matrix[16];
    bool    m_no_partcache;
    LDraw::Part *m_part;
};

} // namespace LDraw

#endif
