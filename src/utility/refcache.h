// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QtCore/QHash>
#include "ref.h"

template <typename KEY, typename REF>
class RefCache
{
    static_assert(std::is_base_of_v<Ref, REF>);
    Q_DISABLE_COPY_MOVE(RefCache)

public:
    explicit RefCache(uint maxCost = 100) : m_maxCost(maxCost) { }
    inline ~RefCache() { clear(); }

    inline qsizetype size() const { return m_hash.size(); }
    inline bool isEmpty() const { return m_hash.isEmpty(); }

    inline void setMaxCost(uint maxCost) { m_maxCost = maxCost; trim(maxCost); }
    inline uint maxCost() const { return m_maxCost; }
    inline uint totalCost() const { return m_totalCost; }

    inline QList<KEY> keys() const { return m_hash.keys(); }

    qsizetype clear();

    REF *insert(const KEY &key, std::unique_ptr<REF> refPtr, uint cost = 1);
    void setObjectCost(const KEY &key, uint cost);

    REF *object(const KEY &key);
    inline REF *operator[](const KEY &key) { return object(key); }

private:
    void trim(uint downToCost);

    uint m_maxCost = 0;
    uint m_totalCost = 0;

    struct Node {
        std::unique_ptr<REF> m_refPtr;
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

template <typename KEY, typename REF>
qsizetype RefCache<KEY, REF>::clear()
{
    qsizetype s = size();
    while (s) {
        trim(0);
        int sAfterTrim = size();
        if (sAfterTrim == s)
            break;
        s = sAfterTrim;
    }
    return s;
}

template <typename KEY, typename REF>
void RefCache<KEY, REF>::setObjectCost(const KEY &key, uint cost)
{
    if (auto it = m_hash.find(key); it != m_hash.end()) {
        Node *node = *it;

        if (cost != node->m_cost) {
            // this might get us over maxCost!
            m_totalCost -= node->m_cost;
            node->m_cost = cost;
            m_totalCost += node->m_cost;
            trim(m_maxCost);
        }
    }
}

template <typename KEY, typename REF>
REF *RefCache<KEY, REF>::insert(const KEY &key, std::unique_ptr<REF> refPtr, uint cost)
{
    if (!refPtr)
        return nullptr;

    if (auto it = m_hash.find(key); it != m_hash.end()) {
        // we cannot guarantee that the existing entry can be removed!
        refPtr.reset();
        return nullptr;
    }

    // this might get us over maxCost!
    auto *node = new Node;
    node->m_refPtr = std::move(refPtr);
    node->m_cost = cost;
    node->m_key = key;

    m_hash.insert(key, node);
    m_totalCost += cost;
    bump(node);
    trim(m_maxCost);
    return node->m_refPtr.get();
}

template <typename KEY, typename REF>
REF *RefCache<KEY, REF>::object(const KEY &key)
{
    if (auto it = m_hash.find(key); it != m_hash.end()) {
        Node *node = *it;
        unlink(node);
        bump(node);
        return node->m_refPtr.get();
    }
    return nullptr;
}

template <typename KEY, typename REF>
void RefCache<KEY, REF>::trim(uint downToCost)
{
    if (m_totalCost > downToCost) {
        Node *node = m_oldest;

        while (node) {
            if (node->m_refPtr->refCount() == 0) {
                m_totalCost -= node->m_cost;
                m_hash.remove(node->m_key);

                auto nextNode = node->m_newer;
                unlink(node);

                delete node;
                node = nextNode;

                if (m_totalCost <= downToCost)
                    break;
            } else {
                node = node->m_newer;
            }
        }
    }
}
