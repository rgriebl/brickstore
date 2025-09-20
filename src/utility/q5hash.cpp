/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
** Copyright (C) 2012 Giuseppe D'Angelo <dangelog@gmail.com>.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

/*
 See q3cache.h on why we need this copy of Qt 5.15's Q5Hash.
*/

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wexpansion-to-defined"
#endif

// for rand_s, _CRT_RAND_S must be #defined before #including stdlib.h.
// put it at the beginning so some indirect inclusion doesn't break it
#ifndef _CRT_RAND_S
#define _CRT_RAND_S
#endif
#include <stdlib.h>

#include "q5hash.h"

#ifdef truncate
#undef truncate
#endif

#include <qbitarray.h>
#include <qstring.h>
#include <qglobal.h>
#include <qbytearray.h>
#include <qdatetime.h>
#include <qbasicatomic.h>
#include <qendian.h>
#include <private/qsimd_p.h>

#ifndef QT_BOOTSTRAPPED
#include <qcoreapplication.h>
#include <qrandom.h>
#endif // QT_BOOTSTRAPPED

#include <limits.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#  if defined(__ARM_FEATURE_CRC32)
#    include <arm_acle.h>
#  endif
#endif

QT_BEGIN_NAMESPACE

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

/*!
    \internal

    Creates the Q5Hash random seed from various sources.
    In order of decreasing precedence:
    - under Unix, it attemps to read from /dev/urandom;
    - under Unix, it attemps to read from /dev/random;
    - under Windows, it attempts to use rand_s;
    - as a general fallback, the application's PID, a timestamp and the
      address of a stack-local variable are used.
*/
static uint qt_create_q5hash_seed()
{
    uint seed = 0;

#ifndef QT_BOOTSTRAPPED
    QByteArray envSeed = qgetenv("QT_HASH_SEED");
    if (!envSeed.isNull()) {
        uint _seed = envSeed.toUInt();
        if (_seed) {
            // can't use qWarning here (reentrancy)
            fprintf(stderr, "QT_HASH_SEED: forced seed value is not 0, cannot guarantee that the "
                     "hashing functions will produce a stable value.");
        }
        return _seed;
    }

    seed = QRandomGenerator::system()->generate();
#endif // QT_BOOTSTRAPPED

    return seed;
}

/*
    The Q5Hash seed itself.
*/
static QBasicAtomicInt qt_q5hash_seed = Q_BASIC_ATOMIC_INITIALIZER(-1);

/*!
    \internal

    Seed == -1 means it that it was not initialized yet.

    We let qt_create_q5hash_seed return any unsigned integer,
    but convert it to signed in order to initialize the seed.

    We don't actually care about the fact that different calls to
    qt_create_q5hash_seed() might return different values,
    as long as in the end everyone uses the very same value.
*/
static void qt_initialize_q5hash_seed()
{
    if (qt_q5hash_seed.loadRelaxed() == -1) {
        int x(qt_create_q5hash_seed() & INT_MAX);
        qt_q5hash_seed.testAndSetRelaxed(-1, x);
    }
}

/*! \relates Q5Hash
    \since 5.6

    Returns the current global Q5Hash seed.

    The seed is set in any newly created Q5Hash. See \l{q5Hash} about how this seed
    is being used by Q5Hash.

    \sa qSetGlobalQ5HashSeed
 */
int qGlobalQ5HashSeed()
{
    qt_initialize_q5hash_seed();
    return qt_q5hash_seed.loadRelaxed();
}

/*! \relates Q5Hash
    \since 5.6

    Sets the global Q5Hash seed to \a newSeed.

    Manually setting the global Q5Hash seed value should be done only for testing
    and debugging purposes, when deterministic and reproducible behavior on a Q5Hash
    is needed. We discourage to do it in production code as it can make your
    application susceptible to \l{algorithmic complexity attacks}.

    From Qt 5.10 and onwards, the only allowed values are 0 and -1. Passing the
    value -1 will reinitialize the global Q5Hash seed to a random value, while
    the value of 0 is used to request a stable algorithm for C++ primitive
    types types (like \c int) and string types (QString, QByteArray).

    The seed is set in any newly created Q5Hash. See \l{q5Hash} about how this seed
    is being used by Q5Hash.

    If the environment variable \c QT_HASH_SEED is set, calling this function will
    result in a no-op.

    \sa qGlobalQ5HashSeed
 */
void qSetGlobalQ5HashSeed(int newSeed)
{
    if (qEnvironmentVariableIsSet("QT_HASH_SEED"))
        return;
    if (newSeed == -1) {
        int x(qt_create_q5hash_seed() & INT_MAX);
        qt_q5hash_seed.storeRelaxed(x);
    } else {
        if (newSeed) {
            // can't use qWarning here (reentrancy)
            fprintf(stderr, "qSetGlobalQ5HashSeed: forced seed value is not 0, cannot guarantee that the "
                            "hashing functions will produce a stable value.");
        }
        qt_q5hash_seed.storeRelaxed(newSeed & INT_MAX);
    }
}

/*!
    \internal

    Private copy of the implementation of the Qt 4 q5Hash algorithm for strings,
    (that is, QChar-based arrays, so all QString-like classes),
    to be used wherever the result is somehow stored or reused across multiple
    Qt versions. The public q5Hash implementation can change at any time,
    therefore one must not rely on the fact that it will always give the same
    results.

    The qt_hash functions must *never* change their results.

    This function can hash discontiguous memory by invoking it on each chunk,
    passing the previous's result in the next call's \a chained argument.
*/
uint qt5_hash(QStringView key, uint chained) noexcept
{
    auto n = key.size();
    auto p = key.utf16();

    uint h = chained;

    while (n--) {
        h = (h << 4) + *p++;
        h ^= (h & 0xf0000000) >> 23;
        h &= 0x0fffffff;
    }
    return h;
}

/*
    The prime_deltas array contains the difference between a power
    of two and the next prime number:

    prime_deltas[i] = nextprime(2^i) - 2^i

    Basically, it's sequence A092131 from OEIS, assuming:
    - nextprime(1) = 1
    - nextprime(2) = 2
    and
    - left-extending it for the offset 0 (A092131 starts at i=1)
    - stopping the sequence at i = 28 (the table is big enough...)
*/

static const uchar prime_deltas[] = {
    0,  0,  1,  3,  1,  5,  3,  3,  1,  9,  7,  5,  3, 17, 27,  3,
    1, 29,  3, 21,  7, 17, 15,  9, 43, 35, 15,  0,  0,  0,  0,  0
};

/*
    The primeForNumBits() function returns the prime associated to a
    power of two. For example, primeForNumBits(8) returns 257.
*/

static inline int primeForNumBits(int numBits)
{
    return (1 << numBits) + prime_deltas[numBits];
}

/*
    Returns the smallest integer n such that
    primeForNumBits(n) >= hint.
*/
static int countBits(int hint)
{
    int numBits = 0;
    int bits = hint;

    while (bits > 1) {
        bits >>= 1;
        numBits++;
    }

    if (numBits >= (int)sizeof(prime_deltas)) {
        numBits = sizeof(prime_deltas) - 1;
    } else if (primeForNumBits(numBits) < hint) {
        ++numBits;
    }
    return numBits;
}

/*
    A Q5Hash has initially around pow(2, MinNumBits) buckets. For
    example, if MinNumBits is 4, it has 17 buckets.
*/
const int MinNumBits = 4;

const Q5HashData Q5HashData::shared_null = {
    nullptr, nullptr, Q_REFCOUNT_INITIALIZE_STATIC, 0, 0, MinNumBits, 0, 0, 0, true, false, 0
};

void *Q5HashData::allocateNode(int nodeAlign)
{
    void *ptr = strictAlignment ? qMallocAligned(nodeSize, nodeAlign) : malloc(nodeSize);
    Q_CHECK_PTR(ptr);
    return ptr;
}

void Q5HashData::freeNode(void *node)
{
    if (strictAlignment)
        qFreeAligned(node);
    else
        free(node);
}

