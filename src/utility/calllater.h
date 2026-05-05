// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>
#include <tuple>

#include <QObject>
#include <QTimer>

template<typename ...Args>
class CallLater : public QObject
{
public:
    CallLater(std::chrono::milliseconds delay = std::chrono::milliseconds(0),
              QObject *parent = nullptr)
        : QObject(parent)
        , m_timer(new QTimer(this))
    {
        m_timer->setInterval(delay);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            if (!!m_callback)
                std::apply(m_callback, m_arguments);
        });
    }

    operator bool() const
    {
        return !!m_callback;
    }

    void set(std::function<void(Args...)> &&callback)
    {
        Q_ASSERT(!m_callback);
        m_callback = std::move(callback);
    };

    void operator()(Args... args)
    {
        m_arguments = std::make_tuple(std::forward<Args>(args)...);
        m_timer->start();
    }

private:
    QTimer *m_timer;
    std::function<void(Args...)> m_callback;
    std::tuple<Args...> m_arguments;
};
