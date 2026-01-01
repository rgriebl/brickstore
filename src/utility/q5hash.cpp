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

QT_BEGIN_NAMESPACE


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

QT_END_NAMESPACE

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif
