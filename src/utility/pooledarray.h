// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <stdexcept>
#include <concepts>
#include <iterator>

#include <QString>
#include <QByteArray>
#include <QDataStream>
#include <QDebug>

#include "memoryresource.h"


/* Small array optimization:
 *  - align to 2bytes at minimum
 *  - if mr && (num_elements < (sizeof(T *)/sizeof(T)), then treat T *data as T data_[...] and store
 *    the size as (num_elements << 1) | 1) in the lower in data_[0] (check be/le: bit zero of the
 *    pointer has to be '1'). Store the array elements in data_[]
 *
 *  Currently we would save ~1.8MB from ~37MB of allocations ... so right now this isn't worth
 *  the effort and runtime penalty for 5% less allocations.
*/

template<typename T> class PooledArray
{
public:
    explicit PooledArray() = default;

    template<typename U = T, std::enable_if_t<std::is_same<char16_t, U>::value, bool> = true>
    void copyQString(const QString &str, MemoryResource *mr) //requires std::same_as<T, char16_t>
    {
        copyStringLike(str.constData(), str.size(), mr);
    }

    template<typename U = T, std::enable_if_t<std::is_same<char16_t, U>::value, bool> = true>
    QString asQString() const //requires std::same_as<T, char16_t>
    {
        const auto s = size();
        return (s < 1) ? QString { } : QString::fromRawData(reinterpret_cast<const QChar *>(cbegin()), s);
    }

    template<typename U = T, std::enable_if_t<std::is_same<char8_t, U>::value, bool> = true>
    void copyQByteArray(const QByteArray &str, MemoryResource *mr) //requires std::same_as<T, char8_t>
    {
        copyStringLike(str.constData(), str.size(), mr);
    }

    template<typename U = T, std::enable_if_t<std::is_same<char8_t, U>::value, bool> = true>
    QByteArray asQByteArray(qsizetype offset = 0) const //requires std::same_as<T, char8_t>
    {
        const auto s = size();
        return (s < (offset + 2)) ? QByteArray { } : QByteArray::fromRawData(reinterpret_cast<const char *>(cbegin()) + offset, s - offset - 1);
    }

    template<typename InputIt>
    void copyContainer(InputIt first, InputIt last, MemoryResource *mr)
    {
        if (first == last) {
            resize(0, mr);
        } else {
            resize(std::distance(first, last), mr);
            std::copy(first, last, &data[1]);
        }
    }

    inline bool isEmpty() const { return size() == 0LL; }
    inline qsizetype size() const { return data ? qsizetype(sizeRef(data)) : 0LL; }
    inline const T *cbegin() const { return data ? &data[1] : nullptr; }
    inline const T *cend() const { return data ? &data[1 + sizeRef(data)] : nullptr; }
    inline const T *begin() const { return cbegin(); }
    inline const T *end() const { return cend(); }

    const T &operator[](size_t i) const
    {
        return (i < size_t(size())) ? *(cbegin() + i)
                                    : throw std::out_of_range("PooledArray operator[] out of range");
    }
    T &operator[](size_t i)
    {
        return const_cast<T &>(const_cast<const PooledArray *>(this)->operator[](i));
    }

    void resize(qsizetype s, MemoryResource *mr)
    {
        if ((s < 0) || (!s && !data) || (data && (s == sizeRef(data))))
            return;

        if (s > maxSize())
            throw std::out_of_range("PooledArray resize() too large for underlying data-type");

        auto amr = mr ? mr : defaultMemoryResource();
        auto dataByteSize = (size() + 1) * sizeof(T);

        if (!data) {
            data = static_cast<T *>(amr->allocate((s + 1) * sizeof(T), alignof(T)));
            sizeRef(data) = static_cast<typename QIntegerForSizeof<T>::Signed>(s);
        } else if (!s) {
            amr->deallocate(data, dataByteSize, alignof(T));
            data = nullptr;
        } else {
            Q_ASSERT_X(!mr, "PooledArray", "resize() should not be called twice with an allocator");

            auto newByteSize = (s + 1) * sizeof(T);
            auto newd = static_cast<T *>(amr->allocate(newByteSize, alignof(T)));
            memcpy(newd, data, std::min(dataByteSize, newByteSize));
            sizeRef(newd) = static_cast<typename QIntegerForSizeof<T>::Signed>(s);
            amr->deallocate(data, dataByteSize, alignof(T));
            data = newd;
        }
    }

    void push_back(const T &t, MemoryResource *mr)
    {
        Q_ASSERT_X(!mr, "PooledArray", "Do not call push_back with a non-default allocator");
        const auto s = size();
        resize(s + 1, mr);
        data[s + 1] = t;
    }

    constexpr qsizetype maxSize() const
    {
        // we need to prevent an overflow on 32bit systems here
        return qsizetype(std::numeric_limits<typename QIntegerForSize<(sizeof(T) < 4) ? sizeof(T) : 4>::Signed>::max());
    }

    std::pair<PooledArray<T> &, MemoryResource *> deserialize(MemoryResource *mr)
    {
        return { *this, mr };
    }

private:
    const typename QIntegerForSizeof<T>::Signed &sizeRef(const T *t) const
    {
        assert(t);
        return *reinterpret_cast<const typename QIntegerForSizeof<T>::Signed *>(&t[0]);
    }

    typename QIntegerForSizeof<T>::Signed &sizeRef(T *t)
    {
        assert(t);
        return *reinterpret_cast<typename QIntegerForSizeof<T>::Signed *>(&t[0]);
    }

    void copyStringLike(const void *ptr, qsizetype size, MemoryResource *mr)
    {
        constexpr qsizetype appendNull = std::is_same<T, char8_t>::value ? 1 : 0;

        if (!ptr || size <= 0) {
            resize(0, mr);
        } else {
            resize(size + appendNull, mr);
            memcpy(data + 1, ptr, size * sizeof(T));
            if (appendNull)
                data[size + 1] = T();
        }
    }

    T *data = nullptr;
};

