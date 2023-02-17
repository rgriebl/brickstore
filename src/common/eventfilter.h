// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <algorithm>

#include <QObject>
#include <QEvent>
#include <QVector>


class EventFilter : public QObject
{
    Q_OBJECT
public:
    enum ResultFlag {
        ContinueEventProcessing = 0,
        StopEventProcessing     = 1,
        RemoveEventFilter       = 0x1000,
        DeleteEventFilter       = 0x3000,
    };
    Q_ENUM(ResultFlag)
    Q_DECLARE_FLAGS(Result, ResultFlag)
    Q_FLAG(Result)

    typedef std::function<Result(QObject *, QEvent *e)> Type;

    inline explicit EventFilter(QObject *o, std::initializer_list<QEvent::Type> types, Type filter)
        : EventFilter(o, o, types, filter)
    { }

    inline explicit EventFilter(QObject *parent, QObject *o, std::initializer_list<QEvent::Type> types, Type filter)
        : QObject(parent)
        , m_filter(filter)
        , m_types(types)
    {
        Q_ASSERT(o);
        Q_ASSERT(filter);
        if (o)
            o->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    Type m_filter;
    QVector<QEvent::Type> m_types;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(EventFilter::Result)
