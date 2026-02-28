// Copyright (C) 2004-2026 Robert Griebl
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


class Part final : public Ref
{
    struct Private { };
public:
    Part(Private) { };
    ~Part() override = default;

    QVector<const Element *> elements() const;
    int cost() const;

    static std::unique_ptr<Part> parse(const QByteArray &data, const QString &dir);

private:

    friend class PartElement;
    friend class Library;

    static void calculateBoundingBox(const Part *part, const QMatrix4x4 &matrix, QVector3D &vmin, QVector3D &vmax);

    std::vector<std::unique_ptr<Element>> m_elements;
    int m_cost = 0;

    Q_DISABLE_COPY_MOVE(Part)
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

    static std::unique_ptr<Element> fromString(const QString &line, const QString &dir);
    inline Type type() const  { return m_type; }
    virtual ~Element() = default;
    virtual uint size() const = 0;

protected:
    Element(Type t)
        : m_type(t) { }

private:
    Q_DISABLE_COPY_MOVE(Element)
    Type m_type;
};


class CommentElement : public Element
{
    struct Private { };
public:
    QString comment() const  { return m_comment; }
    uint size() const override { return int(sizeof(*this)) + uint(m_comment.size() * 2); }

    static std::unique_ptr<CommentElement> create(const QString &text);

    CommentElement(Private, const QString &text);
    ~CommentElement() override = default;

protected:
    CommentElement(Type t, const QString &text);

private:
    Q_DISABLE_COPY_MOVE(CommentElement)
    QString m_comment;
};


class BfcCommandElement : public CommentElement
{
    struct Private { };
public:
    bool certify() const { return m_certify; }
    bool noCertify() const { return m_nocertify; }
    bool clip() const { return m_clip; }
    bool noClip() const { return m_noclip; }
    bool ccw() const { return m_ccw; }
    bool cw() const { return m_cw; }
    bool invertNext() const { return m_invertNext; }

    static std::unique_ptr<BfcCommandElement> create(const QString &text);

    BfcCommandElement(Private, const QString &);
    ~BfcCommandElement() override = default;

private:
    Q_DISABLE_COPY_MOVE(BfcCommandElement)
    bool m_certify    : 1 = false;
    bool m_nocertify  : 1 = false;
    bool m_clip       : 1 = false;
    bool m_noclip     : 1 = false;
    bool m_ccw        : 1 = false;
    bool m_cw         : 1 = false;
    bool m_invertNext : 1 = false;
    friend class CommentElement;
};


template<Element::Type ELEMENT_TYPE, int POINT_COUNT>
class PointElement : public Element
{
    struct Private { };
public:
    static constexpr int PointCount = POINT_COUNT;

    int color() const               { return m_color; }
    uint size() const override      { return sizeof(*this); }
    const std::array<QVector3D, POINT_COUNT> &points() const { return m_points;}

    template <typename T> static std::unique_ptr<T> create(const QStringList &list)
    {
        std::array<QVector3D, T::PointCount> v;

        if (list.size() != (1 + 3 * T::PointCount))
            return nullptr;

        for (int i = 0; i < T::PointCount; ++i)
            v[i] = QVector3D(list[3*i + 1].toFloat(), list[3*i + 2].toFloat(), list[3*i + 3].toFloat());
        return std::make_unique<T>(Private { }, list[0].toInt(), v);
    }

    PointElement(Private, int color, const std::array<QVector3D, POINT_COUNT> &points)
        : Element(ELEMENT_TYPE)
        , m_points(points)
        , m_color(color)
    { }

    ~PointElement() override = default;

private:
    Q_DISABLE_COPY_MOVE(PointElement)
    std::array<QVector3D, POINT_COUNT> m_points { };
    int m_color = 0;
};

using LineElement = PointElement<Element::Type::Line, 2>;
using CondLineElement = PointElement<Element::Type::CondLine, 4>;
using TriangleElement = PointElement<Element::Type::Triangle, 3>;
using QuadElement = PointElement<Element::Type::Quad, 4>;

class PartElement : public Element
{
    struct Private { };
public:
    int color() const                { return m_color; }
    const QMatrix4x4 &matrix() const { return m_matrix; }
    Part *part() const               { return m_part; }
    uint size() const override       { return sizeof(*this); }

    static std::unique_ptr<PartElement> create(const QStringList &list, const QString &parentdir);

    PartElement(Private, int color, const QMatrix4x4 &m, Part *part);
    ~PartElement() override;

private:
    Q_DISABLE_COPY_MOVE(PartElement)
    QMatrix4x4 m_matrix;
    Part *     m_part = nullptr;
    int        m_color = 0;
};

} // namespace LDraw
