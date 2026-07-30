// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <utility>

#include <QtCore/QHash>
#include <QtCore/QList>


// A cost bounded LRU cache of shared objects.
//
// The cache is just one more owner of the objects it holds: everything it hands out stays alive for
// as long as the caller needs it, whether it is still cached or not. Callers therefore never have to
// care about cache membership - that is a pure optimization.
//
// Only objects that nobody else references can be evicted, so exceeding maxCost is a valid state: it
// simply means everything is in use, and those objects would occupy that memory with or without this
// cache. The budget therefore governs how much the cache retains on its own behalf.

template <typename KEY, typename VALUE>
class Cache
{
    Q_DISABLE_COPY_MOVE(Cache)

public:
    using Ptr = std::shared_ptr<VALUE>;

    explicit Cache(uint maxCost = 100) : m_maxCost(maxCost) { }
    ~Cache() { clear(); }

    inline qsizetype size() const { return m_hash.size(); }
    inline bool isEmpty() const { return m_hash.isEmpty(); }

    inline void setMaxCost(uint maxCost) { m_maxCost = maxCost; trimTo(maxCost); }
    inline uint maxCost() const { return m_maxCost; }
    inline uint totalCost() const { return m_totalCost; }

    inline QList<KEY> keys() const { return m_hash.keys(); }

    void clear();

    // Trimming only happens when the total cost grows, so a bulk operation that has just released a
    // lot of objects can use this to hand the memory back before the next insert would.
    inline void trim() { trimTo(m_maxCost); }

    Ptr insert(const KEY &key, Ptr ptr, uint cost = 1);
    void setObjectCost(const KEY &key, uint cost);

    Ptr object(const KEY &key);
    inline Ptr operator[](const KEY &key) { return object(key); }

private:
    void trimTo(uint downToCost);

    uint m_maxCost = 0;
    uint m_totalCost = 0;

    struct Node {
        Ptr m_ptr;
        Node *m_older = nullptr;
        Node *m_newer = nullptr;
        uint m_cost = 0;
        KEY m_key = { };
    };

    QHash<KEY, Node *> m_hash;
    Node *m_oldest = nullptr;
    Node *m_newest = nullptr;

    void unlink(Node *node)
    {
        // unlink towards older
        if (node->m_older) {
            node->m_older->m_newer = node->m_newer;
        } else {
            Q_ASSERT(m_oldest == node);
            m_oldest = node->m_newer;
        }
        // unlink towards newer
        if (node->m_newer) {
            node->m_newer->m_older = node->m_older;
        } else {
            Q_ASSERT(m_newest == node);
            m_newest = node->m_older;
        }
        node->m_older = nullptr;
        node->m_newer = nullptr;
    }

    void bump(Node *node)
    {
        // must be unlinked or new
        Q_ASSERT(!node->m_older);
        Q_ASSERT(!node->m_newer);

        // insert at newest position
        node->m_newer = nullptr;
        node->m_older = m_newest;
        if (m_newest)
            m_newest->m_newer = node;
        m_newest = node;

        // if this node is the only one, it's also the oldest
        if (!m_oldest)
            m_oldest = node;
    }
};


template <typename KEY, typename VALUE>
void Cache<KEY, VALUE>::clear()
{
    // Unconditional, unlike trimTo(): objects that are still referenced elsewhere just outlive the
    // cache. There is no destruction order to get right either, as the handles sort that out among
    // themselves. Detach the state first, in case destroying an object calls back into the cache.
    auto hash = std::move(m_hash);
    m_hash.clear();
    m_oldest = m_newest = nullptr;
    m_totalCost = 0;

    for (auto it = hash.cbegin(); it != hash.cend(); ++it)
        delete it.value();
}

template <typename KEY, typename VALUE>
void Cache<KEY, VALUE>::trimTo(uint downToCost)
{
    // Destroying an object may drop its own references to other cached objects - a part releases its
    // sub-parts, for example - and those may be older than the object we just evicted, i.e. behind
    // the current position of the walk. Repeat until a full pass cannot free anything anymore.
    bool evictedSomething = true;

    while (evictedSomething && (m_totalCost > downToCost)) {
        evictedSomething = false;
        Node *node = m_oldest;

        while (node && (m_totalCost > downToCost)) {
            Node *nextNode = node->m_newer;

            // a use count of 1 means that this cache holds the only reference, so nobody is using it
            if (node->m_ptr.use_count() == 1) {
                m_totalCost -= node->m_cost;
                m_hash.remove(node->m_key);
                unlink(node);
                delete node;
                evictedSomething = true;
            }
            node = nextNode;
        }
    }
}

template <typename KEY, typename VALUE>
void Cache<KEY, VALUE>::setObjectCost(const KEY &key, uint cost)
{
    if (auto it = m_hash.constFind(key); it != m_hash.cend()) {
        Node *node = *it;

        if (cost != node->m_cost) {
            m_totalCost -= node->m_cost;
            node->m_cost = cost;
            m_totalCost += node->m_cost;
            trimTo(m_maxCost);
        }
    }
}

template <typename KEY, typename VALUE>
typename Cache<KEY, VALUE>::Ptr Cache<KEY, VALUE>::insert(const KEY &key, Ptr ptr, uint cost)
{
    if (!ptr)
        return { };

    // Never replace an existing entry: the whole point of the cache is that a key maps to exactly
    // one object, no matter how many callers are holding on to it.
    if (auto it = m_hash.constFind(key); it != m_hash.cend())
        return (*it)->m_ptr;

    auto *node = new Node;
    node->m_ptr = std::move(ptr);
    node->m_cost = cost;
    node->m_key = key;

    m_hash.insert(key, node);
    m_totalCost += cost;
    bump(node);

    // Holding the handle keeps the fresh entry from being evicted right away by its own trim, and
    // makes the return value valid even if it were.
    Ptr inserted = node->m_ptr;
    trimTo(m_maxCost);
    return inserted;
}

template <typename KEY, typename VALUE>
typename Cache<KEY, VALUE>::Ptr Cache<KEY, VALUE>::object(const KEY &key)
{
    if (auto it = m_hash.constFind(key); it != m_hash.cend()) {
        Node *node = *it;
        unlink(node);
        bump(node);
        return node->m_ptr;
    }
    return { };
}
