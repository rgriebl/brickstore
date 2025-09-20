/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Marc Mutz <marc.mutz@kdab.com>
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
 See q3cache.h on why we need this copy of Qt 5.15's QHash.
*/

# pragma once

#include <QtCore/qchar.h>
#include <QtCore/qiterator.h>
#include <QtCore/qlist.h>
#include <QtCore/qrefcount.h>
#include "q5hashfunctions.h"
#include <QtCore/qcontainertools_impl.h>

#include <algorithm>
#include <initializer_list>

#if defined(Q_CC_MSVC)
#pragma warning( push )
#pragma warning( disable : 4311 ) // disable pointer truncation warning
#pragma warning( disable : 4127 ) // conditional expression is constant
#endif

QT_BEGIN_NAMESPACE

struct /*Q_CORE_EXPORT*/ Q5HashData
{
    struct Node {
        Node *next;
        uint h;
    };

    Node *fakeNext;
    Node **buckets;
    QtPrivate::RefCount ref;
    int size;
    int nodeSize;
    short userNumBits;
    short numBits;
    int numBuckets;
    uint seed;
    uint sharable : 1;
    uint strictAlignment : 1;
    uint reserved : 30;

    void *allocateNode(int nodeAlign);
    void freeNode(void *node);
    Q5HashData *detach_helper(void (*node_duplicate)(Node *, void *), void (*node_delete)(Node *),
                             int nodeSize, int nodeAlign);
    bool willGrow();
    void hasShrunk();
    void rehash(int hint);
    void free_helper(void (*node_delete)(Node *));
    Node *firstNode();
#ifdef QT_QHASH_DEBUG
    void dump();
    void checkSanity();
#endif
    static Node *nextNode(Node *node);
    static Node *previousNode(Node *node);

    static const Q5HashData shared_null;
};

inline bool Q5HashData::willGrow()
{
    if (size >= numBuckets) {
        rehash(numBits + 1);
        return true;
    } else {
        return false;
    }
}

inline void Q5HashData::hasShrunk()
{
    if (size <= (numBuckets >> 3) && numBits > userNumBits) {
        QT_TRY {
            rehash(qMax(int(numBits) - 2, int(userNumBits)));
        } QT_CATCH(const std::bad_alloc &) {
            // ignore bad allocs - shrinking shouldn't throw. rehash is exception safe.
        }
    }
}

inline Q5HashData::Node *Q5HashData::firstNode()
{
    Node *e = reinterpret_cast<Node *>(this);
    Node **bucket = buckets;
    int n = numBuckets;
    while (n--) {
        if (*bucket != e)
            return *bucket;
        ++bucket;
    }
    return e;
}

struct Q5HashDummyValue
{
};

constexpr bool operator==(const Q5HashDummyValue &, const Q5HashDummyValue &) noexcept
{
    return true;
}

Q_DECLARE_TYPEINFO(Q5HashDummyValue, Q_MOVABLE_TYPE | Q_DUMMY_TYPE);

template <class Key, class T>
struct Q5HashNode
{
    Q5HashNode *next;
    const uint h;
    const Key key;
    T value;

    inline Q5HashNode(const Key &key0, const T &value0, uint hash, Q5HashNode *n)
        : next(n), h(hash), key(key0), value(value0) {}
    inline bool same_key(uint h0, const Key &key0) const { return h0 == h && key0 == key; }

private:
    Q_DISABLE_COPY(Q5HashNode)
};

// Specialize for Q5HashDummyValue in order to save some memory
template <class Key>
struct Q5HashNode<Key, Q5HashDummyValue>
{
    union {
        Q5HashNode *next;
        Q5HashDummyValue value;
    };
    const uint h;
    const Key key;

    inline Q5HashNode(const Key &key0, const Q5HashDummyValue &, uint hash, Q5HashNode *n)
        : next(n), h(hash), key(key0) {}
    inline bool same_key(uint h0, const Key &key0) const { return h0 == h && key0 == key; }

private:
    Q_DISABLE_COPY(Q5HashNode)
};


#if 0
// ###
// The introduction of the Q5Hash random seed breaks this optimization, as it
// relies on q5Hash(int i) = i. If the hash value is salted, then the hash
// table becomes corrupted.
//
// A bit more research about whether it makes sense or not to salt integer
// keys (and in general keys whose hash value is easy to invert)
// is needed, or about how keep this optimization and the seed (f.i. by
// specializing Q5Hash for integer values, and re-apply the seed during lookup).
//
// Be aware that such changes can easily be binary incompatible, and therefore
// cannot be made during the Qt 5 lifetime.
#define Q_HASH_DECLARE_INT_NODES(key_type) \
    template <class T> \
    struct Q5HashDummyNode<key_type, T> { \
        Q5HashDummyNode *next; \
        union { uint h; key_type key; }; \
\
        inline Q5HashDummyNode(key_type /* key0 */) {} \
    }; \
\
    template <class T> \
    struct Q5HashNode<key_type, T> { \
        Q5HashNode *next; \
        union { uint h; key_type key; }; \
        T value; \
\
        inline Q5HashNode(key_type /* key0 */) {} \
        inline Q5HashNode(key_type /* key0 */, const T &value0) : value(value0) {} \
        inline bool same_key(uint h0, key_type) const { return h0 == h; } \
    }