template<typename T>
QDataStream &operator<<(QDataStream &ds, const PooledArray<T> &pa)
{
    constexpr bool nullAndEmptyDifferent = (std::is_same<T, char16_t>::value || std::is_same<T, char8_t>::value);
    constexpr bool sizeInBytes = std::is_same<T, char16_t>::value;
    constexpr qsizetype appendNull = std::is_same<T, char8_t>::value ? 1 : 0;

    const auto s = std::max(pa.size() - appendNull, qsizetype(0));
    const auto bytes = s * sizeof(T);

    if (!s && nullAndEmptyDifferent) {
        ds << quint32(0xffffffff);
    } else {
        ds << quint32(sizeInBytes ? bytes : s);
        ds.writeRawData(reinterpret_cast<const char *>(pa.cbegin()), uint(bytes));
    }
    return ds;
}

template<typename T>
QDataStream &operator>>(QDataStream &ds, std::pair<PooledArray<T> &, MemoryResource *> pa_mr)
{
    constexpr bool nullAndEmptyDifferent = (std::is_same<T, char16_t>::value || std::is_same<T, char8_t>::value);
    constexpr bool sizeInBytes = std::is_same<T, char16_t>::value;
    constexpr qsizetype appendNull = std::is_same<T, char8_t>::value ? 1 : 0;

    quint32 s;
    ds >> s;

    if ((s == quint32(0xffffffff)) && nullAndEmptyDifferent)
        s = 0;

    qsizetype bytes;
    if constexpr (sizeInBytes) {
        bytes = s;
        s /= sizeof(T);
    } else {
        bytes = s * sizeof(T);
    }

    if ((bytes % sizeof(T)) || (qsizetype(s) > pa_mr.first.maxSize())) {
        pa_mr.first.resize(0, pa_mr.second);
        ds.setStatus(QDataStream::ReadCorruptData);
        return ds;
    }

    pa_mr.first.resize(s ? (s + appendNull) : 0, pa_mr.second);
    if (s) {
        ds.readRawData(reinterpret_cast<char *>(&pa_mr.first[0]), int(bytes));
        if (appendNull)
            pa_mr.first[s] = T();
    }
    return ds;
}
