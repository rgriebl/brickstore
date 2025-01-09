// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QObject>
#include <QVariantMap>


class SystemInfo : public QObject
{
    Q_OBJECT

public:
    static SystemInfo *inst();
    ~SystemInfo() override;
    Q_INVOKABLE QVariantMap asMap() const;
    Q_INVOKABLE QVariant value(const QString &key) const;
    quint64 physicalMemory() const;
    Q_INVOKABLE QString qtDiag() const;

private:
    SystemInfo();
    Q_DISABLE_COPY(SystemInfo)

    static SystemInfo *s_inst;

    QVariantMap m_map;
};