Q5HashData *Q5HashData::detach_helper(void (*node_duplicate)(Node *, void *),
                                    void (*node_delete)(Node *),
                                    int _nodeSize,
                                    int nodeAlign)
{
    union {
        Q5HashData *d;
        Node *e;
    };
    if (this == &shared_null)
        qt_initialize_q5hash_seed(); // may throw
    d = new Q5HashData;
    d->fakeNext = nullptr;
    d->buckets = nullptr;
    d->ref.initializeOwned();
    d->size = size;
    d->nodeSize = _nodeSize;
    d->userNumBits = userNumBits;
    d->numBits = numBits;
    d->numBuckets = numBuckets;
    d->seed = (this == &shared_null) ? uint(qt_q5hash_seed.loadRelaxed()) : seed;
    d->sharable = true;
    d->strictAlignment = nodeAlign > 8;
    d->reserved = 0;

    if (numBuckets) {
        QT_TRY {
            d->buckets = new Node *[numBuckets];
        } QT_CATCH(...) {
            // restore a consistent state for d
            d->numBuckets = 0;
            // roll back
            d->free_helper(node_delete);
            QT_RETHROW;
        }

        Node *this_e = reinterpret_cast<Node *>(this);
        for (int i = 0; i < numBuckets; ++i) {
            Node **nextNode = &d->buckets[i];
            Node *oldNode = buckets[i];
            while (oldNode != this_e) {
                QT_TRY {
                    Node *dup = static_cast<Node *>(allocateNode(nodeAlign));

                    QT_TRY {
                        node_duplicate(oldNode, dup);
                    } QT_CATCH(...) {
                        freeNode( dup );
                        QT_RETHROW;
                    }

                    *nextNode = dup;
                    nextNode = &dup->next;
                    oldNode = oldNode->next;
                } QT_CATCH(...) {
                    // restore a consistent state for d
                    *nextNode = e;
                    d->numBuckets = i+1;
                    // roll back
                    d->free_helper(node_delete);
                    QT_RETHROW;
                }
            }
            *nextNode = e;
        }
    }
    return d;
}

void Q5HashData::free_helper(void (*node_delete)(Node *))
{
    if (node_delete) {
        Node *this_e = reinterpret_cast<Node *>(this);
        Node **bucket = reinterpret_cast<Node **>(this->buckets);

        int n = numBuckets;
        while (n--) {
            Node *cur = *bucket++;
            while (cur != this_e) {
                Node *next = cur->next;
                node_delete(cur);
                freeNode(cur);
                cur = next;
            }
        }
    }
    delete [] buckets;
    delete this;
}

Q5HashData::Node *Q5HashData::nextNode(Node *node)
{
    union {
        Node *next;
        Node *e;
        Q5HashData *d;
    };
    next = node->next;
    Q_ASSERT_X(next, "Q5Hash", "Iterating beyond end()");
    if (next->next)
        return next;

    int start = (node->h % d->numBuckets) + 1;
    Node **bucket = d->buckets + start;
    int n = d->numBuckets - start;
    while (n--) {
        if (*bucket != e)
            return *bucket;
        ++bucket;
    }
    return e;
}

Q5HashData::Node *Q5HashData::previousNode(Node *node)
{
    union {
        Node *e;
        Q5HashData *d;
    };

    e = node;
    while (e->next)
        e = e->next;

    int start;
    if (node == e)
        start = d->numBuckets - 1;
    else
        start = node->h % d->numBuckets;

    Node *sentinel = node;
    Node **bucket = d->buckets + start;
    while (start >= 0) {
        if (*bucket != sentinel) {
            Node *prev = *bucket;
            while (prev->next != sentinel)
                prev = prev->next;
            return prev;
        }

        sentinel = e;
        --bucket;
        --start;
    }
    Q_ASSERT_X(start >= 0, "Q5Hash", "Iterating backward beyond begin()");
    return e;
}

/*
    If hint is negative, -hint gives the approximate number of
    buckets that should be used for the hash table. If hint is
    nonnegative, (1 << hint) gives the approximate number
    of buckets that should be used.
*/
void Q5HashData::rehash(int hint)
{
    if (hint < 0) {
        hint = countBits(-hint);
        if (hint < MinNumBits)
            hint = MinNumBits;
        userNumBits = hint;
        while (primeForNumBits(hint) < (size >> 1))
            ++hint;
    } else if (hint < MinNumBits) {
        hint = MinNumBits;
    }

    if (numBits != hint) {
        Node *e = reinterpret_cast<Node *>(this);
        Node **oldBuckets = buckets;
        int oldNumBuckets = numBuckets;

        int nb = primeForNumBits(hint);
        buckets = new Node *[nb];
        numBits = hint;
        numBuckets = nb;
        for (int i = 0; i < numBuckets; ++i)
            buckets[i] = e;

        for (int i = 0; i < oldNumBuckets; ++i) {
            Node *firstNode = oldBuckets[i];
            while (firstNode != e) {
                uint h = firstNode->h;
                Node *lastNode = firstNode;
                while (lastNode->next != e && lastNode->next->h == h)
                    lastNode = lastNode->next;

                Node *afterLastNode = lastNode->next;
                Node **beforeFirstNode = &buckets[h % numBuckets];
                while (*beforeFirstNode != e)
                    beforeFirstNode = &(*beforeFirstNode)->next;
                lastNode->next = *beforeFirstNode;
                *beforeFirstNode = firstNode;
                firstNode = afterLastNode;
            }
        }
        delete [] oldBuckets;
    }
}

#ifdef QT_QHASH_DEBUG

void Q5HashData::dump()
{
    qDebug("Hash data (ref = %d, size = %d, nodeSize = %d, userNumBits = %d, numBits = %d, numBuckets = %d)",
            int(ref), size, nodeSize, userNumBits, numBits,
            numBuckets);
    qDebug("    %p (fakeNode = %p)", this, fakeNext);
    for (int i = 0; i < numBuckets; ++i) {
        Node *n = buckets[i];
        if (n != reinterpret_cast<Node *>(this)) {
            QString line = QString::asprintf("%d:", i);
            while (n != reinterpret_cast<Node *>(this)) {
                line += QString::asprintf(" -> [%p]", n);
                if (!n) {
                    line += " (CORRUPT)";
                    break;
                }
                n = n->next;
            }
            qDebug("%ls", qUtf16Printable(line));
        }
    }
}

void Q5HashData::checkSanity()
{
    if (Q_UNLIKELY(fakeNext))
        qFatal("Fake next isn't 0");

    for (int i = 0; i < numBuckets; ++i) {
        Node *n = buckets[i];
        Node *p = n;
        if (Q_UNLIKELY(!n))
            qFatal("%d: Bucket entry is 0", i);
        if (n != reinterpret_cast<Node *>(this)) {
            while (n != reinterpret_cast<Node *>(this)) {
                if (Q_UNLIKELY(!n->next))
                    qFatal("%d: Next of %p is 0, should be %p", i, n, this);
                n = n->next;
            }
        }
    }
}
#endif

/*!
    \fn template <typename T1, typename T2> uint q5Hash(const QPair<T1, T2> &key, uint seed = 0)
    \since 5.0
    \relates Q5Hash

    Returns the hash value for the \a key, using \a seed to seed the calculation.

    Types \c T1 and \c T2 must be supported by q5Hash().
*/

/*!
    \fn template <typename T1, typename T2> uint q5Hash(const std::pair<T1, T2> &key, uint seed = 0)
    \since 5.7
    \relates Q5Hash

    Returns the hash value for the \a key, using \a seed to seed the calculation.

    Types \c T1 and \c T2 must be supported by q5Hash().

    \note The return type of this function is \e{not} the same as that of
    \snippet code/src_corelib_tools_qhash.cpp 29
    The two functions use different hashing algorithms; due to binary compatibility
    constraints, we cannot change the QPair algorithm to match the std::pair one before Qt 6.
*/

/*! \fn template <typename InputIterator> uint q5HashRange(InputIterator first, InputIterator last, uint seed = 0)
    \relates Q5Hash
    \since 5.5

    Returns the hash value for the range [\a{first},\a{last}), using \a seed
    to seed the calculation, by successively applying q5Hash() to each
    element and combining the hash values into a single one.

    The return value of this function depends on the order of elements
    in the range. That means that

    \snippet code/src_corelib_tools_qhash.cpp 30

    and
    \snippet code/src_corelib_tools_qhash.cpp 31

    hash to \b{different} values. If order does not matter, for example for hash
    tables, use q5HashRangeCommutative() instead. If you are hashing raw
    memory, use q5HashBits().

    Use this function only to implement q5Hash() for your own custom
    types. For example, here's how you could implement a q5Hash() overload for
    std::vector<int>:

    \snippet code/src_corelib_tools_qhash.cpp qhashrange

    It bears repeating that the implementation of q5HashRange() - like
    the q5Hash() overloads offered by Qt - may change at any time. You
    \b{must not} rely on the fact that q5HashRange() will give the same
    results (for the same inputs) across different Qt versions, even
    if q5Hash() for the element type would.

    \sa q5HashBits(), q5HashRangeCommutative()
*/

