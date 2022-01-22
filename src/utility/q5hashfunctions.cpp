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
#include <qglobal.h>
#include <QtCore/private/qsimd_p.h>
#include <qendian.h>

#include "q5hashfunctions.h"

/*
    The Java's hashing algorithm for strings is a variation of D. J. Bernstein
    hashing algorithm appeared here http://cr.yp.to/cdb/cdb.txt
    and informally known as DJB33XX - DJB's 33 Times Xor.
    Java uses DJB31XA, that is, 31 Times Add.

    The original algorithm was a loop around
        (h << 5) + h ^ c
    (which is indeed h*33 ^ c); it was then changed to
        (h << 5) - h ^ c
    (so h*31^c: DJB31XX), and the XOR changed to a sum:
        (h << 5) - h + c
    (DJB31XA), which can save some assembly instructions.

    Still, we can avoid writing the multiplication as "(h << 5) - h"
    -- the compiler will turn it into a shift and an addition anyway
    (for instance, gcc 4.4 does that even at -O0).
*/

#if QT_COMPILER_SUPPORTS_HERE(SSE4_2)
static inline bool hasFastCrc32()
{
    return qCpuHasFeature(SSE4_2);
}

template <typename Char>
QT_FUNCTION_TARGET(SSE4_2)
static uint crc32(const Char *ptr, size_t len, uint h)
{
    // The CRC32 instructions from Nehalem calculate a 32-bit CRC32 checksum
    const uchar *p = reinterpret_cast<const uchar *>(ptr);
    const uchar *const e = p + (len * sizeof(Char));
#  ifdef Q_PROCESSOR_X86_64
    // The 64-bit instruction still calculates only 32-bit, but without this
    // variable GCC 4.9 still tries to clear the high bits on every loop
    qulonglong h2 = h;

    p += 8;
    for ( ; p <= e; p += 8)
        h2 = _mm_crc32_u64(h2, qFromUnaligned<qlonglong>(p - 8));
    h = h2;
    p -= 8;

    len = e - p;
    if (len & 4) {
        h = _mm_crc32_u32(h, qFromUnaligned<uint>(p));
        p += 4;
    }
#  else
    p += 4;
    for ( ; p <= e; p += 4)
        h = _mm_crc32_u32(h, qFromUnaligned<uint>(p - 4));
    p -= 4;
    len = e - p;
#  endif
    if (len & 2) {
        h = _mm_crc32_u16(h, qFromUnaligned<ushort>(p));
        p += 2;
    }
    if (sizeof(Char) == 1 && len & 1)
        h = _mm_crc32_u8(h, *p);
    return h;
}
#elif defined(__ARM_FEATURE_CRC32)
static inline bool hasFastCrc32()
{
    return qCpuHasFeature(CRC32);
}

template <typename Char>
#if defined(Q_PROCESSOR_ARM_64)
QT_FUNCTION_TARGET(CRC32)
#endif
static uint crc32(const Char *ptr, size_t len, uint h)
{
    // The crc32[whbd] instructions on Aarch64/Aarch32 calculate a 32-bit CRC32 checksum
    const uchar *p = reinterpret_cast<const uchar *>(ptr);
    const uchar *const e = p + (len * sizeof(Char));

#ifndef __ARM_FEATURE_UNALIGNED
    if (Q_UNLIKELY(reinterpret_cast<quintptr>(p) & 7)) {
        if ((sizeof(Char) == 1) && (reinterpret_cast<quintptr>(p) & 1) && (e - p > 0)) {
            h = __crc32b(h, *p);
            ++p;
        }
        if ((reinterpret_cast<quintptr>(p) & 2) && (e >= p + 2)) {
            h = __crc32h(h, *reinterpret_cast<const uint16_t *>(p));
            p += 2;
        }
        if ((reinterpret_cast<quintptr>(p) & 4) && (e >= p + 4)) {
            h = __crc32w(h, *reinterpret_cast<const uint32_t *>(p));
            p += 4;
        }
    }
#endif

    for ( ; p + 8 <= e; p += 8)
        h = __crc32d(h, *reinterpret_cast<const uint64_t *>(p));

    len = e - p;
    if (len == 0)
        return h;
    if (len & 4) {
        h = __crc32w(h, *reinterpret_cast<const uint32_t *>(p));
        p += 4;
    }
    if (len & 2) {
        h = __crc32h(h, *reinterpret_cast<const uint16_t *>(p));
        p += 2;
    }
    if (sizeof(Char) == 1 && len & 1)
        h = __crc32b(h, *p);
    return h;
}
#else
static inline bool hasFastCrc32()
{
    return false;
}

static uint crc32(...)
{
    Q_UNREACHABLE();
    return 0;
}
#endif

static inline uint hash(const uchar *p, size_t len, uint seed) noexcept
{
    uint h = seed;

    if (seed && hasFastCrc32())
        return crc32(p, len, h);

    for (size_t i = 0; i < len; ++i)
        h = 31 * h + p[i];

    return h;
}

uint q5HashBits(const void *p, size_t len, uint seed) noexcept
{
    return hash(static_cast<const uchar*>(p), int(len), seed);
}

static inline uint hash(const QChar *p, size_t len, uint seed) noexcept
{
    uint h = seed;

    if (seed && hasFastCrc32())
        return crc32(p, len, h);

    for (size_t i = 0; i < len; ++i)
        h = 31 * h + p[i].unicode();

    return h;
}

uint q5Hash(const QByteArray &key, uint seed) noexcept
{
    return hash(reinterpret_cast<const uchar *>(key.constData()), size_t(key.size()), seed);
}

#if QT_STRINGVIEW_LEVEL < 2
uint q5Hash(const QString &key, uint seed) noexcept
{
    return hash(key.unicode(), size_t(key.size()), seed);
}

#endif

uint q5Hash(QStringView key, uint seed) noexcept
{
    return hash(key.data(), key.size(), seed);
}
