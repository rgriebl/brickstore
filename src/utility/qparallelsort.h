// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

// The merges are split with the co-rank binary search from Odeh, Green, Mwassi,
// Shmueli, Birk: "Merge Path - Parallel Merging Made Simple" (IPDPS 2012).

#pragma once

#include <algorithm>
#include <bit>
#include <iterator>
#include <memory>

#include <QtCore/QThreadPool>
#include <QtCore/QVarLengthArray>
#include <QtConcurrentRun>

//#define QPARALLELSORT_TESTING // benchmarking

// Bottom-up parallel merge sort:
//   phase 1: sort `chunks` equally sized blocks in parallel with std::sort
//   phase 2: log2(chunks) levels, each merging pairs of adjacent runs
//
// Every merge is split into as many independent output pieces as there are
// chunks. Serial merges would make the top level merge alone O(n) on a single
// thread, which caps the speedup at ~6x regardless of the core count.
// Consecutive levels ping-pong between the array and the scratch buffer, so no
// copy-back is needed; if the number of levels is odd, phase 1 moves the sorted
// chunks into the buffer first, so that the last level lands in the array.

static constexpr qsizetype MinParallelSpan = 4096;

// Runs fn(0) to fn(count - 1) in parallel; the calling thread takes part.
template <typename Fn>
inline void qParallelRun(int count, Fn fn)
{
    QVarLengthArray<QFuture<void>, 64> futures;
    futures.reserve(count - 1);
    for (int i = 1; i < count; ++i)
        futures.append(QtConcurrent::run(fn, i));
    fn(0);
    for (auto &future : futures)
        future.waitForFinished();
}

// Where to cut the merge of [x, x + nx) and [y, y + ny) to get exactly the
// first k elements of the merged range: returns the number of elements to take
// from x. Both cut points of a piece are valid merge path positions, so the
// pieces neither overlap nor leave gaps.
template <typename T, typename LessThan>
inline qsizetype qParallelMergeSplit(const T *x, qsizetype nx, const T *y, qsizetype ny,
                                     qsizetype k, LessThan lessThan)
{
    qsizetype lo = std::max(qsizetype(0), k - ny);
    qsizetype hi = std::min(k, nx);

    while (lo < hi) {
        const qsizetype i = lo + (hi - lo) / 2; // i < nx and 0 <= k - i - 1 < ny
        if (lessThan(x[i], y[k - i - 1]))
            lo = i + 1;
        else
            hi = i;
    }
    return lo;
}

template <typename T, typename LessThan>
void qParallelMergeSortImpl(T *array, qsizetype n, LessThan lessThan)
{
    // as many chunks as the pool can run at once (the calling thread replaces
    // the one task we do not hand to the pool), rounded down to a power of 2,
    // but never smaller than MinParallelSpan elements each
    const qsizetype maxChunks = std::min(qsizetype(QThreadPool::globalInstance()->maxThreadCount()),
                                         n / MinParallelSpan);
    const int levels = (maxChunks < 2) ? 0 : (std::bit_width(size_t(maxChunks)) - 1);
    const int chunks = 1 << levels;

    if (chunks < 2) {
        std::sort(array, array + n, lessThan);
        return;
    }

    // plain new[]: make_unique would value-initialize, i.e. needlessly zero out
    // the whole buffer for scalar types
    const std::unique_ptr<T[]> scratch(new T[n]);

    const qsizetype chunkSize = (n + chunks - 1) / chunks;
    const bool startInScratch = (levels & 1);
    T *src = startInScratch ? scratch.get() : array;
    T *dst = startInScratch ? array : scratch.get();

    qParallelRun(chunks, [=, buffer = scratch.get()](int chunk) {
        const qsizetype from = qsizetype(chunk) * chunkSize;
        const qsizetype to = std::min(from + chunkSize, n);

        std::sort(array + from, array + to, lessThan);
        if (startInScratch)
            std::move(array + from, array + to, buffer + from);
    });

    for (int level = 0; level < levels; ++level) {
        const qsizetype runSize = chunkSize << level;
        const int pieces = 2 << level; // pieces per merge: pairs * pieces == chunks

        qParallelRun(chunks, [=](int task) {
            const qsizetype xFrom = qsizetype(task / pieces) * 2 * runSize;
            const qsizetype xTo = std::min(xFrom + runSize, n);
            const qsizetype yTo = std::min(xTo + runSize, n);
            const qsizetype nx = xTo - xFrom;
            const qsizetype ny = yTo - xTo;

            // this thread merges the output range [k0, k1) of this pair
            const int piece = task % pieces;
            const qsizetype pieceSize = ((nx + ny) + pieces - 1) / pieces;
            const qsizetype k0 = std::min(pieceSize * piece, nx + ny);
            const qsizetype k1 = std::min(k0 + pieceSize, nx + ny);
            if (k0 == k1)
                return;

            const T *x = src + xFrom;
            const T *y = src + xTo;
            const qsizetype i0 = qParallelMergeSplit(x, nx, y, ny, k0, lessThan);
            const qsizetype i1 = qParallelMergeSplit(x, nx, y, ny, k1, lessThan);

            std::merge(x + i0, x + i1, y + (k0 - i0), y + (k1 - i1),
                       dst + xFrom + k0, lessThan);
        });
        std::swap(src, dst);
    }
    Q_ASSERT(src == array);
}