/*! \fn template <typename InputIterator> uint q5HashRangeCommutative(InputIterator first, InputIterator last, uint seed = 0)
    \relates Q5Hash
    \since 5.5

    Returns the hash value for the range [\a{first},\a{last}), using \a seed
    to seed the calculation, by successively applying q5Hash() to each
    element and combining the hash values into a single one.

    The return value of this function does not depend on the order of
    elements in the range. That means that

    \snippet code/src_corelib_tools_qhash.cpp 30

    and
    \snippet code/src_corelib_tools_qhash.cpp 31

    hash to the \b{same} values. If order matters, for example, for vectors
    and arrays, use q5HashRange() instead. If you are hashing raw
    memory, use q5HashBits().

    Use this function only to implement q5Hash() for your own custom
    types. For example, here's how you could implement a q5Hash() overload for
    std::unordered_set<int>:

    \snippet code/src_corelib_tools_qhash.cpp qhashrangecommutative

    It bears repeating that the implementation of
    q5HashRangeCommutative() - like the q5Hash() overloads offered by Qt
    - may change at any time. You \b{must not} rely on the fact that
    q5HashRangeCommutative() will give the same results (for the same
    inputs) across different Qt versions, even if q5Hash() for the
    element type would.

    \sa q5HashBits(), q5HashRange()
*/

/*! \fn uint q5HashBits(const void *p, size_t len, uint seed = 0)
    \relates Q5Hash
    \since 5.4

    Returns the hash value for the memory block of size \a len pointed
    to by \a p, using \a seed to seed the calculation.

    Use this function only to implement q5Hash() for your own custom
    types. For example, here's how you could implement a q5Hash() overload for
    std::vector<int>:

    \snippet code/src_corelib_tools_qhash.cpp qhashbits

    This takes advantage of the fact that std::vector lays out its data
    contiguously. If that is not the case, or the contained type has
    padding, you should use q5HashRange() instead.

    It bears repeating that the implementation of q5HashBits() - like
    the q5Hash() overloads offered by Qt - may change at any time. You
    \b{must not} rely on the fact that q5HashBits() will give the same
    results (for the same inputs) across different Qt versions.

    \sa q5HashRange(), q5HashRangeCommutative()
*/

/*! \fn uint q5Hash(char key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(uchar key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(signed char key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(ushort key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(short key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(uint key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(int key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(ulong key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(long key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(quint64 key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(qint64 key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \relates Q5Hash
    \since 5.3

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/
uint q5Hash(float key, uint seed) noexcept
{
    return key != 0.0f ? hash(reinterpret_cast<const uchar *>(&key), sizeof(key), seed) : seed ;
}

/*! \relates Q5Hash
    \since 5.3

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/
uint q5Hash(double key, uint seed) noexcept
{
    return key != 0.0  ? hash(reinterpret_cast<const uchar *>(&key), sizeof(key), seed) : seed ;
}

#if !defined(Q_OS_DARWIN) || defined(Q_CLANG_QDOC)
/*! \relates Q5Hash
    \since 5.3

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/
uint q5Hash(long double key, uint seed) noexcept
{
    return key != 0.0L ? hash(reinterpret_cast<const uchar *>(&key), sizeof(key), seed) : seed ;
}
#endif

/*! \fn uint q5Hash(const QChar key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(const QByteArray &key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(const QBitArray &key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(const QString &key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(const QStringRef &key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(QStringView key, uint seed = 0)
    \relates QStringView
    \since 5.10

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn uint q5Hash(QLatin1String key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*! \fn template <class T> uint q5Hash(const T *key, uint seed = 0)
    \relates Q5Hash
    \since 5.0

    Returns the hash value for the \a key, using \a seed to seed the calculation.
*/

/*!
    \class Q5Hash
    \inmodule QtCore
    \brief The Q5Hash class is a template class that provides a hash-table-based dictionary.

    \ingroup tools
    \ingroup shared

    \reentrant

    Q5Hash\<Key, T\> is one of Qt's generic \l{container classes}. It
    stores (key, value) pairs and provides very fast lookup of the
    value associated with a key.

    Q5Hash provides very similar functionality to QMap. The
    differences are:

    \list
    \li Q5Hash provides faster lookups than QMap. (See \l{Algorithmic
       Complexity} for details.)
    \li When iterating over a QMap, the items are always sorted by
       key. With Q5Hash, the items are arbitrarily ordered.
    \li The key type of a QMap must provide operator<(). The key
       type of a Q5Hash must provide operator==() and a global
       hash function called q5Hash() (see \l{q5Hash}).
    \endlist

    Here's an example Q5Hash with QString keys and \c int values:
    \snippet code/src_corelib_tools_qhash.cpp 0

    To insert a (key, value) pair into the hash, you can use operator[]():

    \snippet code/src_corelib_tools_qhash.cpp 1

    This inserts the following three (key, value) pairs into the
    Q5Hash: ("one", 1), ("three", 3), and ("seven", 7). Another way to
    insert items into the hash is to use insert():

    \snippet code/src_corelib_tools_qhash.cpp 2

    To look up a value, use operator[]() or value():

    \snippet code/src_corelib_tools_qhash.cpp 3

    If there is no item with the specified key in the hash, these
    functions return a \l{default-constructed value}.

    If you want to check whether the hash contains a particular key,
    use contains():

    \snippet code/src_corelib_tools_qhash.cpp 4

    There is also a value() overload that uses its second argument as
    a default value if there is no item with the specified key:

    \snippet code/src_corelib_tools_qhash.cpp 5

    In general, we recommend that you use contains() and value()
    rather than operator[]() for looking up a key in a hash. The
    reason is that operator[]() silently inserts an item into the
    hash if no item exists with the same key (unless the hash is
    const). For example, the following code snippet will create 1000
    items in memory:

    \snippet code/src_corelib_tools_qhash.cpp 6

    To avoid this problem, replace \c hash[i] with \c hash.value(i)
    in the code above.

    Internally, Q5Hash uses a hash table to perform lookups. This
    hash table automatically grows and shrinks to
    provide fast lookups without wasting too much memory. You can
    still control the size of the hash table by calling reserve() if
    you already know approximately how many items the Q5Hash will
    contain, but this isn't necessary to obtain good performance. You
    can also call capacity() to retrieve the hash table's size.

    If you want to navigate through all the (key, value) pairs stored
    in a Q5Hash, you can use an iterator. Q5Hash provides both
    \l{Java-style iterators} (Q5HashIterator and Q5MutableHashIterator)
    and \l{STL-style iterators} (Q5Hash::const_iterator and
    Q5Hash::iterator). Here's how to iterate over a Q5Hash<QString,
    int> using a Java-style iterator:

    \snippet code/src_corelib_tools_qhash.cpp 7

    Here's the same code, but using an STL-style iterator:

    \snippet code/src_corelib_tools_qhash.cpp 8

    Q5Hash is unordered, so an iterator's sequence cannot be assumed
    to be predictable. If ordering by key is required, use a QMap.

    Normally, a Q5Hash allows only one value per key. If you call
    insert() with a key that already exists in the Q5Hash, the
    previous value is erased. For example:

    \snippet code/src_corelib_tools_qhash.cpp 9

    If you only need to extract the values from a hash (not the keys),
    you can also use \l{foreach}:

    \snippet code/src_corelib_tools_qhash.cpp 12

    Items can be removed from the hash in several ways. One way is to
    call remove(); this will remove any item with the given key.
    Another way is to use Q5MutableHashIterator::remove(). In addition,
    you can clear the entire hash using clear().

    Q5Hash's key and value data types must be \l{assignable data
    types}. You cannot, for example, store a QWidget as a value;
    instead, store a QWidget *.

    \target q5Hash
    \section2 The q5Hash() hashing function

    A Q5Hash's key type has additional requirements other than being an
    assignable data type: it must provide operator==(), and there must also be
    a q5Hash() function in the type's namespace that returns a hash value for an
    argument of the key's type.

    The q5Hash() function computes a numeric value based on a key. It
    can use any algorithm imaginable, as long as it always returns
    the same value if given the same argument. In other words, if
    \c{e1 == e2}, then \c{q5Hash(e1) == q5Hash(e2)} must hold as well.
    However, to obtain good performance, the q5Hash() function should
    attempt to return different hash values for different keys to the
    largest extent possible.

    For a key type \c{K}, the q5Hash function must have one of these signatures:

    \snippet code/src_corelib_tools_qhash.cpp 32

    The two-arguments overloads take an unsigned integer that should be used to
    seed the calculation of the hash function. This seed is provided by Q5Hash
    in order to prevent a family of \l{algorithmic complexity attacks}. If both
    a one-argument and a two-arguments overload are defined for a key type,
    the latter is used by Q5Hash (note that you can simply define a
    two-arguments version, and use a default value for the seed parameter).

    Here's a partial list of the C++ and Qt types that can serve as keys in a
    Q5Hash: any integer type (char, unsigned long, etc.), any pointer type,
    QChar, QString, and QByteArray. For all of these, the \c <Q5Hash> header
    defines a q5Hash() function that computes an adequate hash value. Many other
    Qt classes also declare a q5Hash overload for their type; please refer to
    the documentation of each class.

    If you want to use other types as the key, make sure that you provide
    operator==() and a q5Hash() implementation.

    Example:
    \snippet code/src_corelib_tools_qhash.cpp 13

    In the example above, we've relied on Qt's global q5Hash(const
    QString &, uint) to give us a hash value for the employee's name, and
    XOR'ed this with the day they were born to help produce unique
    hashes for people with the same name.

    Note that the implementation of the q5Hash() overloads offered by Qt
    may change at any time. You \b{must not} rely on the fact that q5Hash()
    will give the same results (for the same inputs) across different Qt
    versions.

    \section2 Algorithmic complexity attacks

    All hash tables are vulnerable to a particular class of denial of service
    attacks, in which the attacker carefully pre-computes a set of different
    keys that are going to be hashed in the same bucket of a hash table (or
    even have the very same hash value). The attack aims at getting the
    worst-case algorithmic behavior (O(n) instead of amortized O(1), see
    \l{Algorithmic Complexity} for the details) when the data is fed into the
    table.

    In order to avoid this worst-case behavior, the calculation of the hash
    value done by q5Hash() can be salted by a random seed, that nullifies the
    attack's extent. This seed is automatically generated by Q5Hash once per
    process, and then passed by Q5Hash as the second argument of the
    two-arguments overload of the q5Hash() function.

    This randomization of Q5Hash is enabled by default. Even though programs
    should never depend on a particular Q5Hash ordering, there may be situations
    where you temporarily need deterministic behavior, for example for debugging or
    regression testing. To disable the randomization, define the environment
    variable \c QT_HASH_SEED to have the value 0. Alternatively, you can call
    the qSetGlobalQ5HashSeed() function with the value 0.

    \sa Q5HashIterator, Q5MutableHashIterator, QMap, QSet
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::Q5Hash()

    Constructs an empty hash.

    \sa clear()
*/

