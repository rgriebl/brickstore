// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDateTime>
#include <QCoreApplication>

class HumanReadableTimeDelta
{
    Q_DECLARE_TR_FUNCTIONS(HumanReadableTimeDelta)

    HumanReadableTimeDelta() = delete;

public:
    static QString toString(const QDateTime &from, const QDateTime &to);
};

