// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QString>
#include <QVector>
#include <QColor>
#include <QVector3D>
#include <QMatrix4x4>

#include "utility/ref.h"


namespace LDraw {

class Element;
class PartElement;


class Part : public Ref
{
public:
    ~Part() override;

    inline const QVector<Element *> &elements() const  { return m_elements; }
    int cost() const;

protected:
    Part() = default;

    static Part *parse(const QByteArray &data, const QString &dir);
    friend class PartElement;
    friend class Library;

    static void calculateBoundingBox(const Part *part, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax);

    QVector<Element *> m_elements;
    int m_cost = 0;
};


class Element
{
public:
    enum class Type {
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
    uint size() const override { return int(sizeof(*this)) + uint(m_comment.size() * 2); }

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
    QVector3D m_points[2];
    int       m_color;
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
    QVector3D m_points[4];
    int       m_color;
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
    QVector3D m_points[3];
    int       m_color;
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
    QVector3D m_points[4];
    int       m_color;
    Q_DISABLE_COPY(QuadElement)
};


class PartElement : public Element
{
public:
    int color() const                { return m_color; }
    const QMatrix4x4 &matrix() const { return m_matrix; }
    Part *part() const               { return m_part; }
    uint size() const override       { return sizeof(*this); }

    static PartElement *create(int color, const QMatrix4x4 &m, const QString &filename,
                               const QString &parentdir);

    ~PartElement() override;

protected:
    PartElement(int color, const QMatrix4x4 &m, Part *part);

private:
    Q_DISABLE_COPY(PartElement)
    QMatrix4x4 m_matrix;
    Part *     m_part;
    int        m_color;
};

} // namespace LDraw

// tell Qt that Parts are shared and can't simply be deleted
// (QCache will use that function to determine what can really be purged from the cache)

template<> inline bool q3IsDetached<LDraw::Part>(LDraw::Part &p) { return p.refCount() == 0; }
