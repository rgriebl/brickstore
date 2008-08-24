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
#include <QList>
#include <QDir>
#include <QColor>

#include "vector_t.h"
#include "matrix_t.h"

namespace LDraw {

class Part;
class Element;
class PartElement;

struct ParseContext {
    QHash<QString, Part *> *m_cache;
    QList<QDir> m_searchpath;
};

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

    inline const QVector<Element *> &elements() const  { return m_elements; }

    bool boundingBox(vector_t &vmin, vector_t &vmax);

    void dump() const;

protected:
    Part();

    static Part *parse(const QString &file, ParseContext *ctx);
    friend class PartElement;
    friend class Model;

    static void calc_bounding_box(const Part *part, const matrix_t &matrix, vector_t &vmin, vector_t &vmax);
    static void check_bounding(int cnt, const vector_t *v, const matrix_t &matrix, vector_t &vmin, vector_t &vmax);
    

    QVector<Element *> m_elements;
    bool m_bounding_calculated;
    vector_t m_bounding_min;
    vector_t m_bounding_max;
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
    QString comment() const  { return m_comment; }

    static CommentElement *create(const QString &text);

    virtual void dump() const;

protected:
    CommentElement(const QString &);

    QString m_comment;
};


class LineElement : public Element {
public:
    int color() const              { return m_color; }
    const vector_t *points() const { return m_points;}

    static LineElement *create(int color, const vector_t *points);

    virtual void dump() const;

protected:
    LineElement(int color, const vector_t *points);

    int    m_color;
    vector_t m_points[2];
};


class CondLineElement : public Element {
public:
    int color() const              { return m_color; }
    const vector_t *points() const { return m_points;}

    static CondLineElement *create(int color, const vector_t *points);

    virtual void dump() const;

protected:
    CondLineElement(int color, const vector_t *points);

    int    m_color;
    vector_t m_points[4];
};


class TriangleElement : public Element {
public:
    int color() const              { return m_color; }
    const vector_t *points() const { return m_points;}

    static TriangleElement *create(int color, const vector_t *points);

    virtual void dump() const;

protected:
    TriangleElement(int color, const vector_t *points);

    int    m_color;
    vector_t m_points[3];
};


class QuadElement : public Element {
public:
    int color() const              { return m_color; }
    const vector_t *points() const { return m_points;}

    static QuadElement *create(int color, const vector_t *points);

    virtual void dump() const;

protected:
    QuadElement(int color, const vector_t *points);

    int    m_color;
    vector_t m_points[4];
};


class PartElement : public Element {
public:
    int color() const              { return m_color; }
    QString fileName() const       { return m_file; }
    const matrix_t &matrix() const { return m_matrix; }
    LDraw::Part *part() const      { return m_part; }

    static PartElement *create(int color, const matrix_t &m, const QString &filename, ParseContext *cache);

    virtual ~PartElement();
    virtual void dump() const;

protected:
    PartElement(int color, const matrix_t &m, const QString &filename);

    int           m_color;
    QString       m_file;
    matrix_t      m_matrix;
    bool          m_no_partcache;
    LDraw::Part * m_part;
};

class Core {
public:
    QString dataPath() const;
    bool setDataPath(const QString &dir);
    
    QColor color(int id, int baseid = -1) const;
    QColor edgeColor(int id) const;
    
private:
    bool create_part_list();
    bool parse_ldconfig(const char *file);
    QColor parse_color_string(const QString &str);

    Core(const QString &datadir);

    static Core *create(const QString &datadir, QString *errstring);
    static inline Core *inst() { return s_inst; }
    static Core *s_inst;

    friend Core *core();
    friend Core *create(const QString &, QString *);
    
    static bool check_ldrawdir(const QString &dir);
    static QString get_platform_ldrawdir();

    
private:
    QString m_datadir;
    
    QHash<int, QPair<QColor, QColor> > m_colors;
    QMap<QByteArray, QByteArray> m_items;
};

inline Core *core() { return Core::inst(); }

inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

} // namespace LDraw

#endif