#if defined(Q_BYTE_ORDER) && Q_BYTE_ORDER == Q_LITTLE_ENDIAN
Q_HASH_DECLARE_INT_NODES(short);
Q_HASH_DECLARE_INT_NODES(ushort);
#endif
Q_HASH_DECLARE_INT_NODES(int);
Q_HASH_DECLARE_INT_NODES(uint);
#undef Q_HASH_DECLARE_INT_NODES
#endif // #if 0

template <class Key, class T> class Q5MultiHash;


template <class Key, class T>
class Q5Hash
{
    typedef Q5HashNode<Key, T> Node;

    union {
        Q5HashData *d;
        Q5HashNode<Key, T> *e;
    };

    static inline Node *concrete(Q5HashData::Node *node) {
        return reinterpret_cast<Node *>(node);
    }

    static inline int alignOfNode() { return qMax<int>(sizeof(void*), Q_ALIGNOF(Node)); }

public:
    inline Q5Hash() noexcept : d(const_cast<Q5HashData *>(&Q5HashData::shared_null)) { }
    inline Q5Hash(std::initializer_list<std::pair<Key,T> > list)
        : d(const_cast<Q5HashData *>(&Q5HashData::shared_null))
    {
        reserve(int(list.size()));
        for (typename std::initializer_list<std::pair<Key,T> >::const_iterator it = list.begin(); it != list.end(); ++it)
            insert(it->first, it->second);
    }
    Q5Hash(const Q5Hash &other) : d(other.d) { d->ref.ref(); if (!d->sharable) detach(); }
    ~Q5Hash() { if (!d->ref.deref()) freeData(d); }

    Q5Hash &operator=(const Q5Hash &other);
    Q5Hash(Q5Hash &&other) noexcept : d(other.d) { other.d = const_cast<Q5HashData *>(&Q5HashData::shared_null); }
    Q5Hash &operator=(Q5Hash &&other) noexcept
    { Q5Hash moved(std::move(other)); swap(moved); return *this; }
#ifdef Q_QDOC
    template <typename InputIterator>
    Q5Hash(InputIterator f, InputIterator l);
#else
    template <typename InputIterator, QtPrivate::IfAssociativeIteratorHasKeyAndValue<InputIterator> = true>
    Q5Hash(InputIterator f, InputIterator l)
        : Q5Hash()
    {
        QtPrivate::reserveIfForwardIterator(this, f, l);
        for (; f != l; ++f)
            insert(f.key(), f.value());
    }

    template <typename InputIterator, QtPrivate::IfAssociativeIteratorHasFirstAndSecond<InputIterator> = true>
    Q5Hash(InputIterator f, InputIterator l)
        : Q5Hash()
    {
        QtPrivate::reserveIfForwardIterator(this, f, l);
        for (; f != l; ++f)
            insert(f->first, f->second);
    }
#endif
    void swap(Q5Hash &other) noexcept { qSwap(d, other.d); }

    bool operator==(const Q5Hash &other) const;
    bool operator!=(const Q5Hash &other) const { return !(*this == other); }

    inline int size() const { return d->size; }

    inline bool isEmpty() const { return d->size == 0; }

    inline int capacity() const { return d->numBuckets; }
    void reserve(int size);
    inline void squeeze() { reserve(1); }

    inline void detach() { if (d->ref.isShared()) detach_helper(); }
    inline bool isDetached() const { return !d->ref.isShared(); }
#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
    inline void setSharable(bool sharable) { if (!sharable) detach(); if (d != &Q5HashData::shared_null) d->sharable = sharable; }
#endif
    bool isSharedWith(const Q5Hash &other) const { return d == other.d; }

    void clear();

    int remove(const Key &key);
    T take(const Key &key);

    bool contains(const Key &key) const;
    const Key key(const T &value) const;
    const Key key(const T &value, const Key &defaultKey) const;
    const T value(const Key &key) const;
    const T value(const Key &key, const T &defaultValue) const;
    T &operator[](const Key &key);
    const T operator[](const Key &key) const;

    QList<Key> keys() const;
    QList<Key> keys(const T &value) const;
    QList<T> values() const;
    int count(const Key &key) const;

    class const_iterator;

    class iterator
    {
        friend class const_iterator;
        friend class Q5Hash<Key, T>;
        friend class QSet<Key>;
        Q5HashData::Node *i;

    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef qptrdiff difference_type;
        typedef T value_type;
        typedef T *pointer;
        typedef T &reference;

        inline iterator() : i(nullptr) { }
        explicit inline iterator(void *node) : i(reinterpret_cast<Q5HashData::Node *>(node)) { }

        inline const Key &key() const { return concrete(i)->key; }
        inline T &value() const { return concrete(i)->value; }
        inline T &operator*() const { return concrete(i)->value; }
        inline T *operator->() const { return &concrete(i)->value; }
        inline bool operator==(const iterator &o) const { return i == o.i; }
        inline bool operator!=(const iterator &o) const { return i != o.i; }