/*!
    \fn template <class Key, class T> Q5Hash<Key, T>::Q5Hash(Q5Hash &&other)

    Move-constructs a Q5Hash instance, making it point at the same
    object that \a other was pointing to.

    \since 5.2
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::Q5Hash(std::initializer_list<std::pair<Key,T> > list)
    \since 5.1

    Constructs a hash with a copy of each of the elements in the
    initializer list \a list.

    This function is only available if the program is being
    compiled in C++11 mode.
*/

/*! \fn template <class Key, class T> template <class InputIterator> Q5Hash<Key, T>::Q5Hash(InputIterator begin, InputIterator end)
    \since 5.14

    Constructs a hash with a copy of each of the elements in the iterator range
    [\a begin, \a end). Either the elements iterated by the range must be
    objects with \c{first} and \c{second} data members (like \c{QPair},
    \c{std::pair}, etc.) convertible to \c Key and to \c T respectively; or the
    iterators must have \c{key()} and \c{value()} member functions, returning a
    key convertible to \c Key and a value convertible to \c T respectively.
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::Q5Hash(const Q5Hash &other)

    Constructs a copy of \a other.

    This operation occurs in \l{constant time}, because Q5Hash is
    \l{implicitly shared}. This makes returning a Q5Hash from a
    function very fast. If a shared instance is modified, it will be
    copied (copy-on-write), and this takes \l{linear time}.

    \sa operator=()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::~Q5Hash()

    Destroys the hash. References to the values in the hash and all
    iterators of this hash become invalid.
*/

/*! \fn template <class Key, class T> Q5Hash &Q5Hash<Key, T>::operator=(const Q5Hash &other)

    Assigns \a other to this hash and returns a reference to this hash.
*/

/*!
    \fn template <class Key, class T> Q5Hash &Q5Hash<Key, T>::operator=(Q5Hash &&other)

    Move-assigns \a other to this Q5Hash instance.

    \since 5.2
*/

/*! \fn template <class Key, class T> void Q5Hash<Key, T>::swap(Q5Hash &other)
    \since 4.8

    Swaps hash \a other with this hash. This operation is very
    fast and never fails.
*/

/*! \fn template <class Key, class T> void Q5MultiHash<Key, T>::swap(Q5MultiHash &other)
    \since 4.8

    Swaps hash \a other with this hash. This operation is very
    fast and never fails.
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::operator==(const Q5Hash &other) const

    Returns \c true if \a other is equal to this hash; otherwise returns
    false.

    Two hashes are considered equal if they contain the same (key,
    value) pairs.

    This function requires the value type to implement \c operator==().

    \sa operator!=()
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::operator!=(const Q5Hash &other) const

    Returns \c true if \a other is not equal to this hash; otherwise
    returns \c false.

    Two hashes are considered equal if they contain the same (key,
    value) pairs.

    This function requires the value type to implement \c operator==().

    \sa operator==()
*/

/*! \fn template <class Key, class T> int Q5Hash<Key, T>::size() const

    Returns the number of items in the hash.

    \sa isEmpty(), count()
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::isEmpty() const

    Returns \c true if the hash contains no items; otherwise returns
    false.

    \sa size()
*/

/*! \fn template <class Key, class T> int Q5Hash<Key, T>::capacity() const

    Returns the number of buckets in the Q5Hash's internal hash table.

    The sole purpose of this function is to provide a means of fine
    tuning Q5Hash's memory usage. In general, you will rarely ever
    need to call this function. If you want to know how many items are
    in the hash, call size().

    \sa reserve(), squeeze()
*/

/*! \fn template <class Key, class T> void Q5Hash<Key, T>::reserve(int size)

    Ensures that the Q5Hash's internal hash table consists of at least
    \a size buckets.

    This function is useful for code that needs to build a huge hash
    and wants to avoid repeated reallocation. For example:

    \snippet code/src_corelib_tools_qhash.cpp 14

    Ideally, \a size should be slightly more than the maximum number
    of items expected in the hash. \a size doesn't have to be prime,
    because Q5Hash will use a prime number internally anyway. If \a size
    is an underestimate, the worst that will happen is that the Q5Hash
    will be a bit slower.

    In general, you will rarely ever need to call this function.
    Q5Hash's internal hash table automatically shrinks or grows to
    provide good performance without wasting too much memory.

    \sa squeeze(), capacity()
*/

/*! \fn template <class Key, class T> void Q5Hash<Key, T>::squeeze()

    Reduces the size of the Q5Hash's internal hash table to save
    memory.

    The sole purpose of this function is to provide a means of fine
    tuning Q5Hash's memory usage. In general, you will rarely ever
    need to call this function.

    \sa reserve(), capacity()
*/

/*! \fn template <class Key, class T> void Q5Hash<Key, T>::detach()

    \internal

    Detaches this hash from any other hashes with which it may share
    data.

    \sa isDetached()
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::isDetached() const

    \internal

    Returns \c true if the hash's internal data isn't shared with any
    other hash object; otherwise returns \c false.

    \sa detach()
*/

/*! \fn template <class Key, class T> void Q5Hash<Key, T>::setSharable(bool sharable)

    \internal
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::isSharedWith(const Q5Hash &other) const

    \internal
*/

/*! \fn template <class Key, class T> void Q5Hash<Key, T>::clear()

    Removes all items from the hash.

    \sa remove()
*/

/*! \fn template <class Key, class T> int Q5Hash<Key, T>::remove(const Key &key)

    Removes all the items that have the \a key from the hash.
    Returns the number of items removed which is 1 if the key exists in the hash,
    and 0 otherwise.

    \sa clear(), take(), Q5MultiHash::remove()
*/

/*! \fn template <class Key, class T> T Q5Hash<Key, T>::take(const Key &key)

    Removes the item with the \a key from the hash and returns
    the value associated with it.

    If the item does not exist in the hash, the function simply
    returns a \l{default-constructed value}. If there are multiple
    items for \a key in the hash, only the most recently inserted one
    is removed.

    If you don't use the return value, remove() is more efficient.

    \sa remove()
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::contains(const Key &key) const

    Returns \c true if the hash contains an item with the \a key;
    otherwise returns \c false.

    \sa count(), Q5MultiHash::contains()
*/

/*! \fn template <class Key, class T> const T Q5Hash<Key, T>::value(const Key &key) const

    Returns the value associated with the \a key.

    If the hash contains no item with the \a key, the function
    returns a \l{default-constructed value}. If there are multiple
    items for the \a key in the hash, the value of the most recently
    inserted one is returned.

    \sa key(), values(), contains(), operator[]()
*/

/*! \fn template <class Key, class T> const T Q5Hash<Key, T>::value(const Key &key, const T &defaultValue) const
    \overload

    If the hash contains no item with the given \a key, the function returns
    \a defaultValue.
*/

/*! \fn template <class Key, class T> T &Q5Hash<Key, T>::operator[](const Key &key)

    Returns the value associated with the \a key as a modifiable
    reference.

    If the hash contains no item with the \a key, the function inserts
    a \l{default-constructed value} into the hash with the \a key, and
    returns a reference to it. If the hash contains multiple items
    with the \a key, this function returns a reference to the most
    recently inserted value.

    \sa insert(), value()
*/

