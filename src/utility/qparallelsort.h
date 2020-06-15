/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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

// The parallel merge sort algorithm was derived from Markus Weissmann's
// BSD licensed "pmsort" (originally implemented in C and pthreads):

/*_
 * Copyright (c) 2006, 2007, Markus W. Weissmann <mail@mweissmann.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of OpenDarwin nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: $
 *
 */

#ifndef QPARALLELSORT_H
#define QPARALLELSORT_H

#include <QtConcurrentRun>
#include <QtAlgorithms>

namespace QAlgorithmsPrivate {

template <typename RandomAccessIterator, typename T, typename LessThan>
static inline void qParallelSortJoin(RandomAccessIterator begin, RandomAccessIterator end, LessThan lessThan, T *tmp, int a_span, int b_span)
{
    RandomAccessIterator a = begin;
    RandomAccessIterator b = begin + a_span;
    int span = a_span + b_span;

    Q_ASSERT(span == (end - begin));
    Q_ASSERT((b + b_span) == end);

    for (int i = 0; i < span; ++i) {
        // 'a' if no b left or a < b
        // 'b' if no a left or a >= b
        if (!b_span || (a_span && b_span && lessThan(*a, *b))) {
            *(tmp + i) = *a;
            a++;
            a_span--;
        } else {
            *(tmp + i) = *b;
            b++;
            b_span--;
        }
    }
    while (begin != end)
        *begin++ = *tmp++;
}

template <typename RandomAccessIterator, typename T, typename LessThan>
static inline void qParallelSortSerial(RandomAccessIterator begin, RandomAccessIterator end, LessThan lessThan, T *tmp)
{
    int span = end - begin;

    if (span < 2) {
        return;
    } else if (span == 2) {
        if (!lessThan(*begin, *(begin + 1))) {
            *tmp = *begin;
            *begin = *(begin + 1);
            *(begin + 1) = *tmp;
        }
    } else {
        int a_span = (span + 1) / 2;  // bigger half of array if array-size is uneven
        int b_span = span / 2;        // smaller half of array if array-size is uneven

        if (a_span > 1)	// no sorting necessary if size == 1
            qParallelSortSerial(begin, begin + a_span, lessThan, tmp);

        if (b_span > 1)	// no sorting necessary if size == 1
            qParallelSortSerial(begin + a_span, end, lessThan, tmp + a_span);

        qParallelSortJoin(begin, end, lessThan, tmp, a_span, b_span);
    }
}

template <typename RandomAccessIterator, typename T, typename LessThan>
void qParallelSortThread(RandomAccessIterator begin, RandomAccessIterator end, LessThan lessThan, T *tmp, int threadCount)
{
    int span = end - begin;

    // stop if there are no threads left OR if size of array is at most 3
    if (threadCount < 2 || span <= 3) {
        // let's just sort this in serial
        qParallelSortSerial(begin, end, lessThan, tmp);
        return;
    } else {
        // divide & conquer
        int a_span = (span + 1) / 2; // bigger half of array if array-size is uneven
        int b_span = span - a_span;  // smaller half of array if array-size is uneven

        if (b_span > 1) {	// prepare thread only if sorting is necessary
            QFuture<void> future = QtConcurrent::run(qParallelSortThread<RandomAccessIterator, T, LessThan>, begin + a_span, end, lessThan, tmp + a_span, threadCount / 2);
            qParallelSortThread(begin, begin + a_span, lessThan, tmp, (threadCount + 1) / 2);
            future.waitForFinished();
        } else if (a_span == 2) {
            qParallelSortSerial(begin, end, lessThan, tmp);
        }

        qParallelSortJoin(begin, end, lessThan, tmp, a_span, b_span);
    }
}

template <typename RandomAccessIterator, typename T, typename LessThan>
Q_OUTOFLINE_TEMPLATE void qParallelSortHelper(RandomAccessIterator begin, RandomAccessIterator end, const T &, LessThan lessThan)
{
    const int span = end - begin;
    if (span < 2)
       return;

    int threadCount = QThread::idealThreadCount();
    if (threadCount == 1 || span < 1000) {
        qSort(begin, end, lessThan);
        return;
    }

    T *tmp = new T[span];
    qParallelSortThread(begin, end, lessThan, tmp, threadCount);
    delete tmp;
}

template <typename RandomAccessIterator, typename T>
inline void qParallelSortHelper(RandomAccessIterator begin, RandomAccessIterator end, const T &dummy)
{
    qParallelSortHelper(begin, end, dummy, qLess<T>());
}

} // namespace QAlgorithmsPrivate

template <typename RandomAccessIterator>
inline void qParallelSort(RandomAccessIterator start, RandomAccessIterator end)
{
    if (start != end)
        QAlgorithmsPrivate::qParallelSortHelper(start, end, *start);
}

template <typename RandomAccessIterator, typename LessThan>
inline void qParallelSort(RandomAccessIterator start, RandomAccessIterator end, LessThan lessThan)
{
    if (start != end)
        QAlgorithmsPrivate::qParallelSortHelper(start, end, *start, lessThan);
}

template<typename Container>
inline void qParallelSort(Container &c)
{
#ifdef Q_CC_BOR
    // Work around Borland 5.5 optimizer bug
    c.detach();
#endif
    if (!c.empty())
        QAlgorithmsPrivate::qParallelSortHelper(c.begin(), c.end(), *c.begin());
}

#if 0 // benchmarking
#include "stopwatch.h"

void test_par_sort()
{
    for (int k = 0; k < 3; ++k) {
        const char *which = (k == 0 ? "Stable Sort  " :
                            (k == 1 ? "Quick Sort   " :
                                      "Parallel Sort"));
        int maxloop = 21;
        int *src = new int[1 << maxloop];
        for (int j = 0; j < (1 << maxloop); ++j)
            src[j] = rand();

        for (int i = 0; i <= maxloop; ++i) {
            int count = 1 << i;
            QByteArray msg = QString("%3 test run %1 ... %2 values").arg(i).arg(count).arg(which).toLatin1();
            int *array = new int[count];
            memcpy(array, src, sizeof(int) * count);

            {
                stopwatch sw(msg.constData());
                if (k == 0)
                    qStableSort(array, array + count);
                else if (k == 1)
                    qSort(array, array + count);
                else
                    qParallelSort(array, array + count);
            }
            delete [] array;
        }
        delete [] src;
    }
}
#endif

#endif // QPARALLELSort_H