        inline iterator &operator++() {
            i = Q5HashData::nextNode(i);
            return *this;
        }
        inline iterator operator++(int) {
            iterator r = *this;
            i = Q5HashData::nextNode(i);
            return r;
        }
#ifndef QT_STRICT_ITERATORS
    public:
        inline bool operator==(const const_iterator &o) const
            { return i == o.i; }
        inline bool operator!=(const const_iterator &o) const
            { return i != o.i; }
#endif
    };
    friend class iterator;

    class const_iterator
    {
        friend class iterator;
        friend class Q5Hash<Key, T>;
        friend class Q5MultiHash<Key, T>;
        friend class QSet<Key>;
        Q5HashData::Node *i;

    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef qptrdiff difference_type;
        typedef T value_type;
        typedef const T *pointer;
        typedef const T &reference;

        Q_DECL_CONSTEXPR inline const_iterator() : i(nullptr) { }
        explicit inline const_iterator(void *node)
            : i(reinterpret_cast<Q5HashData::Node *>(node)) { }
#ifdef QT_STRICT_ITERATORS
        explicit inline const_iterator(const iterator &o)
#else
        inline const_iterator(const iterator &o)
#endif
        { i = o.i; }

        inline const Key &key() const { return concrete(i)->key; }
        inline const T &value() const { return concrete(i)->value; }
        inline const T &operator*() const { return concrete(i)->value; }
        inline const T *operator->() const { return &concrete(i)->value; }
        Q_DECL_CONSTEXPR inline bool operator==(const const_iterator &o) const { return i == o.i; }
        Q_DECL_CONSTEXPR inline bool operator!=(const const_iterator &o) const { return i != o.i; }

        inline const_iterator &operator++() {
            i = Q5HashData::nextNode(i);
            return *this;
        }
        inline const_iterator operator++(int) {
            const_iterator r = *this;
            i = Q5HashData::nextNode(i);
            return r;
        }

        // ### Qt 5: not sure this is necessary anymore
#ifdef QT_STRICT_ITERATORS
    private:
        inline bool operator==(const iterator &o) const { return operator==(const_iterator(o)); }
        inline bool operator!=(const iterator &o) const { return operator!=(const_iterator(o)); }
#endif
    };
    friend class const_iterator;

    class key_iterator
    {
        const_iterator i;

    public:
        typedef typename const_iterator::iterator_category iterator_category;
        typedef typename const_iterator::difference_type difference_type;
        typedef Key value_type;
        typedef const Key *pointer;
        typedef const Key &reference;

        key_iterator() = default;
        explicit key_iterator(const_iterator o) : i(o) { }

        const Key &operator*() const { return i.key(); }
        const Key *operator->() const { return &i.key(); }
        bool operator==(key_iterator o) const { return i == o.i; }
        bool operator!=(key_iterator o) const { return i != o.i; }

        inline key_iterator &operator++() { ++i; return *this; }
        inline key_iterator operator++(int) { return key_iterator(i++);}
        const_iterator base() const { return i; }
    };

    typedef QKeyValueIterator<const Key&, const T&, const_iterator> const_key_value_iterator;
    typedef QKeyValueIterator<const Key&, T&, iterator> key_value_iterator;

    // STL style
    inline iterator begin() { detach(); return iterator(d->firstNode()); }
    inline const_iterator begin() const { return const_iterator(d->firstNode()); }
    inline const_iterator cbegin() const { return const_iterator(d->firstNode()); }
    inline const_iterator constBegin() const { return const_iterator(d->firstNode()); }
    inline iterator end() { detach(); return iterator(e); }
    inline const_iterator end() const { return const_iterator(e); }
    inline const_iterator cend() const { return const_iterator(e); }
    inline const_iterator constEnd() const { return const_iterator(e); }
    inline key_iterator keyBegin() const { return key_iterator(begin()); }
    inline key_iterator keyEnd() const { return key_iterator(end()); }
    inline key_value_iterator keyValueBegin() { return key_value_iterator(begin()); }
    inline key_value_iterator keyValueEnd() { return key_value_iterator(end()); }
    inline const_key_value_iterator keyValueBegin() const { return const_key_value_iterator(begin()); }
    inline const_key_value_iterator constKeyValueBegin() const { return const_key_value_iterator(begin()); }
    inline const_key_value_iterator keyValueEnd() const { return const_key_value_iterator(end()); }
    inline const_key_value_iterator constKeyValueEnd() const { return const_key_value_iterator(end()); }

    QPair<iterator, iterator> equal_range(const Key &key);
    QPair<const_iterator, const_iterator> equal_range(const Key &key) const noexcept;
    iterator erase(iterator it) { return erase(const_iterator(it.i)); }
    iterator erase(const_iterator it);

    // more Qt
    typedef iterator Iterator;
    typedef const_iterator ConstIterator;
    inline int count() const { return d->size; }
    iterator find(const Key &key);
    const_iterator find(const Key &key) const;
    const_iterator constFind(const Key &key) const;
    iterator insert(const Key &key, const T &value);
    void insert(const Q5Hash &hash);