/*! \fn template <class Key, class T> const T Q5Hash<Key, T>::operator[](const Key &key) const

    \overload

    Same as value().
*/

/*! \fn template <class Key, class T> QList<Key> Q5Hash<Key, T>::uniqueKeys() const
    \since 4.2
    \obsolete Use Q5MultiHash for storing multiple values with the same key.

    Returns a list containing all the keys in the map. Keys that occur multiple
    times in the map (because items were inserted with insertMulti(), or
    unite() was used) occur only once in the returned list.

    \sa Q5MultiHash::uniqueKeys()
*/

/*! \fn template <class Key, class T> QList<Key> Q5Hash<Key, T>::keys() const

    Returns a list containing all the keys in the hash, in an
    arbitrary order. Keys that occur multiple times in the hash
    (because the method is operating on a Q5MultiHash) also occur
    multiple times in the list.

    The order is guaranteed to be the same as that used by values().

    \sa QMultiMap::uniqueKeys(), values(), key()
*/

/*! \fn template <class Key, class T> QList<Key> Q5Hash<Key, T>::keys(const T &value) const

    \overload

    Returns a list containing all the keys associated with value \a
    value, in an arbitrary order.

    This function can be slow (\l{linear time}), because Q5Hash's
    internal data structure is optimized for fast lookup by key, not
    by value.
*/

/*! \fn template <class Key, class T> QList<T> Q5Hash<Key, T>::values() const

    Returns a list containing all the values in the hash, in an
    arbitrary order. If a key is associated with multiple values, all of
    its values will be in the list, and not just the most recently
    inserted one.

    The order is guaranteed to be the same as that used by keys().

    \sa keys(), value()
*/

/*! \fn template <class Key, class T> QList<T> Q5Hash<Key, T>::values(const Key &key) const
    \overload
    \obsolete Use Q5MultiHash for storing multiple values with the same key.

    Returns a list of all the values associated with the \a key,
    from the most recently inserted to the least recently inserted.

    \sa count(), insertMulti()
*/

/*! \fn template <class Key, class T> Key Q5Hash<Key, T>::key(const T &value) const

    Returns the first key mapped to \a value.

    If the hash contains no item with the \a value, the function
    returns a \l{default-constructed value}{default-constructed key}.

    This function can be slow (\l{linear time}), because Q5Hash's
    internal data structure is optimized for fast lookup by key, not
    by value.

    \sa value(), keys()
*/

/*!
    \fn template <class Key, class T> Key Q5Hash<Key, T>::key(const T &value, const Key &defaultKey) const
    \since 4.3
    \overload

    Returns the first key mapped to \a value, or \a defaultKey if the
    hash contains no item mapped to \a value.

    This function can be slow (\l{linear time}), because Q5Hash's
    internal data structure is optimized for fast lookup by key, not
    by value.
*/

/*! \fn template <class Key, class T> int Q5Hash<Key, T>::count(const Key &key) const

    Returns the number of items associated with the \a key.

    \sa contains(), insertMulti()
*/

/*! \fn template <class Key, class T> int Q5Hash<Key, T>::count() const

    \overload

    Same as size().
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::begin()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the first item in
    the hash.

    \sa constBegin(), end()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::begin() const

    \overload
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::cbegin() const
    \since 5.0

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first item
    in the hash.

    \sa begin(), cend()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::constBegin() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first item
    in the hash.

    \sa begin(), constEnd()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::key_iterator Q5Hash<Key, T>::keyBegin() const
    \since 5.6

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first key
    in the hash.

    \sa keyEnd()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::end()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the imaginary item
    after the last item in the hash.

    \sa begin(), constEnd()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::end() const

    \overload
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::constEnd() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    item after the last item in the hash.

    \sa constBegin(), end()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::cend() const
    \since 5.0

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    item after the last item in the hash.

    \sa cbegin(), end()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::key_iterator Q5Hash<Key, T>::keyEnd() const
    \since 5.6

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    item after the last key in the hash.

    \sa keyBegin()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::key_value_iterator Q5Hash<Key, T>::keyValueBegin()
    \since 5.10

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the first entry
    in the hash.

    \sa keyValueEnd()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::key_value_iterator Q5Hash<Key, T>::keyValueEnd()
    \since 5.10

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    entry after the last entry in the hash.

    \sa keyValueBegin()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_key_value_iterator Q5Hash<Key, T>::keyValueBegin() const
    \since 5.10

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first entry
    in the hash.

    \sa keyValueEnd()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_key_value_iterator Q5Hash<Key, T>::constKeyValueBegin() const
    \since 5.10

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first entry
    in the hash.

    \sa keyValueBegin()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_key_value_iterator Q5Hash<Key, T>::keyValueEnd() const
    \since 5.10

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    entry after the last entry in the hash.

    \sa keyValueBegin()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_key_value_iterator Q5Hash<Key, T>::constKeyValueEnd() const
    \since 5.10

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    entry after the last entry in the hash.

    \sa constKeyValueBegin()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::erase(const_iterator pos)
    \since 5.7

    Removes the (key, value) pair associated with the iterator \a pos
    from the hash, and returns an iterator to the next item in the
    hash.

    Unlike remove() and take(), this function never causes Q5Hash to
    rehash its internal data structure. This means that it can safely
    be called while iterating, and won't affect the order of items in
    the hash. For example:

    \snippet code/src_corelib_tools_qhash.cpp 15

    \sa remove(), take(), find()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::erase(iterator pos)
    \overload
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::find(const Key &key)

    Returns an iterator pointing to the item with the \a key in the
    hash.

    If the hash contains no item with the \a key, the function
    returns end().

    If the hash contains multiple items with the \a key, this
    function returns an iterator that points to the most recently
    inserted value. The other values are accessible by incrementing
    the iterator. For example, here's some code that iterates over all
    the items with the same key:

    \snippet code/src_corelib_tools_qhash.cpp 16

    \sa value(), values(), Q5MultiHash::find()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::find(const Key &key) const

    \overload
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::constFind(const Key &key) const
    \since 4.1

    Returns an iterator pointing to the item with the \a key in the
    hash.

    If the hash contains no item with the \a key, the function
    returns constEnd().

    \sa find(), Q5MultiHash::constFind()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::insert(const Key &key, const T &value)

    Inserts a new item with the \a key and a value of \a value.

    If there is already an item with the \a key, that item's value
    is replaced with \a value.

    If there are multiple items with the \a key, the most
    recently inserted item's value is replaced with \a value.
*/

/*! \fn template <class Key, class T> void Q5Hash<Key, T>::insert(const Q5Hash &other)
    \since 5.15

    Inserts all the items in the \a other hash into this hash.

    If a key is common to both hashes, its value will be replaced with the
    value stored in \a other.

    \note If \a other contains multiple entries with the same key then the
    final value of the key is undefined.
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::insertMulti(const Key &key, const T &value)
    \obsolete

    Inserts a new item with the \a key and a value of \a value.

    If there is already an item with the same key in the hash, this
    function will simply create a new one. (This behavior is
    different from insert(), which overwrites the value of an
    existing item.)

    This function is obsolete. Use Q5MultiHash or QMultiMap instead.

    \sa insert(), values()
*/

/*! \fn template <class Key, class T> Q5Hash &Q5Hash<Key, T>::unite(const Q5Hash &other)
    \obsolete

    Inserts all the items in the \a other hash into this hash. If a
    key is common to both hashes, the resulting hash will contain the
    key multiple times.
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::empty() const

    This function is provided for STL compatibility. It is equivalent
    to isEmpty(), returning true if the hash is empty; otherwise
    returns \c false.
*/

/*! \fn template <class Key, class T> QPair<iterator, iterator> Q5Hash<Key, T>::equal_range(const Key &key)
    \since 5.7

    Returns a pair of iterators delimiting the range of values \c{[first, second)}, that
    are stored under \a key. If the range is empty then both iterators will be equal to end().
*/

/*!
    \fn template <class Key, class T> QPair<const_iterator, const_iterator> Q5Hash<Key, T>::equal_range(const Key &key) const
    \overload
    \since 5.7
*/

/*! \typedef Q5Hash::ConstIterator

    Qt-style synonym for Q5Hash::const_iterator.
*/

/*! \typedef Q5Hash::Iterator

    Qt-style synonym for Q5Hash::iterator.
*/

/*! \typedef Q5Hash::difference_type

    Typedef for ptrdiff_t. Provided for STL compatibility.
*/

/*! \typedef Q5Hash::key_type

    Typedef for Key. Provided for STL compatibility.
*/

/*! \typedef Q5Hash::mapped_type

    Typedef for T. Provided for STL compatibility.
*/

/*! \typedef Q5Hash::size_type

    Typedef for int. Provided for STL compatibility.
*/

/*! \typedef Q5Hash::iterator::difference_type
    \internal
*/

/*! \typedef Q5Hash::iterator::iterator_category
    \internal
*/

