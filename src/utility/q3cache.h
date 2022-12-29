/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#pragma once

/*
 Qt 5's version of QCache lost the qIsDetached check, that would prevent items being trimmed, when
 they are still in use somewhere else. Although this is an obscure feature and messes with the
 max. cost of the cache (you can easily exceed it this way), it is exactly what BrickStore needs
 to keep tabs on the PriceGuide and Picture objects.
 QCache in Qt 3 and Qt 5 are nearly identical (minus the qIsDetached() check), so this file is a
 copy of the Qt 5 QCache with the check added back in.

 Qt 6's versions of QCache and QHash have a race condition when trimming (QTBUG-99710).
 The problem is that our recursiveTrim/q3IsDetached checks do not work with the fixed
 algorithm, because that only works by repeatedly unlinking the last entry. We cannot
 guarantee however, that the last entry CAN be unlinked and this will throw us into
 an endless loop sooner or later.
 
 Both these problems combined result in the need for a copy of Qt 5's QCache with the qIsDetched
 check added back in -> this file, q3cache.h / Q3Cache
 In addition, with Qt 6 we also need a copy of Qt 5's QHash -> q5hash.h / Q5Hash
*/

#include "q5hash.h"
#include <QDebug>

template <typename T> inline bool q3IsDetached(T &) { return true; }



QT_BEGIN_NAMESPACE


template <class Key, class T>
class Q3Cache
{
    struct Node {
        inline Node() : keyPtr(nullptr) {}
        inline Node(T *data, int cost)
            : keyPtr(nullptr), t(data), c(cost), p(nullptr), n(nullptr) {}
        const Key *keyPtr; T *t; int c; Node *p,*n;
    };
    Node *f, *l;
    Q5Hash<Key, Node> hash;
    int mx, total;

    inline void unlink(Node &n) {
        if (n.p) n.p->n = n.n;
        if (n.n) n.n->p = n.p;
        if (l == &n) l = n.p;
        if (f == &n) f = n.n;
        total -= n.c;
        T *obj = n.t;
        hash.remove(*n.keyPtr);
        delete obj;
    }
    inline T *relink(const Key &key) {
        typename Q5Hash<Key, Node>::iterator i = hash.find(key);
        if (typename Q5Hash<Key, Node>::const_iterator(i) == hash.constEnd())
            return nullptr;

        Node &n = *i;
        if (f != &n) {
            if (n.p) n.p->n = n.n;
            if (n.n) n.n->p = n.p;
            if (l == &n) l = n.p;
            n.p = nullptr;
            n.n = f;
            f->p = &n;
            f = &n;
        }
        return n.t;
    }

    Q_DISABLE_COPY(Q3Cache)

public:
    inline explicit Q3Cache(int maxCost = 100) Q_DECL_NOTHROW;
    inline ~Q3Cache() { clear(); }

    inline int maxCost() const { return mx; }
    void setMaxCost(int m);
    inline int totalCost() const { return total; }

    inline int size() const { return hash.size(); }
    inline int count() const { return hash.size(); }
    inline bool isEmpty() const { return hash.isEmpty(); }
    inline QList<Key> keys() const { return hash.keys(); }

    void clear();

    bool insert(const Key &key, T *object, int cost = 1);
    T *object(const Key &key) const;
    inline bool contains(const Key &key) const { return hash.contains(key); }
    T *operator[](const Key &key) const;

    bool remove(const Key &key);
    T *take(const Key &key);

    void setObjectCost(const Key &key, int cost);
    int clearRecursive();

private:
    void trim(int m);
};

template <class Key, class T>
inline Q3Cache<Key, T>::Q3Cache(int amaxCost) Q_DECL_NOTHROW
    : f(nullptr), l(nullptr), mx(amaxCost), total(0) {}

template <class Key, class T>
inline void Q3Cache<Key,T>::clear()
{ while (f) { delete f->t; f = f->n; }
 hash.clear(); l = nullptr; total = 0; }

template <class Key, class T>
inline void Q3Cache<Key,T>::setMaxCost(int m)
{ mx = m; trim(mx); }

template <class Key, class T>
inline T *Q3Cache<Key,T>::object(const Key &key) const
{ return const_cast<Q3Cache<Key,T>*>(this)->relink(key); }

template <class Key, class T>
inline T *Q3Cache<Key,T>::operator[](const Key &key) const
{ return object(key); }

template <class Key, class T>
inline bool Q3Cache<Key,T>::remove(const Key &key)
{
    typename Q5Hash<Key, Node>::iterator i = hash.find(key);
    if (typename Q5Hash<Key, Node>::const_iterator(i) == hash.constEnd()) {
        return false;
    } else {
        unlink(*i);
        return true;
    }
}

template <class Key, class T>
inline T *Q3Cache<Key,T>::take(const Key &key)
{
    typename Q5Hash<Key, Node>::iterator i = hash.find(key);
    if (i == hash.end())
        return nullptr;

    Node &n = *i;
    T *t = n.t;
    n.t = nullptr;
    unlink(n);
    return t;
}

template <class Key, class T>
bool Q3Cache<Key,T>::insert(const Key &akey, T *aobject, int acost)
{
    remove(akey);
    if (acost > mx) {
        delete aobject;
        return false;
    }
    trim(mx - acost);
    Node sn(aobject, acost);
    typename Q5Hash<Key, Node>::iterator i = hash.insert(akey, sn);
    total += acost;
    Node *n = &i.value();
    n->keyPtr = &i.key();
    if (f) f->p = n;
    n->n = f;
    f = n;
    if (!l) l = f;
    return true;
}

template <class Key, class T>
void Q3Cache<Key,T>::trim(int m)
{
    Node *n = l;
    while (n && total > m) {
        Node *u = n;
        n = n->p;
        if (q3IsDetached(*u->t))
            unlink(*u);
    }
}

template<class Key, class T>
int Q3Cache<Key, T>::clearRecursive()
{
    int s = size();
    while (s) {
        trim(0);
        int new_s = size();
        if (new_s == s)
            break;
        s = new_s;
    }
    if (s) {
        // we cannot clear(), as this deletes objects that may still be in use, we HAVE TO leak them

        const auto leakKeys = keys();
        for (const auto &key : leakKeys)
            (void) take(key);
    }
    return s;
}

template <class Key, class T>
inline void Q3Cache<Key,T>::setObjectCost(const Key &key, int cost)
{
    // Reduce the cost if possible. Increasing is not possible, because the cache might
    // overflow, leaving us in a weird state.
    auto i = hash.find(key);
    if (i != hash.end()) {
        int d = cost - (*i).c;
        /*if ((d > 0) && ((total + d) > mx)) {
            qWarning() << "Q3Cache: adjusting cache object cost by" << d << "would overflow the cache";
        } else*/ if (d != 0) {
            (*i).c = cost;
            total += d;
            //qWarning() << "Adjusted cache object cost by" << d;
        }
    }
}



QT_END_NAMESPACE
