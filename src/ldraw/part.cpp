// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QTextStream>
#include <QDebug>

#include "library.h"
#include "part.h"


namespace LDraw {

template <typename T> static T *parseVectors(const QStringList &list)
{
    std::array<QVector3D, T::PointCount> v;

    for (int i = 0; i < T::PointCount; ++i)
        v[i] = QVector3D(list[3*i + 1].toFloat(), list[3*i + 2].toFloat(), list[3*i + 3].toFloat());
    return T::create(list[0].toInt(), v);
}

std::unique_ptr<Element> Element::fromString(const QString &line, const QString &dir)
{
    std::unique_ptr<Element> e;

    auto list = line.simplified().split(u' ');

    if (!list.isEmpty()) {
        int t = list.at(0).toInt();
        list.removeFirst();

        if (t >= 0 && t <= 5) {
            switch (t) {
            case 0: {
                const QString cmd = line.mid(1).trimmed();
                if (cmd.startsWith(u"PE_TEX_")) // Stud.io textures do not have fallbacks
                    break;
                e = CommentElement::create(cmd);
                break;
            }
            case 1:
                e = PartElement::create(list, dir);
                break;
            case 2:
                e = LineElement::create<LineElement>(list);
                break;
            case 3:
                e = TriangleElement::create<TriangleElement>(list);
                break;
            case 4:
                e = QuadElement::create<QuadElement>(list);
                break;
            case 5:
                e = CondLineElement::create<CondLineElement>(list);
                break;
            }
        }
    }
    return e;
}



CommentElement::CommentElement(Private, const QString &text)
    : CommentElement(Type::Comment, text)
{ }

CommentElement::CommentElement(Element::Type t, const QString &text)
    : Element(t)
    , m_comment(text)
{ }

std::unique_ptr<CommentElement> CommentElement::create(const QString &text)
{
    if (text.startsWith(u"BFC "))
        return BfcCommandElement::create(text);
    else
        return std::make_unique<CommentElement>(Private { }, text);
}


BfcCommandElement::BfcCommandElement(Private, const QString &text)
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

std::unique_ptr<BfcCommandElement> BfcCommandElement::create(const QString &text)
{
    return std::make_unique<BfcCommandElement>(Private { }, text);
}


PartElement::PartElement(Private, int color, const QMatrix4x4 &matrix, Part *p)
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

std::unique_ptr<PartElement> PartElement::create(const QStringList &list, const QString &parentdir)
{
    if (list.size() != 14)
        return { };

    QMatrix4x4 m {
        list[4].toFloat(), list[5].toFloat(), list[6].toFloat(), list[1].toFloat(),
        list[7].toFloat(), list[8].toFloat(), list[9].toFloat(), list[2].toFloat(),
        list[10].toFloat(), list[11].toFloat(), list[12].toFloat(), list[3].toFloat(),
        0, 0, 0, 1
    };
    m.optimize();
    const QString filename = list[13];
    const int color = list[0].toInt();

    if (Part *p = library()->findPart(filename, parentdir))
        return std::make_unique<PartElement>(Private { }, color, m, p);
    return nullptr;
}


std::unique_ptr<Part> Part::parse(const QByteArray &data, const QString &dir)
{
    auto p = std::make_unique<Part>(Private { });
    QTextStream ts(data);

    QString line;
    int lineno = 0;
    while (!ts.atEnd()) {
        line = ts.readLine();
        lineno++;
        if (line.isEmpty())
            continue;
        if (std::unique_ptr<Element> e = Element::fromString(line, dir)) {
            p->m_cost += int(e->size());
            p->m_elements.emplace_back(std::move(e));
        } else {
            qCWarning(LogLDraw) << "Could not parse line" << lineno << ":" << line;
            p.reset();
            return { };
        }
    }

    if (p->m_elements.empty()) {
        p.reset();
        return { };
    }
    return p;
}

QVector<const Element *> Part::elements() const
{
    QVector<const Element *> v { qsizetype(m_elements.size()) };
    int i = 0;
    for (const auto &ue : m_elements)
        v[i++] = ue.get();
    return v;
}

int Part::cost() const
{
    return m_cost;
}

} // namespace LDraw
