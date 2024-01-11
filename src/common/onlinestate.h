// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>


class OnlineState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool online READ isOnline NOTIFY onlineStateChanged)

public:
    static OnlineState *inst();
    inline bool isOnline() const { return m_online; }

signals:
    void onlineStateChanged(bool isOnline);

private:
    OnlineState(QObject *parent = nullptr);

    bool m_online = true;
    static OnlineState *s_inst;
};
