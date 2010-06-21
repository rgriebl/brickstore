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
#include <QByteArray>
#include <QVector>
#include <QList>
#include <QColor>
#include <QCache>
#include <QVector3D>
#include <QMatrix4x4>

#include "ref.h"

class QFile;
class QDir;

namespace LDraw {

class Element;
class PartElement;

class Part : public Ref {
public:
    inline const QVector<Element *> &elements() const  { return m_elements; }

    bool boundingBox(QVector3D &vmin, QVector3D &vmax);

    void dump() const;

protected:
    Part();

    static Part *parse(QFile &file, const QDir &dir);
    friend class PartElement;
    friend class Core;

    static void calc_bounding_box(const Part *part, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax);
    static void check_bounding(int cnt, const QVector3D *v, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax);


    QVector<Element *> m_elements;
    bool m_bounding_calculated;
    QVector3D m_bounding_min;
    QVector3D m_bounding_max;
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

    static Element *fromByteArray(const QByteArray &line, const QDir &dir);

    inline Type type() const  { return m_type; }

    virtual ~Element() { };

    virtual void dump() const { };

protected:
    Element(Type t)
        : m_type(t) { }

private:
    Type m_type;
};


class CommentElement : public Element {
public:
    QByteArray comment() const  { return m_comment; }

    static CommentElement *create(const QByteArray &text);

    virtual void dump() const;

protected:
    CommentElement(const QByteArray &);

    QByteArray m_comment;
};


class LineElement : public Element {
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}

    static LineElement *create(int color, const QVector3D *points);

    virtual void dump() const;

protected:
    LineElement(int color, const QVector3D *points);

    int    m_color;
    QVector3D m_points[2];
};


class CondLineElement : public Element {
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}

    static CondLineElement *create(int color, const QVector3D *points);

    virtual void dump() const;

protected:
    CondLineElement(int color, const QVector3D *points);

    int    m_color;
    QVector3D m_points[4];
};


class TriangleElement : public Element {
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}

    static TriangleElement *create(int color, const QVector3D *points);

    virtual void dump() const;

protected:
    TriangleElement(int color, const QVector3D *points);

    int    m_color;
    QVector3D m_points[3];
};


class QuadElement : public Element {
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}

    static QuadElement *create(int color, const QVector3D *points);

    virtual void dump() const;

protected:
    QuadElement(int color, const QVector3D *points);

    int    m_color;
    QVector3D m_points[4];
};


class PartElement : public Element {
public:
    int color() const              { return m_color; }
    const QMatrix4x4 &matrix() const { return m_matrix; }
    LDraw::Part *part() const      { return m_part; }

    static PartElement *create(int color, const QMatrix4x4 &m, const QString &filename, const QDir &parentdir);

    virtual ~PartElement();
    virtual void dump() const;

protected:
    PartElement(int color, const QMatrix4x4 &m, LDraw::Part *part);

    int           m_color;
    QMatrix4x4    m_matrix;
    LDraw::Part * m_part;
};

/*
class Color {
public:
    enum Material {
        Plastic = 0,
        Chrome,
        Pearlescent,
        Rubber,
        MatteMetallic,
        Metal,

        Default = Plastic
    };

private:
    uint     m_id;
    char *   m_name;
    float    m_luminance;
    Material m_material;
    QColor   m_color;
    QColor   m_edgecolor;
};

class Item {
private:
    char *m_id;
    char *m_name;
    char *m_path;
};

*/

class Core {
public:
    QString dataPath() const;

    QColor color(int id, int baseid = -1) const;
    QColor edgeColor(int id) const;

    Part *partFromId(const char *id);
    Part *partFromFile(const QString &filename);

private:
    bool create_part_list();
    bool parse_ldconfig(const char *file);
    QColor parse_color_string(const QString &str);

    Core(const QString &datadir);

    Part *findPart(const QString &filename, const QDir &parentdir);

    static Core *create(const QString &datadir, QString *errstring);
    static inline Core *inst() { return s_inst; }
    static Core *s_inst;

    friend Core *core();
    friend Core *create(const QString &, QString *);

    static bool check_ldrawdir(const QString &dir);
    static QString get_platform_ldrawdir();


private:
    QString m_datadir;
    QList<QDir> m_searchpath;

    QHash<int, QPair<QColor, QColor> > m_colors;  // ldraw color -> [color, edge color]
    QMap<QByteArray, QByteArray>       m_items;   // ldraw id -> ldraw name
    QCache<QString, Part>              m_cache;   // path -> part

    friend class PartElement;
};

inline Core *core() { return Core::inst(); }

inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

} // namespace LDraw

#endif