/*! \typedef Q5Hash::iterator::pointer
    \internal
*/

/*! \typedef Q5Hash::iterator::reference
    \internal
*/

/*! \typedef Q5Hash::iterator::value_type
    \internal
*/

/*! \typedef Q5Hash::const_iterator::difference_type
    \internal
*/

/*! \typedef Q5Hash::const_iterator::iterator_category
    \internal
*/

/*! \typedef Q5Hash::const_iterator::pointer
    \internal
*/

/*! \typedef Q5Hash::const_iterator::reference
    \internal
*/

/*! \typedef Q5Hash::const_iterator::value_type
    \internal
*/

/*! \typedef Q5Hash::key_iterator::difference_type
    \internal
*/

/*! \typedef Q5Hash::key_iterator::iterator_category
    \internal
*/

/*! \typedef Q5Hash::key_iterator::pointer
    \internal
*/

/*! \typedef Q5Hash::key_iterator::reference
    \internal
*/

/*! \typedef Q5Hash::key_iterator::value_type
    \internal
*/

/*! \class Q5Hash::iterator
    \inmodule QtCore
    \brief The Q5Hash::iterator class provides an STL-style non-const iterator for Q5Hash and Q5MultiHash.

    Q5Hash features both \l{STL-style iterators} and \l{Java-style
    iterators}. The STL-style iterators are more low-level and more
    cumbersome to use; on the other hand, they are slightly faster
    and, for developers who already know STL, have the advantage of
    familiarity.

    Q5Hash\<Key, T\>::iterator allows you to iterate over a Q5Hash (or
    Q5MultiHash) and to modify the value (but not the key) associated
    with a particular key. If you want to iterate over a const Q5Hash,
    you should use Q5Hash::const_iterator. It is generally good
    practice to use Q5Hash::const_iterator on a non-const Q5Hash as
    well, unless you need to change the Q5Hash through the iterator.
    Const iterators are slightly faster, and can improve code
    readability.

    The default Q5Hash::iterator constructor creates an uninitialized
    iterator. You must initialize it using a Q5Hash function like
    Q5Hash::begin(), Q5Hash::end(), or Q5Hash::find() before you can
    start iterating. Here's a typical loop that prints all the (key,
    value) pairs stored in a hash:

    \snippet code/src_corelib_tools_qhash.cpp 17

    Unlike QMap, which orders its items by key, Q5Hash stores its
    items in an arbitrary order.

    Let's see a few examples of things we can do with a
    Q5Hash::iterator that we cannot do with a Q5Hash::const_iterator.
    Here's an example that increments every value stored in the Q5Hash
    by 2:

    \snippet code/src_corelib_tools_qhash.cpp 18

    Here's an example that removes all the items whose key is a
    string that starts with an underscore character:

    \snippet code/src_corelib_tools_qhash.cpp 19

    The call to Q5Hash::erase() removes the item pointed to by the
    iterator from the hash, and returns an iterator to the next item.
    Here's another way of removing an item while iterating:

    \snippet code/src_corelib_tools_qhash.cpp 20

    It might be tempting to write code like this:

    \snippet code/src_corelib_tools_qhash.cpp 21

    However, this will potentially crash in \c{++i}, because \c i is
    a dangling iterator after the call to erase().

    Multiple iterators can be used on the same hash. However, be
    aware that any modification performed directly on the Q5Hash has
    the potential of dramatically changing the order in which the
    items are stored in the hash, as they might cause Q5Hash to rehash
    its internal data structure. There is one notable exception:
    Q5Hash::erase(). This function can safely be called while
    iterating, and won't affect the order of items in the hash. If you
    need to keep iterators over a long period of time, we recommend
    that you use QMap rather than Q5Hash.

    \warning Iterators on implicitly shared containers do not work
    exactly like STL-iterators. You should avoid copying a container
    while iterators are active on that container. For more information,
    read \l{Implicit sharing iterator problem}.

    \sa Q5Hash::const_iterator, Q5Hash::key_iterator, Q5MutableHashIterator
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator::iterator()

    Constructs an uninitialized iterator.

    Functions like key(), value(), and operator++() must not be
    called on an uninitialized iterator. Use operator=() to assign a
    value to it before using it.

    \sa Q5Hash::begin(), Q5Hash::end()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator::iterator(void *node)

    \internal
*/

/*! \fn template <class Key, class T> const Key &Q5Hash<Key, T>::iterator::key() const

    Returns the current item's key as a const reference.

    There is no direct way of changing an item's key through an
    iterator, although it can be done by calling Q5Hash::erase()
    followed by Q5Hash::insert().

    \sa value()
*/

/*! \fn template <class Key, class T> T &Q5Hash<Key, T>::iterator::value() const

    Returns a modifiable reference to the current item's value.

    You can change the value of an item by using value() on
    the left side of an assignment, for example:

    \snippet code/src_corelib_tools_qhash.cpp 22

    \sa key(), operator*()
*/

/*! \fn template <class Key, class T> T &Q5Hash<Key, T>::iterator::operator*() const

    Returns a modifiable reference to the current item's value.

    Same as value().

    \sa key()
*/

/*! \fn template <class Key, class T> T *Q5Hash<Key, T>::iterator::operator->() const

    Returns a pointer to the current item's value.

    \sa value()
*/

/*!
    \fn template <class Key, class T> bool Q5Hash<Key, T>::iterator::operator==(const iterator &other) const
    \fn template <class Key, class T> bool Q5Hash<Key, T>::iterator::operator==(const const_iterator &other) const

    Returns \c true if \a other points to the same item as this
    iterator; otherwise returns \c false.

    \sa operator!=()
*/

/*!
    \fn template <class Key, class T> bool Q5Hash<Key, T>::iterator::operator!=(const iterator &other) const
    \fn template <class Key, class T> bool Q5Hash<Key, T>::iterator::operator!=(const const_iterator &other) const

    Returns \c true if \a other points to a different item than this
    iterator; otherwise returns \c false.

    \sa operator==()
*/

/*!
    \fn template <class Key, class T> Q5Hash<Key, T>::iterator &Q5Hash<Key, T>::iterator::operator++()

    The prefix ++ operator (\c{++i}) advances the iterator to the
    next item in the hash and returns an iterator to the new current
    item.

    Calling this function on Q5Hash::end() leads to undefined results.

    \sa operator--()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::iterator::operator++(int)

    \overload

    The postfix ++ operator (\c{i++}) advances the iterator to the
    next item in the hash and returns an iterator to the previously
    current item.
*/

/*!
    \fn template <class Key, class T> Q5Hash<Key, T>::iterator &Q5Hash<Key, T>::iterator::operator--()
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    The prefix -- operator (\c{--i}) makes the preceding item
    current and returns an iterator pointing to the new current item.

    Calling this function on Q5Hash::begin() leads to undefined
    results.

    \sa operator++()
*/

/*!
    \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::iterator::operator--(int)
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    \overload

    The postfix -- operator (\c{i--}) makes the preceding item
    current and returns an iterator pointing to the previously
    current item.
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::iterator::operator+(int j) const
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    Returns an iterator to the item at \a j positions forward from
    this iterator. (If \a j is negative, the iterator goes backward.)

    This operation can be slow for large \a j values.

    \sa operator-()

*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator Q5Hash<Key, T>::iterator::operator-(int j) const
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    Returns an iterator to the item at \a j positions backward from
    this iterator. (If \a j is negative, the iterator goes forward.)

    This operation can be slow for large \a j values.

    \sa operator+()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator &Q5Hash<Key, T>::iterator::operator+=(int j)
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    Advances the iterator by \a j items. (If \a j is negative, the
    iterator goes backward.)

    \sa operator-=(), operator+()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::iterator &Q5Hash<Key, T>::iterator::operator-=(int j)
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    Makes the iterator go back by \a j items. (If \a j is negative,
    the iterator goes forward.)

    \sa operator+=(), operator-()
*/

