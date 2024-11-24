// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QRegularExpression>

#include "dimensions.h"


namespace BrickLink {

int Dimensions::dimensions() const
{
    return (m_bits.x && m_bits.y && m_bits.z)
               ? 3 : ((m_bits.x && m_bits.y)
                      ? 2 : (m_bits.x
                             ? 1 : 0));
}

bool Dimensions::fuzzyCompare(const Dimensions &other) const
{
    std::array<float, 3> d = { x(), y(), z() };
    std::array<float, 3> od = { other.x(), other.y(), other.z() };

    // ignore the order
    std::sort(d.begin(), d.end());
    std::sort(od.begin(), od.end());

    // only 2 dimension need to match
    int count = od[2] ? 3 : 2;

    // a match needs to be within 0.1
    bool match = true;
    for (int i = 0; i < count; ++i) {
        if (std::abs(od[i] - d[i]) > 0.1f) {
            match = false;
            break;
        }
    }
    return match;
}

class DimensionsPrivate
{
public:
    const QString re_oneNumberFull = uR"((\d+)\s+(\d+)\s*\/\s*([1-9]\d*)|(\d+)\s*\/\s*([1-9]\d*)|(\d+\.\d+)|(\d+))"_qs;
    const QString re_oneNumberRelaxed = [this]() { return QString(re_oneNumberFull).remove(u'(').remove(u')'); }();
    const QString re_oneNumberStrict = [this]() { return QString(re_oneNumberRelaxed).replace(u"\\s+"_qs, u" "_qs).remove(u"\\s*"_qs); }();
    const QString re_xStrict = uR"( x )"_qs;
    const QString re_xRelaxed = uR"(\s*[*x]\s*)"_qs;
    const QString re_start = uR"((?<=^|\s))"_qs;
    const QString re_end = uR"((?=\s|$))"_qs;
    const QString re_strict = u'(' + re_oneNumberStrict + u')' + re_xStrict + u'('
                              + re_oneNumberStrict + u')'+ u"(?:" + re_xStrict + u'('
                              + re_oneNumberStrict + u"))?";
    const QString re_relaxed = u'(' + re_oneNumberRelaxed + u')' + re_xRelaxed + u'('
                               + re_oneNumberRelaxed + u')' + u"(?:" + re_xRelaxed + u'('
                               + re_oneNumberRelaxed + u"))?";

    const QRegularExpression &number() const {
        static const QRegularExpression regex(re_oneNumberFull);
        return regex;
    }
    const QRegularExpression &strict() const {
        static const QRegularExpression regex(re_start + re_strict + re_end);
        return regex;
    }
    const QRegularExpression &relaxed() const {
        static const QRegularExpression regex(re_start + re_relaxed + re_end);
        return regex;
    }
};

Q_GLOBAL_STATIC(DimensionsPrivate, d)

QString Dimensions::detectionRegExp(Strictness strictness)
{
    if (auto dptr = d())
        return (strictness == Strict) ? dptr->re_strict : dptr->re_relaxed;
    return { };
}

Dimensions Dimensions::parseString(const QString &str, qsizetype offset, Strictness strictness)
{
    auto dptr = d();
    if (!dptr)
        return { };
    const auto &regex = (strictness == Strict) ? dptr->strict() : dptr->relaxed();
    auto m = regex.match(str, offset);

    if (m.hasMatch()) {
        Dimensions dim;

        int i = 0;
        for ( ; i < 3; ++i) {
            auto nm = dptr->number().match(m.captured(i + 1));

            if (!nm.hasMatch())
                break;

            float f = 0;

            if (nm.hasCaptured(1) && nm.hasCaptured(2) && nm.hasCaptured(3)) // a b/c
                f = nm.captured(1).toFloat() + nm.captured(2).toFloat() / nm.captured(3).toFloat();
            else if (nm.hasCaptured(4) && nm.hasCaptured(5)) // b/c
                f = nm.captured(4).toFloat() / nm.captured(5).toFloat();
            else if (nm.hasCaptured(6)) // a.x
                f = nm.captured(6).toFloat();
            else if (nm.hasCaptured(7)) // a
                f = nm.captured(7).toFloat();

            switch (i) {
            case 0: dim.m_bits.x = qfloat16(f); break;
            case 1: dim.m_bits.y = qfloat16(f); break;
            case 2: dim.m_bits.z = qfloat16(f); break;
            }
        }

        if (dim.m_bits.x && dim.m_bits.y) {
            // we need to skip any leading and trailing whitespace
            auto pos = m.capturedStart(1);
            auto len = m.capturedEnd(i) - pos;
            constexpr qsizetype uint8max = std::numeric_limits<quint8>::max();

            if ((pos < uint8max) && (len < uint8max)) {
                dim.m_bits.pos = quint8(pos);
                dim.m_bits.len = quint8(len);
                return dim;
            }
        }
    }
    return { };
}

} // namespace BrickLink