    // STL compatibility
    typedef T mapped_type;
    typedef Key key_type;
    typedef qptrdiff difference_type;
    typedef int size_type;

    inline bool empty() const { return isEmpty(); }

#ifdef QT_QHASH_DEBUG
    inline void dump() const { d->dump(); }
    inline void checkSanity() const { d->checkSanity(); }
#endif

private:
    void detach_helper();
    void freeData(Q5HashData *d);
    Node **findNode(const Key &key, uint *hp = nullptr) const;
    Node **findNode(const Key &key, uint h) const;
    Node *createNode(uint h, const Key &key, const T &value, Node **nextNode);
    void deleteNode(Node *node);
    static void deleteNode2(Q5HashData::Node *node);

    static void duplicateNode(Q5HashData::Node *originalNode, void *newNode);

    bool isValidIterator(const iterator &it) const noexcept
    { return isValidNode(it.i); }
    bool isValidIterator(const const_iterator &it) const noexcept
    { return isValidNode(it.i); }
    bool isValidNode(Q5HashData::Node *node) const noexcept
    {
#if defined(QT_DEBUG) && !defined(Q_HASH_NO_ITERATOR_DEBUG)
        while (node->next)
            node = node->next;
        return (static_cast<void *>(node) == d);
#else
        Q_UNUSED(node)
        return true;
#endif
    }
    friend class QSet<Key>;
    friend class Q5MultiHash<Key, T>;
};


template <class Key, class T>
Q_INLINE_TEMPLATE void Q5Hash<Key, T>::deleteNode(Node *node)
{
    deleteNode2(reinterpret_cast<Q5HashData::Node*>(node));
    d->freeNode(node);
}

template <class Key, class T>
Q_INLINE_TEMPLATE void Q5Hash<Key, T>::deleteNode2(Q5HashData::Node *node)
{
#ifdef Q_CC_BOR
    concrete(node)->~Q5HashNode<Key, T>();
#else
    concrete(node)->~Node();
#endif
}

template <class Key, class T>
Q_INLINE_TEMPLATE void Q5Hash<Key, T>::duplicateNode(Q5HashData::Node *node, void *newNode)
{
    Node *concreteNode = concrete(node);
    new (newNode) Node(concreteNode->key, concreteNode->value, concreteNode->h, nullptr);
}

