/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

// The parallel merge algorithm was derived from Markus Weissmann's
// BSD licensed "pmsort" (implemented in C and pthreads):

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

#ifndef QPARALLELMERGESORT_H
#define QPARALLELMERGESORT_H

#include <QtConcurrentRun>
#include <QtAlgorithms>

namespace QAlgorithmsPrivate {

template <typename RandomAccessIterator, typename T, typename LessThan>
static inline void qParallelMergeSort_JoinSets(RandomAccessIterator begin, RandomAccessIterator end, LessThan lessThan, T *tmp, int a_span, int b_span)
{
    RandomAccessIterator a = begin;
    RandomAccessIterator b = begin + a_span;
    int span = a_span + b_span;

    Q_ASSERT(span == (end - begin));
    Q_ASSERT((b + b_span) == end);

#ifndef QT_DEBUG
    Q_UNUSED(end);
#endif

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
    for (int i = 0; i < span; ++i)
        *begin++ = *tmp++;
}

template <typename RandomAccessIterator, typename T, typename LessThan>
static inline void qParallelMergeSort_SerialSort(RandomAccessIterator begin, RandomAccessIterator end, LessThan lessThan, T *tmp)
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
        int b_span = span / 2;      // smaller half of array if array-size is uneven

        if (a_span > 1)	// no sorting necessary if size == 1
            qParallelMergeSort_SerialSort(begin, begin + a_span, lessThan, tmp);

        if (b_span > 1) // ### was 0
            qParallelMergeSort_SerialSort(begin + a_span, end, lessThan, tmp + a_span);

        qParallelMergeSort_JoinSets(begin, end, lessThan, tmp, a_span, b_span);
    }
}

template <typename RandomAccessIterator, typename T, typename LessThan>
void qParallelMergeSort_Thread(RandomAccessIterator begin, RandomAccessIterator end, LessThan lessThan, T *tmp, int threadCount)
{
    int span = end - begin;

    // stop if there are no threads left OR if size of array is at most 3
    if (threadCount < 2 || span <= 3) {
        // let's just sort this in serial
        qParallelMergeSort_SerialSort(begin, end, lessThan, tmp);
        return;
    } else {
        // divide & conquer
        int a_span = (span + 1) / 2; // bigger half of array if array-size is uneven
        int b_span = span - a_span;  // smaller half of array if array-size is uneven

        if (b_span > 1) {	// prepare thread only if sorting is necessary
            QFuture<void> future = QtConcurrent::run(qParallelMergeSort_Thread<RandomAccessIterator, T, LessThan>, begin + a_span, end, lessThan, tmp + a_span, threadCount / 2);
            qParallelMergeSort_Thread(begin, begin + a_span, lessThan, tmp, (threadCount + 1) / 2);
            future.waitForFinished();
        } else if (a_span == 2) {
            qParallelMergeSort_SerialSort(begin, end, lessThan, tmp);
        }

        qParallelMergeSort_JoinSets(begin, end, lessThan, tmp, a_span, b_span);
    }
}

template <typename RandomAccessIterator, typename T, typename LessThan>
Q_OUTOFLINE_TEMPLATE void qParallelMergeSortHelper(RandomAccessIterator begin, RandomAccessIterator end, const T &, LessThan lessThan)
{
    const int span = end - begin;
    if (span < 2)
       return;

    int threadCount = QThread::idealThreadCount();
    if (threadCount == 1) {
        qStableSort(begin, end, lessThan);
        return;
    }

    T *tmp = new T[span];
    qParallelMergeSort_Thread(begin, end, lessThan, tmp, threadCount);
    delete tmp;
}

template <typename RandomAccessIterator, typename T>
inline void qParallelMergeSortHelper(RandomAccessIterator begin, RandomAccessIterator end, const T &dummy)
{
    qParallelMergeSortHelper(begin, end, dummy, qLess<T>());
}

} // namespace QAlgorithmsPrivate

template <typename RandomAccessIterator>
inline void qParallelMergeSort(RandomAccessIterator start, RandomAccessIterator end)
{
    if (start != end)
        QAlgorithmsPrivate::qParallelMergeSortHelper(start, end, *start);
}

template <typename RandomAccessIterator, typename LessThan>
inline void qParallelMergeSort(RandomAccessIterator start, RandomAccessIterator end, LessThan lessThan)
{
    if (start != end)
        QAlgorithmsPrivate::qParallelMergeSortHelper(start, end, *start, lessThan);
}

template<typename Container>
inline void qParallelMergeSort(Container &c)
{
#ifdef Q_CC_BOR
    // Work around Borland 5.5 optimizer bug
    c.detach();
#endif
    if (!c.empty())
        QAlgorithmsPrivate::qParallelMergeSortHelper(c.begin(), c.end(), *c.begin());
}

#endif // QPARALLELMERGESORT_H