/*! \class Q5Hash::const_iterator
    \inmodule QtCore
    \brief The Q5Hash::const_iterator class provides an STL-style const iterator for Q5Hash and Q5MultiHash.

    Q5Hash features both \l{STL-style iterators} and \l{Java-style
    iterators}. The STL-style iterators are more low-level and more
    cumbersome to use; on the other hand, they are slightly faster
    and, for developers who already know STL, have the advantage of
    familiarity.

    Q5Hash\<Key, T\>::const_iterator allows you to iterate over a
    Q5Hash (or a Q5MultiHash). If you want to modify the Q5Hash as you
    iterate over it, you must use Q5Hash::iterator instead. It is
    generally good practice to use Q5Hash::const_iterator on a
    non-const Q5Hash as well, unless you need to change the Q5Hash
    through the iterator. Const iterators are slightly faster, and
    can improve code readability.

    The default Q5Hash::const_iterator constructor creates an
    uninitialized iterator. You must initialize it using a Q5Hash
    function like Q5Hash::constBegin(), Q5Hash::constEnd(), or
    Q5Hash::find() before you can start iterating. Here's a typical
    loop that prints all the (key, value) pairs stored in a hash:

    \snippet code/src_corelib_tools_qhash.cpp 23

    Unlike QMap, which orders its items by key, Q5Hash stores its
    items in an arbitrary order. The only guarantee is that items that
    share the same key (because they were inserted using
    a Q5MultiHash) will appear consecutively, from the most
    recently to the least recently inserted value.

    Multiple iterators can be used on the same hash. However, be aware
    that any modification performed directly on the Q5Hash has the
    potential of dramatically changing the order in which the items
    are stored in the hash, as they might cause Q5Hash to rehash its
    internal data structure. If you need to keep iterators over a long
    period of time, we recommend that you use QMap rather than Q5Hash.

    \warning Iterators on implicitly shared containers do not work
    exactly like STL-iterators. You should avoid copying a container
    while iterators are active on that container. For more information,
    read \l{Implicit sharing iterator problem}.

    \sa Q5Hash::iterator, Q5HashIterator
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator::const_iterator()

    Constructs an uninitialized iterator.

    Functions like key(), value(), and operator++() must not be
    called on an uninitialized iterator. Use operator=() to assign a
    value to it before using it.

    \sa Q5Hash::constBegin(), Q5Hash::constEnd()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator::const_iterator(void *node)

    \internal
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator::const_iterator(const iterator &other)

    Constructs a copy of \a other.
*/

/*! \fn template <class Key, class T> const Key &Q5Hash<Key, T>::const_iterator::key() const

    Returns the current item's key.

    \sa value()
*/

/*! \fn template <class Key, class T> const T &Q5Hash<Key, T>::const_iterator::value() const

    Returns the current item's value.

    \sa key(), operator*()
*/

/*! \fn template <class Key, class T> const T &Q5Hash<Key, T>::const_iterator::operator*() const

    Returns the current item's value.

    Same as value().

    \sa key()
*/

/*! \fn template <class Key, class T> const T *Q5Hash<Key, T>::const_iterator::operator->() const

    Returns a pointer to the current item's value.

    \sa value()
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::const_iterator::operator==(const const_iterator &other) const

    Returns \c true if \a other points to the same item as this
    iterator; otherwise returns \c false.

    \sa operator!=()
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::const_iterator::operator!=(const const_iterator &other) const

    Returns \c true if \a other points to a different item than this
    iterator; otherwise returns \c false.

    \sa operator==()
*/

/*!
    \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator &Q5Hash<Key, T>::const_iterator::operator++()

    The prefix ++ operator (\c{++i}) advances the iterator to the
    next item in the hash and returns an iterator to the new current
    item.

    Calling this function on Q5Hash::end() leads to undefined results.

    \sa operator--()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::const_iterator::operator++(int)

    \overload

    The postfix ++ operator (\c{i++}) advances the iterator to the
    next item in the hash and returns an iterator to the previously
    current item.
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator &Q5Hash<Key, T>::const_iterator::operator--()
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    The prefix -- operator (\c{--i}) makes the preceding item
    current and returns an iterator pointing to the new current item.

    Calling this function on Q5Hash::begin() leads to undefined
    results.

    \sa operator++()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::const_iterator::operator--(int)
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    \overload

    The postfix -- operator (\c{i--}) makes the preceding item
    current and returns an iterator pointing to the previously
    current item.
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::const_iterator::operator+(int j) const
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    Returns an iterator to the item at \a j positions forward from
    this iterator. (If \a j is negative, the iterator goes backward.)

    This operation can be slow for large \a j values.

    \sa operator-()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::const_iterator::operator-(int j) const
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    Returns an iterator to the item at \a j positions backward from
    this iterator. (If \a j is negative, the iterator goes forward.)

    This operation can be slow for large \a j values.

    \sa operator+()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator &Q5Hash<Key, T>::const_iterator::operator+=(int j)
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    Advances the iterator by \a j items. (If \a j is negative, the
    iterator goes backward.)

    This operation can be slow for large \a j values.

    \sa operator-=(), operator+()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::const_iterator &Q5Hash<Key, T>::const_iterator::operator-=(int j)
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    Makes the iterator go back by \a j items. (If \a j is negative,
    the iterator goes forward.)

    This operation can be slow for large \a j values.

    \sa operator+=(), operator-()
*/

/*! \class Q5Hash::key_iterator
    \inmodule QtCore
    \since 5.6
    \brief The Q5Hash::key_iterator class provides an STL-style const iterator for Q5Hash and Q5MultiHash keys.

    Q5Hash::key_iterator is essentially the same as Q5Hash::const_iterator
    with the difference that operator*() and operator->() return a key
    instead of a value.

    For most uses Q5Hash::iterator and Q5Hash::const_iterator should be used,
    you can easily access the key by calling Q5Hash::iterator::key():

    \snippet code/src_corelib_tools_qhash.cpp 27

    However, to have interoperability between Q5Hash's keys and STL-style
    algorithms we need an iterator that dereferences to a key instead
    of a value. With Q5Hash::key_iterator we can apply an algorithm to a
    range of keys without having to call Q5Hash::keys(), which is inefficient
    as it costs one Q5Hash iteration and memory allocation to create a temporary
    QList.

    \snippet code/src_corelib_tools_qhash.cpp 28

    Q5Hash::key_iterator is const, it's not possible to modify the key.

    The default Q5Hash::key_iterator constructor creates an uninitialized
    iterator. You must initialize it using a Q5Hash function like
    Q5Hash::keyBegin() or Q5Hash::keyEnd().

    \warning Iterators on implicitly shared containers do not work
    exactly like STL-iterators. You should avoid copying a container
    while iterators are active on that container. For more information,
    read \l{Implicit sharing iterator problem}.

    \sa Q5Hash::const_iterator, Q5Hash::iterator
*/

/*! \fn template <class Key, class T> const T &Q5Hash<Key, T>::key_iterator::operator*() const

    Returns the current item's key.
*/

/*! \fn template <class Key, class T> const T *Q5Hash<Key, T>::key_iterator::operator->() const

    Returns a pointer to the current item's key.
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::key_iterator::operator==(key_iterator other) const

    Returns \c true if \a other points to the same item as this
    iterator; otherwise returns \c false.

    \sa operator!=()
*/

/*! \fn template <class Key, class T> bool Q5Hash<Key, T>::key_iterator::operator!=(key_iterator other) const

    Returns \c true if \a other points to a different item than this
    iterator; otherwise returns \c false.

    \sa operator==()
*/

/*!
    \fn template <class Key, class T> Q5Hash<Key, T>::key_iterator &Q5Hash<Key, T>::key_iterator::operator++()

    The prefix ++ operator (\c{++i}) advances the iterator to the
    next item in the hash and returns an iterator to the new current
    item.

    Calling this function on Q5Hash::keyEnd() leads to undefined results.

    \sa operator--()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::key_iterator Q5Hash<Key, T>::key_iterator::operator++(int)

    \overload

    The postfix ++ operator (\c{i++}) advances the iterator to the
    next item in the hash and returns an iterator to the previous
    item.
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::key_iterator &Q5Hash<Key, T>::key_iterator::operator--()
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    The prefix -- operator (\c{--i}) makes the preceding item
    current and returns an iterator pointing to the new current item.

    Calling this function on Q5Hash::keyBegin() leads to undefined
    results.

    \sa operator++()
*/

/*! \fn template <class Key, class T> Q5Hash<Key, T>::key_iterator Q5Hash<Key, T>::key_iterator::operator--(int)
    \obsolete This operator is deprecated in order to align with std::unordered_map functionality.

    \overload

    The postfix -- operator (\c{i--}) makes the preceding item
    current and returns an iterator pointing to the previous
    item.
*/

/*! \fn template <class Key, class T> const_iterator Q5Hash<Key, T>::key_iterator::base() const
    Returns the underlying const_iterator this key_iterator is based on.
*/

/*! \typedef Q5Hash::const_key_value_iterator
    \inmodule QtCore
    \since 5.10
    \brief The QMap::const_key_value_iterator typedef provides an STL-style const iterator for Q5Hash and Q5MultiHash.

    Q5Hash::const_key_value_iterator is essentially the same as Q5Hash::const_iterator
    with the difference that operator*() returns a key/value pair instead of a
    value.

    \sa QKeyValueIterator
*/

/*! \typedef Q5Hash::key_value_iterator
    \inmodule QtCore
    \since 5.10
    \brief The QMap::key_value_iterator typedef provides an STL-style iterator for Q5Hash and Q5MultiHash.

    Q5Hash::key_value_iterator is essentially the same as Q5Hash::iterator
    with the difference that operator*() returns a key/value pair instead of a
    value.

    \sa QKeyValueIterator
*/

