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
#pragma once

#include <QHash>
#include <QDate>
#include <QString>
#include <QByteArray>
#include <QLoggingCategory>
#include <QVector>
#include <QColor>
#include <QVector3D>
#include <QMatrix4x4>

#include "utility/ref.h"
#include "utility/q3cache.h"

QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QDir)

Q_DECLARE_LOGGING_CATEGORY(LogLDraw)

class MiniZip;

namespace LDraw {

class Element;
class PartElement;


class Part : public Ref
{
public:
    ~Part() override;

    inline const QVector<Element *> &elements() const  { return m_elements; }

    bool boundingBox(QVector3D &vmin, QVector3D &vmax);
    uint cost() const;

protected:
    Part();

    static Part *parse(const QByteArray &data, const QString &dir);
    friend class PartElement;
    friend class Core;

    static void calculateBoundingBox(const Part *part, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax);

    QVector<Element *> m_elements;
    bool m_boundingCalculated;
    QVector3D m_boundingMin;
    QVector3D m_boundingMax;
    uint m_cost = 0;
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

    static Element *fromString(const QString &line, const QString &dir);
    inline Type type() const  { return m_type; }
    virtual ~Element() = default;
    virtual uint size() const = 0;

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
    QString comment() const  { return m_comment; }
    uint size() const override { return sizeof(*this) + m_comment.size() * 2; }

    static CommentElement *create(const QString &text);

protected:
    CommentElement(Type t, const QString &text);
    CommentElement(const QString &);

private:
    Q_DISABLE_COPY(CommentElement)
    QString m_comment;
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

    static BfcCommandElement *create(const QString &text);

protected:
    BfcCommandElement(const QString &);

private:
    Q_DISABLE_COPY(BfcCommandElement)
    bool m_certify    : 1 = false;
    bool m_nocertify  : 1 = false;
    bool m_clip       : 1 = false;
    bool m_noclip     : 1 = false;
    bool m_ccw        : 1 = false;
    bool m_cw         : 1 = false;
    bool m_invertNext : 1 = false;
    friend class CommentElement;
};


class LineElement : public Element
{
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}
    uint size() const override      { return sizeof(*this); }

    static LineElement *create(int color, const QVector3D *points);

protected:
    LineElement(int color, const QVector3D *points);

private:
    Q_DISABLE_COPY(LineElement)
    int       m_color;
    QVector3D m_points[2];
};


class CondLineElement : public Element
{
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}
    uint size() const override      { return sizeof(*this); }

    static CondLineElement *create(int color, const QVector3D *points);

protected:
    CondLineElement(int color, const QVector3D *points);

private:
    Q_DISABLE_COPY(CondLineElement)
    int       m_color;
    QVector3D m_points[4];
};


class TriangleElement : public Element
{
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}
    uint size() const override      { return sizeof(*this); }

    static TriangleElement *create(int color, const QVector3D *points);

protected:
    TriangleElement(int color, const QVector3D *points);

private:
    Q_DISABLE_COPY(TriangleElement)
    int       m_color;
    QVector3D m_points[3];
};


class QuadElement : public Element
{
public:
    int color() const               { return m_color; }
    const QVector3D *points() const { return m_points;}
    uint size() const override      { return sizeof(*this); }

    static QuadElement *create(int color, const QVector3D *points);

protected:
    QuadElement(int color, const QVector3D *points);

private:
    int       m_color;
    QVector3D m_points[4];
    Q_DISABLE_COPY(QuadElement)
};


class PartElement : public Element
{
public:
    int color() const              { return m_color; }
    const QMatrix4x4 &matrix() const { return m_matrix; }
    LDraw::Part *part() const      { return m_part; }
    uint size() const override      { return sizeof(*this); }

    static PartElement *create(int color, const QMatrix4x4 &m, const QString &filename,
                               const QString &parentdir);

    ~PartElement() override;

protected:
    PartElement(int color, const QMatrix4x4 &m, LDraw::Part *part);

private:
    Q_DISABLE_COPY(PartElement)
    int          m_color;
    QMatrix4x4   m_matrix;
    LDraw::Part *m_part;
};


class Core
{
public:
    ~Core();

    QString dataPath() const;
    QDate lastUpdate() const;

    QColor color(int id, int baseid = -1) const;
    QColor edgeColor(int id) const;

    Part *partFromId(const QByteArray &id);
    Part *partFromFile(const QString &filename);

    static QStringList potentialDrawDirs();
    static bool isValidLDrawDir(const QString &dir);

private:
    bool parseLDconfig(const char *file);
    static QColor parseColorString(const QString &str);

    Core(const QString &datadir);

    Part *findPart(const QString &filename, const QString &parentdir = { });

    static Core *create(const QString &datadir, QString *errstring);
    static inline Core *inst() { return s_inst; }
    static Core *s_inst;

    friend Core *core();
    friend Core *create(const QString &, QString *);

private:
    QString m_datadir;
    MiniZip *m_zip = nullptr;
    QStringList m_searchpath;

    struct Color
    {
        QString name;
        QColor color;
        QColor edgeColor;
        int id;
        int luminance;
        bool chrome : 1;
        bool metal : 1;
        bool matteMetallic : 1;
        bool rubber : 1;
        bool pearlescent : 1;
    };

    QHash<int, Color> m_colors;  // id -> color struct
    Q3Cache<QString, Part> m_cache;  // path -> part
    QDate m_date;

    friend class PartElement;
    QByteArray readLDrawFile(const QString &filename);
};

inline Core *core() { return Core::inst(); }

inline Core *create(const QString &datadir, QString *errstring) { return Core::create(datadir, errstring); }

} // namespace LDraw

// tell Qt that Parts are shared and can't simply be deleted
// (QCache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<LDraw::Part>(LDraw::Part &p) { return p.refCount() == 0; }