template <class Key, class T>
Q_INLINE_TEMPLATE typename Q5Hash<Key, T>::Node *
Q5Hash<Key, T>::createNode(uint ah, const Key &akey, const T &avalue, Node **anextNode)
{
    Node *node = new (d->allocateNode(alignOfNode())) Node(akey, avalue, ah, *anextNode);
    *anextNode = node;
    ++d->size;
    return node;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE void Q5Hash<Key, T>::freeData(Q5HashData *x)
{
    x->free_helper(deleteNode2);
}

template <class Key, class T>
Q_INLINE_TEMPLATE void Q5Hash<Key, T>::clear()
{
    *this = Q5Hash();
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE void Q5Hash<Key, T>::detach_helper()
{
    Q5HashData *x = d->detach_helper(duplicateNode, deleteNode2, sizeof(Node), alignOfNode());
    if (!d->ref.deref())
        freeData(d);
    d = x;
}

template <class Key, class T>
Q_INLINE_TEMPLATE Q5Hash<Key, T> &Q5Hash<Key, T>::operator=(const Q5Hash &other)
{
    if (d != other.d) {
        Q5HashData *o = other.d;
        o->ref.ref();
        if (!d->ref.deref())
            freeData(d);
        d = o;
        if (!d->sharable)
            detach_helper();
    }
    return *this;
}

template <class Key, class T>
Q_INLINE_TEMPLATE const T Q5Hash<Key, T>::value(const Key &akey) const
{
    Node *node;
    if (d->size == 0 || (node = *findNode(akey)) == e) {
        return T();
    } else {
        return node->value;
    }
}

template <class Key, class T>
Q_INLINE_TEMPLATE const T Q5Hash<Key, T>::value(const Key &akey, const T &adefaultValue) const
{
    Node *node;
    if (d->size == 0 || (node = *findNode(akey)) == e) {
        return adefaultValue;
    } else {
        return node->value;
    }
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE QList<Key> Q5Hash<Key, T>::keys() const
{
    QList<Key> res;
    res.reserve(size());
    const_iterator i = begin();
    while (i != end()) {
        res.append(i.key());
        ++i;
    }
    return res;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE QList<Key> Q5Hash<Key, T>::keys(const T &avalue) const
{
    QList<Key> res;
    const_iterator i = begin();
    while (i != end()) {
        if (i.value() == avalue)
            res.append(i.key());
        ++i;
    }
    return res;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE const Key Q5Hash<Key, T>::key(const T &avalue) const
{
    return key(avalue, Key());
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE const Key Q5Hash<Key, T>::key(const T &avalue, const Key &defaultValue) const
{
    const_iterator i = begin();
    while (i != end()) {
        if (i.value() == avalue)
            return i.key();
        ++i;
    }

    return defaultValue;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE QList<T> Q5Hash<Key, T>::values() const
{
    QList<T> res;
    res.reserve(size());
    const_iterator i = begin();
    while (i != end()) {
        res.append(i.value());
        ++i;
    }
    return res;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE int Q5Hash<Key, T>::count(const Key &akey) const
{
    int cnt = 0;
    Node *node = *findNode(akey);
    if (node != e) {
        do {
            ++cnt;
        } while ((node = node->next) != e && node->key == akey);
    }
    return cnt;
}

template <class Key, class T>
Q_INLINE_TEMPLATE const T Q5Hash<Key, T>::operator[](const Key &akey) const
{
    return value(akey);
}

template <class Key, class T>
Q_INLINE_TEMPLATE T &Q5Hash<Key, T>::operator[](const Key &akey)
{
    detach();

    uint h;
    Node **node = findNode(akey, &h);
    if (*node == e) {
        if (d->willGrow())
            node = findNode(akey, h);
        return createNode(h, akey, T(), node)->value;
    }
    return (*node)->value;
}

template <class Key, class T>
Q_INLINE_TEMPLATE typename Q5Hash<Key, T>::iterator Q5Hash<Key, T>::insert(const Key &akey,
                                                                         const T &avalue)
{
    detach();

    uint h;
    Node **node = findNode(akey, &h);
    if (*node == e) {
        if (d->willGrow())
            node = findNode(akey, h);
        return iterator(createNode(h, akey, avalue, node));
    }

    if (!std::is_same<T, Q5HashDummyValue>::value)
        (*node)->value = avalue;
    return iterator(*node);
}

template <class Key, class T>
Q_INLINE_TEMPLATE void Q5Hash<Key, T>::insert(const Q5Hash &hash)
{
    if (d == hash.d)
        return;

    detach();

    Q5HashData::Node *i = hash.d->firstNode();
    Q5HashData::Node *end = reinterpret_cast<Q5HashData::Node *>(hash.e);
    while (i != end) {
        Node *n = concrete(i);
        Node **node = findNode(n->key, n->h);
        if (*node == e) {
            if (d->willGrow())
                node = findNode(n->key, n->h);
            createNode(n->h, n->key, n->value, node);
        } else {
            if (!std::is_same<T, Q5HashDummyValue>::value)
                (*node)->value = n->value;
        }
        i = Q5HashData::nextNode(i);
    }
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE int Q5Hash<Key, T>::remove(const Key &akey)
{
    if (isEmpty()) // prevents detaching shared null
        return 0;
    detach();

    int oldSize = d->size;
    Node **node = findNode(akey);
    if (*node != e) {
        bool deleteNext = true;
        do {
            Node *next = (*node)->next;
            deleteNext = (next != e && next->key == (*node)->key);
            deleteNode(*node);
            *node = next;
            --d->size;
        } while (deleteNext);
        d->hasShrunk();
    }
    return oldSize - d->size;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE T Q5Hash<Key, T>::take(const Key &akey)
{
    if (isEmpty()) // prevents detaching shared null
        return T();
    detach();

    Node **node = findNode(akey);
    if (*node != e) {
        T t = std::move((*node)->value);
        Node *next = (*node)->next;
        deleteNode(*node);
        *node = next;
        --d->size;
        d->hasShrunk();
        return t;
    }
    return T();
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE typename Q5Hash<Key, T>::iterator Q5Hash<Key, T>::erase(const_iterator it)
{
    Q_ASSERT_X(isValidIterator(it), "Q5Hash::erase", "The specified iterator argument 'it' is invalid");

    if (it == const_iterator(e))
        return iterator(it.i);

    if (d->ref.isShared()) {
        // save 'it' across the detach:
        int bucketNum = (it.i->h % d->numBuckets);
        const_iterator bucketIterator(*(d->buckets + bucketNum));
        int stepsFromBucketStartToIte = 0;
        while (bucketIterator != it) {
            ++stepsFromBucketStartToIte;
            ++bucketIterator;
        }
        detach();
        it = const_iterator(*(d->buckets + bucketNum));
        while (stepsFromBucketStartToIte > 0) {
            --stepsFromBucketStartToIte;
            ++it;
        }
    }

    iterator ret(it.i);
    ++ret;

    Node *node = concrete(it.i);
    Node **node_ptr = reinterpret_cast<Node **>(&d->buckets[node->h % d->numBuckets]);
    while (*node_ptr != node)
        node_ptr = &(*node_ptr)->next;
    *node_ptr = node->next;
    deleteNode(node);
    --d->size;
    return ret;
}

template <class Key, class T>
Q_INLINE_TEMPLATE void Q5Hash<Key, T>::reserve(int asize)
{
    detach();
    d->rehash(-qMax(asize, 1));
}

template <class Key, class T>
Q_INLINE_TEMPLATE typename Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::find(const Key &akey) const
{
    return const_iterator(*findNode(akey));
}

template <class Key, class T>
Q_INLINE_TEMPLATE typename Q5Hash<Key, T>::const_iterator Q5Hash<Key, T>::constFind(const Key &akey) const
{
    return const_iterator(*findNode(akey));
}

template <class Key, class T>
Q_INLINE_TEMPLATE typename Q5Hash<Key, T>::iterator Q5Hash<Key, T>::find(const Key &akey)
{
    detach();
    return iterator(*findNode(akey));
}

template <class Key, class T>
Q_INLINE_TEMPLATE bool Q5Hash<Key, T>::contains(const Key &akey) const
{
    return *findNode(akey) != e;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE typename Q5Hash<Key, T>::Node **Q5Hash<Key, T>::findNode(const Key &akey, uint h) const
{
    Node **node;

    if (d->numBuckets) {
        node = reinterpret_cast<Node **>(&d->buckets[h % d->numBuckets]);
        Q_ASSERT(*node == e || (*node)->next);
        while (*node != e && !(*node)->same_key(h, akey))
            node = &(*node)->next;
    } else {
        node = const_cast<Node **>(reinterpret_cast<const Node * const *>(&e));
    }
    return node;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE typename Q5Hash<Key, T>::Node **Q5Hash<Key, T>::findNode(const Key &akey,
                                                                            uint *ahp) const
{
    uint h = 0;

    if (d->numBuckets || ahp) {
        h = q5Hash(akey, d->seed);
        if (ahp)
            *ahp = h;
    }
    return findNode(akey, h);
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE bool Q5Hash<Key, T>::operator==(const Q5Hash &other) const
{
    if (d == other.d)
        return true;
    if (size() != other.size())
        return false;

    const_iterator it = begin();

    while (it != end()) {
        // Build two equal ranges for i.key(); one for *this and one for other.
        // For *this we can avoid a lookup via equal_range, as we know the beginning of the range.
        auto thisEqualRangeStart = it;
        const Key &thisEqualRangeKey = it.key();
        size_type n = 0;
        do {
            ++it;
            ++n;
        } while (it != end() && it.key() == thisEqualRangeKey);

        const auto otherEqualRange = other.equal_range(thisEqualRangeKey);

        if (n != std::distance(otherEqualRange.first, otherEqualRange.second))
            return false;

        // Keys in the ranges are equal by construction; this checks only the values.
        if (!qt_is_permutation(thisEqualRangeStart, it, otherEqualRange.first, otherEqualRange.second))
            return false;
    }

    return true;
}

template <class Key, class T>
QPair<typename Q5Hash<Key, T>::iterator, typename Q5Hash<Key, T>::iterator> Q5Hash<Key, T>::equal_range(const Key &akey)
{
    detach();
    auto pair = qAsConst(*this).equal_range(akey);
    return qMakePair(iterator(pair.first.i), iterator(pair.second.i));
}

template <class Key, class T>
QPair<typename Q5Hash<Key, T>::const_iterator, typename Q5Hash<Key, T>::const_iterator> Q5Hash<Key, T>::equal_range(const Key &akey) const noexcept
{
    Node *node = *findNode(akey);
    const_iterator firstIt = const_iterator(node);

    if (node != e) {
        // equal keys must hash to the same value and so they all
        // end up in the same bucket. So we can use node->next,
        // which only works within a bucket, instead of (out-of-line)
        // Q5HashData::nextNode()
        while (node->next != e && node->next->key == akey)
            node = node->next;

        // 'node' may be the last node in the bucket. To produce the end iterator, we'd
        // need to enter the next bucket in this case, so we need to use
        // Q5HashData::nextNode() here, which, unlike node->next above, can move between
        // buckets.
        node = concrete(Q5HashData::nextNode(reinterpret_cast<Q5HashData::Node *>(node)));
    }

    return qMakePair(firstIt, const_iterator(node));
}

template <class Key, class T>
class Q5MultiHash : public Q5Hash<Key, T>
{
public:
    Q5MultiHash() noexcept {}
    inline Q5MultiHash(std::initializer_list<std::pair<Key,T> > list)
    {
        this->reserve(int(list.size()));
        for (typename std::initializer_list<std::pair<Key,T> >::const_iterator it = list.begin(); it != list.end(); ++it)
            insert(it->first, it->second);
    }
#ifdef Q_QDOC
    template <typename InputIterator>
    Q5MultiHash(InputIterator f, InputIterator l);
#else
    template <typename InputIterator, QtPrivate::IfAssociativeIteratorHasKeyAndValue<InputIterator> = true>
    Q5MultiHash(InputIterator f, InputIterator l)
    {
        QtPrivate::reserveIfForwardIterator(this, f, l);
        for (; f != l; ++f)
            insert(f.key(), f.value());
    }

    template <typename InputIterator, QtPrivate::IfAssociativeIteratorHasFirstAndSecond<InputIterator> = true>
    Q5MultiHash(InputIterator f, InputIterator l)
    {
        QtPrivate::reserveIfForwardIterator(this, f, l);
        for (; f != l; ++f)
            insert(f->first, f->second);
    }
#endif
    // compiler-generated copy/move ctors/assignment operators are fine!
    // compiler-generated destructor is fine!

    Q5MultiHash(const Q5Hash<Key, T> &other) : Q5Hash<Key, T>(other) {}
    Q5MultiHash(Q5Hash<Key, T> &&other) noexcept : Q5Hash<Key, T>(std::move(other)) {}
    void swap(Q5MultiHash &other) noexcept { Q5Hash<Key, T>::swap(other); } // prevent Q5MultiHash<->Q5Hash swaps

    inline typename Q5Hash<Key, T>::iterator replace(const Key &key, const T &value)
    { return Q5Hash<Key, T>::insert(key, value); }

    typename Q5Hash<Key, T>::iterator insert(const Key &key, const T &value);

    inline Q5MultiHash &unite(const Q5MultiHash &other);

    inline Q5MultiHash &operator+=(const Q5MultiHash &other)
    { return unite(other); }
    inline Q5MultiHash operator+(const Q5MultiHash &other) const
    { Q5MultiHash result = *this; result += other; return result; }

    using Q5Hash<Key, T>::contains;
    using Q5Hash<Key, T>::remove;
    using Q5Hash<Key, T>::count;
    using Q5Hash<Key, T>::find;
    using Q5Hash<Key, T>::constFind;
    using Q5Hash<Key, T>::values;
    using Q5Hash<Key, T>::findNode;
    using Q5Hash<Key, T>::createNode;
    using Q5Hash<Key, T>::concrete;
    using Q5Hash<Key, T>::detach;

    using typename Q5Hash<Key, T>::Node;
    using typename Q5Hash<Key, T>::iterator;
    using typename Q5Hash<Key, T>::const_iterator;

    bool contains(const Key &key, const T &value) const;

    int remove(const Key &key, const T &value);

    int count(const Key &key, const T &value) const;

    QList<Key> uniqueKeys() const;

    QList<T> values(const Key &akey) const;

    typename Q5Hash<Key, T>::iterator find(const Key &key, const T &value) {
        typename Q5Hash<Key, T>::iterator i(find(key));
        typename Q5Hash<Key, T>::iterator end(this->end());
        while (i != end && i.key() == key) {
            if (i.value() == value)
                return i;
            ++i;
        }
        return end;
    }
    typename Q5Hash<Key, T>::const_iterator find(const Key &key, const T &value) const {
        typename Q5Hash<Key, T>::const_iterator i(constFind(key));
        typename Q5Hash<Key, T>::const_iterator end(Q5Hash<Key, T>::constEnd());
        while (i != end && i.key() == key) {
            if (i.value() == value)
                return i;
            ++i;
        }
        return end;
    }
    typename Q5Hash<Key, T>::const_iterator constFind(const Key &key, const T &value) const
        { return find(key, value); }
private:
    T &operator[](const Key &key);
    const T operator[](const Key &key) const;
};

template <class Key, class T>
Q_INLINE_TEMPLATE typename Q5Hash<Key, T>::iterator Q5MultiHash<Key, T>::insert(const Key &akey, const T &avalue)
{
    detach();
    this->d->willGrow();

    uint h;
    Node **nextNode = findNode(akey, &h);
    return iterator(createNode(h, akey, avalue, nextNode));
}

template <class Key, class T>
Q_INLINE_TEMPLATE Q5MultiHash<Key, T> &Q5MultiHash<Key, T>::unite(const Q5MultiHash &other)
{
    if (this->d == &Q5HashData::shared_null) {
        *this = other;
    } else {
        const Q5MultiHash copy(other);
        const_iterator it = copy.cbegin();
        const const_iterator end = copy.cend();
        while (it != end) {
            const auto rangeStart = it++;
            while (it != end && rangeStart.key() == it.key())
                ++it;
            const qint64 last = std::distance(rangeStart, it) - 1;
            for (qint64 i = last; i >= 0; --i) {
                auto next = std::next(rangeStart, i);
                insert(next.key(), next.value());
            }
        }
    }
    return *this;
}


template <class Key, class T>
Q_INLINE_TEMPLATE bool Q5MultiHash<Key, T>::contains(const Key &key, const T &value) const
{
    return constFind(key, value) != Q5Hash<Key, T>::constEnd();
}

template <class Key, class T>
Q_INLINE_TEMPLATE int Q5MultiHash<Key, T>::remove(const Key &key, const T &value)
{
    int n = 0;
    typename Q5Hash<Key, T>::iterator i(find(key));
    typename Q5Hash<Key, T>::iterator end(Q5Hash<Key, T>::end());
    while (i != end && i.key() == key) {
        if (i.value() == value) {
            i = this->erase(i);
            ++n;
        } else {
            ++i;
        }
    }
    return n;
}

template <class Key, class T>
Q_INLINE_TEMPLATE int Q5MultiHash<Key, T>::count(const Key &key, const T &value) const
{
    int n = 0;
    typename Q5Hash<Key, T>::const_iterator i(constFind(key));
    typename Q5Hash<Key, T>::const_iterator end(Q5Hash<Key, T>::constEnd());
    while (i != end && i.key() == key) {
        if (i.value() == value)
            ++n;
        ++i;
    }
    return n;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE QList<Key> Q5MultiHash<Key, T>::uniqueKeys() const
{
    QList<Key> res;
    res.reserve(Q5Hash<Key, T>::size()); // May be too much, but assume short lifetime
    typename Q5Hash<Key, T>::const_iterator i = Q5Hash<Key, T>::begin();
    if (i != Q5Hash<Key, T>::end()) {
        for (;;) {
            const Key &aKey = i.key();
            res.append(aKey);
            do {
                if (++i == Q5Hash<Key, T>::end())
                    goto break_out_of_outer_loop;
            } while (aKey == i.key());
        }
    }
break_out_of_outer_loop:
    return res;
}

template <class Key, class T>
Q_OUTOFLINE_TEMPLATE QList<T> Q5MultiHash<Key, T>::values(const Key &akey) const
{
    QList<T> res;
    Node *node = *findNode(akey);
    if (node != this->e) {
        do {
            res.append(node->value);
        } while ((node = node->next) != this->e && node->key == akey);
    }
    return res;
}

#if !defined(QT_NO_JAVA_STYLE_ITERATORS)
template <class Key, class T>
class Q5HashIterator
{
    typedef typename Q5Hash<Key, T>::const_iterator const_iterator;
    typedef const_iterator Item;
    Q5Hash<Key, T> c;
    const_iterator i, n;
    inline bool item_exists() const { return n != c.constEnd(); }

public:
    inline Q5HashIterator(const Q5Hash<Key, T> &container)
        : c(container), i(c.constBegin()), n(c.constEnd())
    {
    }
    inline Q5HashIterator &operator=(const Q5Hash<Key, T> &container)
    {
        c = container;
        i = c.constBegin();
        n = c.constEnd();
        return *this;
    }
    inline void toFront()
    {
        i = c.constBegin();
        n = c.constEnd();
    }
    inline void toBack()
    {
        i = c.constEnd();
        n = c.constEnd();
    }
    inline bool hasNext() const { return i != c.constEnd(); }
    inline Item next()
    {
        n = i++;
        return n;
    }
    inline Item peekNext() const { return i; }
    inline const T &value() const
    {
        Q_ASSERT(item_exists());
        return *n;
    }
    inline const Key &key() const
    {
        Q_ASSERT(item_exists());
        return n.key();
    }
    inline bool findNext(const T &t)
    {
        while ((n = i) != c.constEnd())
            if (*i++ == t)
                return true;
        return false;
    }
};

template<class Key, class T>
class Q5MutableHashIterator
{
    typedef typename Q5Hash<Key, T>::iterator iterator;
    typedef typename Q5Hash<Key, T>::const_iterator const_iterator;
    typedef iterator Item;
    Q5Hash<Key, T> *c;
    iterator i, n;
    inline bool item_exists() const { return const_iterator(n) != c->constEnd(); }

public:
    inline Q5MutableHashIterator(Q5Hash<Key, T> &container) : c(&container)
    {
        i = c->begin();
        n = c->end();
    }
    inline Q5MutableHashIterator &operator=(Q5Hash<Key, T> &container)
    {
        c = &container;
        i = c->begin();
        n = c->end();
        return *this;
    }
    inline void toFront()
    {
        i = c->begin();
        n = c->end();
    }
    inline void toBack()
    {
        i = c->end();
        n = c->end();
    }
    inline bool hasNext() const { return const_iterator(i) != c->constEnd(); }
    inline Item next()
    {
        n = i++;
        return n;
    }
    inline Item peekNext() const { return i; }
    inline void remove()
    {
        if (const_iterator(n) != c->constEnd()) {
            i = c->erase(n);
            n = c->end();
        }
    }
    inline void setValue(const T &t)
    {
        if (const_iterator(n) != c->constEnd())
            *n = t;
    }
    inline T &value()
    {
        Q_ASSERT(item_exists());
        return *n;
    }
    inline const T &value() const
    {
        Q_ASSERT(item_exists());
        return *n;
    }
    inline const Key &key() const
    {
        Q_ASSERT(item_exists());
        return n.key();
    }
    inline bool findNext(const T &t)
    {
        while (const_iterator(n = i) != c->constEnd())
            if (*i++ == t)
                return true;
        return false;
    }
};
#endif // !QT_NO_JAVA_STYLE_ITERATORS

template <class Key, class T>
uint q5Hash(const Q5Hash<Key, T> &key, uint seed = 0)
    noexcept(noexcept(q5Hash(std::declval<Key&>())) && noexcept(q5Hash(std::declval<T&>())))
{
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
    QtPrivate::QHashCombineCommutative hash;
#else
    QtPrivate::QHashCombineCommutative hash(seed);
#endif
    for (auto it = key.begin(), end = key.end(); it != end; ++it) {
        const Key &k = it.key();
        const T   &v = it.value();
        seed = hash(seed, std::pair<const Key&, const T&>(k, v));
    }
    return seed;
}

template <class Key, class T>
inline uint q5Hash(const Q5MultiHash<Key, T> &key, uint seed = 0)
    noexcept(noexcept(q5Hash(std::declval<Key&>())) && noexcept(q5Hash(std::declval<T&>())))
{
    const Q5Hash<Key, T> &key2 = key;
    return q5Hash(key2, seed);
}

QT_END_NAMESPACE

#if defined(Q_CC_MSVC)
#pragma warning( pop )
#endif