/*! \fn template <class Key, class T> QDataStream &operator<<(QDataStream &out, const Q5Hash<Key, T>& hash)
    \relates Q5Hash

    Writes the hash \a hash to stream \a out.

    This function requires the key and value types to implement \c
    operator<<().

    \sa {Serializing Qt Data Types}
*/

/*! \fn template <class Key, class T> QDataStream &operator>>(QDataStream &in, Q5Hash<Key, T> &hash)
    \relates Q5Hash

    Reads a hash from stream \a in into \a hash.

    This function requires the key and value types to implement \c
    operator>>().

    \sa {Serializing Qt Data Types}
*/

/*! \class Q5MultiHash
    \inmodule QtCore
    \brief The Q5MultiHash class is a convenience Q5Hash subclass that provides multi-valued hashes.

    \ingroup tools
    \ingroup shared

    \reentrant

    Q5MultiHash\<Key, T\> is one of Qt's generic \l{container classes}.
    It inherits Q5Hash and extends it with a few convenience functions
    that make it more suitable than Q5Hash for storing multi-valued
    hashes. A multi-valued hash is a hash that allows multiple values
    with the same key.

    Because Q5MultiHash inherits Q5Hash, all of Q5Hash's functionality also
    applies to Q5MultiHash. For example, you can use isEmpty() to test
    whether the hash is empty, and you can traverse a Q5MultiHash using
    Q5Hash's iterator classes (for example, Q5HashIterator). But opposed to
    Q5Hash, it provides an insert() function will allow the insertion of
    multiple items with the same key. The replace() function corresponds to
    Q5Hash::insert(). It also provides convenient operator+() and
    operator+=().

    Unlike QMultiMap, Q5MultiHash does not provide and ordering of the
    inserted items. The only guarantee is that items that
    share the same key will appear consecutively, from the most
    recently to the least recently inserted value.

    Example:
    \snippet code/src_corelib_tools_qhash.cpp 24

    Unlike Q5Hash, Q5MultiHash provides no operator[]. Use value() or
    replace() if you want to access the most recently inserted item
    with a certain key.

    If you want to retrieve all the values for a single key, you can
    use values(const Key &key), which returns a QList<T>:

    \snippet code/src_corelib_tools_qhash.cpp 25

    The items that share the same key are available from most
    recently to least recently inserted.

    A more efficient approach is to call find() to get
    the STL-style iterator for the first item with a key and iterate from
    there:

    \snippet code/src_corelib_tools_qhash.cpp 26

    Q5MultiHash's key and value data types must be \l{assignable data
    types}. You cannot, for example, store a QWidget as a value;
    instead, store a QWidget *. In addition, Q5MultiHash's key type
    must provide operator==(), and there must also be a q5Hash() function
   in the type's namespace that returns a hash value for an argument of the
    key's type. See the Q5Hash documentation for details.

    \sa Q5Hash, Q5HashIterator, Q5MutableHashIterator, QMultiMap
*/

/*! \fn template <class Key, class T> Q5MultiHash<Key, T>::Q5MultiHash()

    Constructs an empty hash.
*/

/*! \fn template <class Key, class T> Q5MultiHash<Key, T>::Q5MultiHash(std::initializer_list<std::pair<Key,T> > list)
    \since 5.1

    Constructs a multi-hash with a copy of each of the elements in the
    initializer list \a list.

    This function is only available if the program is being
    compiled in C++11 mode.
*/

/*! \fn template <class Key, class T> Q5MultiHash<Key, T>::Q5MultiHash(const Q5Hash<Key, T> &other)

    Constructs a copy of \a other (which can be a Q5Hash or a
    Q5MultiHash).

    \sa operator=()
*/

/*! \fn template <class Key, class T> template <class InputIterator> Q5MultiHash<Key, T>::Q5MultiHash(InputIterator begin, InputIterator end)
    \since 5.14

    Constructs a multi-hash with a copy of each of the elements in the iterator range
    [\a begin, \a end). Either the elements iterated by the range must be
    objects with \c{first} and \c{second} data members (like \c{QPair},
    \c{std::pair}, etc.) convertible to \c Key and to \c T respectively; or the
    iterators must have \c{key()} and \c{value()} member functions, returning a
    key convertible to \c Key and a value convertible to \c T respectively.
*/

/*! \fn template <class Key, class T> Q5MultiHash<Key, T>::iterator Q5MultiHash<Key, T>::replace(const Key &key, const T &value)

    Inserts a new item with the \a key and a value of \a value.

    If there is already an item with the \a key, that item's value
    is replaced with \a value.

    If there are multiple items with the \a key, the most
    recently inserted item's value is replaced with \a value.

    \sa insert()
*/

/*! \fn template <class Key, class T> Q5MultiHash<Key, T>::iterator Q5MultiHash<Key, T>::insert(const Key &key, const T &value)

    Inserts a new item with the \a key and a value of \a value.

    If there is already an item with the same key in the hash, this
    function will simply create a new one. (This behavior is
    different from replace(), which overwrites the value of an
    existing item.)

    \sa replace()
*/

/*! \fn template <class Key, class T> Q5MultiHash &Q5MultiHash<Key, T>::unite(const Q5MultiHash &other)
    \since 5.13

    Inserts all the items in the \a other hash into this hash
    and returns a reference to this hash.

    \sa insert()
*/

/*! \fn template <class Key, class T> QList<Key> Q5MultiHash<Key, T>::uniqueKeys() const
    \since 5.13

    Returns a list containing all the keys in the map. Keys that occur multiple
    times in the map occur only once in the returned list.

    \sa keys(), values()
*/

/*! \fn template <class Key, class T> QList<T> Q5MultiHash<Key, T>::values(const Key &key) const
    \overload

    Returns a list of all the values associated with the \a key,
    from the most recently inserted to the least recently inserted.

    \sa count(), insert()
*/

/*! \fn template <class Key, class T> Q5MultiHash &Q5MultiHash<Key, T>::operator+=(const Q5MultiHash &other)

    Inserts all the items in the \a other hash into this hash
    and returns a reference to this hash.

    \sa unite(), insert()
*/

/*! \fn template <class Key, class T> Q5MultiHash Q5MultiHash<Key, T>::operator+(const Q5MultiHash &other) const

    Returns a hash that contains all the items in this hash in
    addition to all the items in \a other. If a key is common to both
    hashes, the resulting hash will contain the key multiple times.

    \sa operator+=()
*/

/*!
    \fn template <class Key, class T> bool Q5MultiHash<Key, T>::contains(const Key &key, const T &value) const
    \since 4.3

    Returns \c true if the hash contains an item with the \a key and
    \a value; otherwise returns \c false.

    \sa Q5Hash::contains()
*/

/*!
    \fn template <class Key, class T> int Q5MultiHash<Key, T>::remove(const Key &key, const T &value)
    \since 4.3

    Removes all the items that have the \a key and the value \a
    value from the hash. Returns the number of items removed.

    \sa Q5Hash::remove()
*/

/*!
    \fn template <class Key, class T> int Q5MultiHash<Key, T>::count(const Key &key, const T &value) const
    \since 4.3

    Returns the number of items with the \a key and \a value.

    \sa Q5Hash::count()
*/

/*!
    \fn template <class Key, class T> typename Q5Hash<Key, T>::iterator Q5MultiHash<Key, T>::find(const Key &key, const T &value)
    \since 4.3

    Returns an iterator pointing to the item with the \a key and \a value.
    If the hash contains no such item, the function returns end().

    If the hash contains multiple items with the \a key and \a value, the
    iterator returned points to the most recently inserted item.

    \sa Q5Hash::find()
*/

/*!
    \fn template <class Key, class T> typename Q5Hash<Key, T>::const_iterator Q5MultiHash<Key, T>::find(const Key &key, const T &value) const
    \since 4.3
    \overload
*/

/*!
    \fn template <class Key, class T> typename Q5Hash<Key, T>::const_iterator Q5MultiHash<Key, T>::constFind(const Key &key, const T &value) const
    \since 4.3

    Returns an iterator pointing to the item with the \a key and the
    \a value in the hash.

    If the hash contains no such item, the function returns
    constEnd().

    \sa Q5Hash::constFind()
*/

/*!
    \fn template <class Key, class T> uint q5Hash(const Q5Hash<Key, T> &key, uint seed = 0)
    \since 5.8
    \relates Q5Hash

    Returns the hash value for the \a key, using \a seed to seed the calculation.

    Type \c T must be supported by q5Hash().
*/

/*!
    \fn template <class Key, class T> uint q5Hash(const Q5MultiHash<Key, T> &key, uint seed = 0)
    \since 5.8
    \relates Q5MultiHash

    Returns the hash value for the \a key, using \a seed to seed the calculation.

    Type \c T must be supported by q5Hash().
*/

QT_END_NAMESPACE

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