template <typename RandomAccessIterator, typename LessThan>
inline void qParallelSortImpl(RandomAccessIterator begin, RandomAccessIterator end, LessThan lessThan)
{
    const qsizetype span = end - begin;
    if (span < 2)
        return;

    // ping-ponging between the array and the scratch buffer needs both to be
    // the same type, so anything not backed by plain memory is sorted serially
    if constexpr (std::contiguous_iterator<RandomAccessIterator>)
        qParallelMergeSortImpl(std::to_address(begin), span, lessThan);
    else
        std::sort(begin, end, lessThan);
}


#if defined(BS_HAS_PARALLEL_STL) && __has_include(<execution>)
#  if defined(emit)
#    undef emit // libtbb uses a function named emit()
#  endif
#  include <execution>
#  if (__cpp_lib_execution >= 201603) && (__cpp_lib_parallel_algorithm >= 201603)
#    define BS_HAS_PARALLEL_STL_EXECUTION
#    include <algorithm>
#  endif
#  if !defined(QT_NO_EMIT)
#    define emit
#  endif
#endif


template <class IT, class LT>
inline constexpr void qParallelSort(const IT begin, const IT end, LT lt)
{
#ifdef BS_HAS_PARALLEL_STL_EXECUTION
    std::sort(std::execution::par_unseq, begin, end, lt);
#else
    qParallelSortImpl(begin, end, lt);
#endif
}

template <class IT>
inline constexpr void qParallelSort(const IT begin, const IT end)
{
    qParallelSort(begin, end, std::less<> { });
}


#ifdef QPARALLELSORT_TESTING // benchmarking
#include "stopwatch.h"

static struct test_par_sort
{
    test_par_sort()
    {
        static constinit bool once = true;
        if (!once)
            return;
        once = false;

        const struct {
            const char *name;
            std::function<void(int *, size_t)> fun;
        } tests[] = {
        { "std::sort           ", [](int *array, size_t count) { std::sort(array, array + count); } },
        { "std::sort par_unseq ", [](int *array, size_t count) { std::sort(std::execution::par_unseq, array, array + count); } },
        { "Parallel Sort       ", [](int *array, size_t count) { qParallelSortImpl(array, array + count, std::less<int>()); } },
        };

        static constexpr int maxloop = 24;

        auto *src = new qint64[1 << maxloop];
        for (int j = 0; j < (1 << maxloop); ++j)
            src[j] = rand();

        for (const auto &test : tests) {
            for (auto i = 0; i <= maxloop; ++i) {
                size_t count = size_t(1ULL << i);
                QByteArray msg = u"%3 test run %1 ... %2 values"_qs.arg(i).arg(count)
                        .arg(QLatin1String(test.name)).toLatin1();
                int *array = new int[count];
                memcpy(array, src, sizeof(int) * count);

                {
                    stopwatch sw(msg.constData());
                    test.fun(array, count);
                }
                for (size_t j = 1; j < count; ++j) {
                    if (array[j - 1] > array[j]) {
                        qWarning("Sort failed at index %zu", j);
                        break;
                    }
                }
                delete [] array;
            }
        }
        delete [] src;
    }
} dummy;

#  endif

