/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
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
#pragma once

#include <QHash>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QColor>
#include <QVector3D>
#include <QMatrix4x4>

#include "ref.h"
#include "q3cache.h"

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QDir)

namespace LDraw {

class Element;
class PartElement;


class Part : public Ref
{
public:
    ~Part() override;

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


class Element
{
public:
    enum Type {
        Comment,
        BfcCommand,
        Line,
        Part,
        Triangle,
        Quad,
        CondLine
    };

    static Element *fromByteArray(const QByteArray &line, const QDir &dir);

    inline Type type() const  { return m_type; }

    virtual ~Element() = default;

    virtual void dump() const;

protected:
    Element(Type t)
        : m_type(t) { }

private:
    Q_DISABLE_COPY(Element)
    Type m_type;
};


class CommentElement : public Element
{
public:
    QByteArray comment() const  { return m_comment; }

    static CommentElement *create(const QByteArray &text);

    void dump() const override;

protected:
    CommentElement(Type t, const QByteArray &text);
    CommentElement(const QByteArray &);

    QByteArray m_comment;

private:
    Q_DISABLE_COPY(CommentElement)
};


class BfcCommandElement : public CommentElement
{
public:
    bool certify() const { return m_certify; }
    bool noCertify() const { return m_nocertify; }
    bool clip() const { return m_clip; }
    bool noClip() const { return m_noclip; }
    bool ccw() const { return m_ccw; }
    bool cw() const { return m_cw; }
    bool invertNext() const { return m_invertNext; }

    static BfcCommandElement *create(const QByteArray &text);

protected:
    BfcCommandElement(const QByteArray &);

    bool m_certify = false;
    bool m_nocertify = false;
    bool m_clip = false;
    bool m_noclip = false;
    bool m_ccw = false;
    bool m_cw = false;
    bool m_invertNext = false;

private:
    Q_DISABLE_COPY(BfcCommandElement)
    friend class CommentElement;
};


class LineElement : public Element
{
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}

    static LineElement *create(int color, const QVector3D *points);

    void dump() const override;

protected:
    LineElement(int color, const QVector3D *points);

    int       m_color;
    QVector3D m_points[2];

private:
    Q_DISABLE_COPY(LineElement)
};


class CondLineElement : public Element
{
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}

    static CondLineElement *create(int color, const QVector3D *points);

    void dump() const override;

protected:
    CondLineElement(int color, const QVector3D *points);

    int       m_color;
    QVector3D m_points[4];

private:
    Q_DISABLE_COPY(CondLineElement)
};


class TriangleElement : public Element
{
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}

    static TriangleElement *create(int color, const QVector3D *points);

    void dump() const override;

protected:
    TriangleElement(int color, const QVector3D *points);

    int       m_color;
    QVector3D m_points[3];

private:
    Q_DISABLE_COPY(TriangleElement)
};


class QuadElement : public Element
{
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}

    static QuadElement *create(int color, const QVector3D *points);

    void dump() const override;

protected:
    QuadElement(int color, const QVector3D *points);

    int       m_color;
    QVector3D m_points[4];

private:
    Q_DISABLE_COPY(QuadElement)
};


class PartElement : public Element
{
public:
    int color() const              { return m_color; }
    const QMatrix4x4 &matrix() const { return m_matrix; }
    LDraw::Part *part() const      { return m_part; }

    static PartElement *create(int color, const QMatrix4x4 &m, const QString &filename, const QDir &parentdir);

    ~PartElement() override;
    void dump() const override;

protected:
    PartElement(int color, const QMatrix4x4 &m, LDraw::Part *part);

    int          m_color;
    QMatrix4x4   m_matrix;
    LDraw::Part *m_part;

private:
    Q_DISABLE_COPY(PartElement)
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

class Core
{
public:
    ~Core();

    QString dataPath() const;

    QColor color(int id, int baseid = -1) const;
    QColor edgeColor(int id) const;

    Part *partFromId(const QString &id);
    Part *partFromFile(const QString &filename);

    static QStringList potentialDrawDirs();
    static bool isValidLDrawDir(const QString &dir);

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

private:
    QString m_datadir;
    QVector<QDir> m_searchpath;

    struct Color
    {
        int id;
        QString name;
        QColor color;
        QColor edgeColor;
        int luminance;
        bool chrome : 1;
        bool metal : 1;
        bool matteMetallic : 1;
        bool rubber : 1;
        bool pearlescent : 1;
    };

    QHash<int, Color> m_colors;  // id -> color struct
    Q3Cache<QString, Part> m_cache;  // path -> part

    friend class PartElement;
};

inline Core *core() { return Core::inst(); }

inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

} // namespace LDraw

// tell Qt that Parts are shared and can't simply be deleted
// (QCache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<LDraw::Part>(LDraw::Part &p) { return p.refCount() == 0; }
