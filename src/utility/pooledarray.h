// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <stdexcept>
#include <concepts>
#include <iterator>
#include <memory_resource>

#include <QString>
#include <QByteArray>


/*TODO: small array optimization:
 *  - align to 2bytes at minimum
 *  - if mr && (num_elements < (sizeof(T *)/sizeof(T)), then treat T *data as T data_[...] and store
 *    the size as (num_elements << 1) | 1) in the lower in data_[0] (check be/le: bit zero of the
 *    pointer has to be '1'). Store the array elements in data_[]
 *
 *  Currently we would save ~1.8MB from ~37MB of allocations ... so right now this isn't worth
 *  the effort and runtime penalty for 5% less allocations.
*/
//extern quint64 possibleSSO;

template<typename T> class PooledArray
{
public:
    explicit PooledArray() = default;

    //template<typename = std::enable_if_t<std::is_same<char16_t, T>::value>>
    void copyQString(const QString &str, std::pmr::memory_resource *mr) requires std::same_as<T, char16_t>
    {
        copyStringLike(str.constData(), str.size(), mr);
    }

    // template<typename = std::enable_if_t<std::is_same<char16_t, T>::value>>
    QString asQString() const requires std::same_as<T, char16_t>
    {
        const auto s = size();
        return (s < 2) ? QString { } : QString::fromRawData(reinterpret_cast<const QChar *>(cbegin()), s - 1);
    }

    // template<typename = std::enable_if_t<std::is_same<char, T>::value>>
    void copyQByteArray(const QByteArray &str, std::pmr::memory_resource *mr) requires std::same_as<T, char>
    {
        copyStringLike(str.constData(), str.size(), mr);
    }

    // template<typename = std::enable_if_t<std::is_same<char, T>::value>>
    QByteArray asQByteArray(qsizetype offset = 0) const requires std::same_as<T, char>
    {
        const auto s = size();
        return (s < (offset + 2)) ? QByteArray { } : QByteArray::fromRawData(cbegin() + offset, s - offset - 1);
    }

    // template<typename InputIt>
    template<std::input_iterator InputIt>
    void copyContainer(InputIt first, InputIt last, std::pmr::memory_resource *mr)
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
    T &operator[](size_t i) { return const_cast<T &>(const_cast<const PooledArray *>(this)->operator[](i)); }

    void resize(qsizetype s, std::pmr::memory_resource *mr)
    {
        if ((s < 0) || (!s && !data) || (data && (s == sizeRef(data))))
            return;

        if (s > std::numeric_limits<typename intTypeFromSize<sizeof(T)>::type>::max())
            throw std::out_of_range("PooledArray resize() too large for undelying data-type");

        std::pmr::polymorphic_allocator<> pool(mr ? mr : std::pmr::get_default_resource());

//        if (s && (s < (sizeof(T *) / sizeof(T))))
//            possibleSSO += ((s + 1) * sizeof(T));

        if (!data) {
            data = pool.allocate_object<T>(s + 1);
            sizeRef(data) = s;
        } else if (!s) {
            pool.deallocate_object(data, sizeRef(data));
            data = nullptr;
        } else {
            Q_ASSERT_X(!mr, "PooledArray", "resize() should not be called twice with an allocator");

            auto newd = pool.allocate_object<T>(s + 1);
            memcpy(newd, data, size());
            sizeRef(newd) = s;
            pool.deallocate_object(data, sizeRef(data));
            data = newd;
        }
    }

    void push_back(const T &t, std::pmr::memory_resource *mr)
    {
        Q_ASSERT_X(!mr, "PooledArray", "Do not call push_back with a non-default allocator");
        const auto s = size();
        resize(s + 1, mr);
        data[s + 1] = t;
    }

private:
    template<int N> struct intTypeFromSize { using type = void; };
    template<int N> requires (N == 1) struct intTypeFromSize<N> { using type = qint8; };
    template<int N> requires (N == 2) struct intTypeFromSize<N> { using type = qint16; };
    template<int N> requires (N == 4) struct intTypeFromSize<N> { using type = qint32; };
    template<int N> requires (N == 8) struct intTypeFromSize<N> { using type = qint64; };

    const typename intTypeFromSize<sizeof(T)>::type &sizeRef(const T *t) const
    {
        assert(t);
        return *reinterpret_cast<const intTypeFromSize<sizeof(T)>::type *>(&t[0]);
    }

    intTypeFromSize<sizeof(T)>::type &sizeRef(T *t)
    {
        assert(t);
        return *reinterpret_cast<intTypeFromSize<sizeof(T)>::type *>(&t[0]);
    }

    void copyStringLike(const void *ptr, qsizetype size, std::pmr::memory_resource *mr)
    {
        if (!ptr || size <= 0) {
            resize(0, mr);
        } else {
            resize(size + 1, mr);
            memcpy(data + 1, ptr, size * sizeof(T));
            data[size + 1] = T();
        }
    }

    T *data = nullptr;
};
