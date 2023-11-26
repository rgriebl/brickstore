// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QFloat16>
#include <QString>

namespace BrickLink {

class DimensionsPrivate;

class Dimensions
{
public:
    enum Strictness
    {
        Strict,
        Relaxed,
    };

    inline bool isValid() const { return m_bits.x && m_bits.y; }

    int dimensions() const;

    inline float x() const { return m_bits.x; }
    inline float y() const { return m_bits.y; }
    inline float z() const { return m_bits.z; }

    inline qsizetype offset() const { return qsizetype(m_bits.pos); }
    inline qsizetype length() const { return qsizetype(m_bits.len); }

    bool operator==(const Dimensions &other) const { return m_data == other.m_data; }
    bool fuzzyCompare(const Dimensions &other) const;

    static QString detectionRegExp(Strictness strictness);
    static Dimensions parseString(const QString &str, qsizetype offset, Strictness strictness);

private:
    union {
        struct {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            qfloat16 x;
            qfloat16 y;
            qfloat16 z;
            quint8   pos = 0;
            quint8   len = 0;
#else
            quint8   len = 0;
            quint8   pos = 0;
            qfloat16 z;
            qfloat16 y;
            qfloat16 x;
#endif
        } m_bits = { };
        quint64 m_data;
    };
};

Q_STATIC_ASSERT(sizeof(Dimensions) == 8);

} // namespace BrickLink
