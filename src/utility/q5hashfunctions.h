// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <qglobal.h>
#include <QString>
#include <QByteArray>
#include <QStringView>


// This is a copy of qhashfunctions.h / qhashfunctions.cpp from Qt 5.15.2.
// Qt 6 changed the hashing algorithm from crc32 to murmurhash. While murmur is quite
// a bit faster, it generates different hash values. Since we are using the hash value to for
// directory names, we need to keep using crc32 hashes for that.

Q_DECL_PURE_FUNCTION uint q5Hash(const QByteArray &key, uint seed = 0) noexcept;
Q_DECL_PURE_FUNCTION uint q5Hash(QStringView key, uint seed = 0) noexcept;

Q_DECL_CONST_FUNCTION Q_DECL_CONSTEXPR inline uint q5Hash(ulong key, uint seed = 0) Q_DECL_NOTHROW
{
    return (sizeof(ulong) > sizeof(uint))
        ? (uint(((key >> (8 * sizeof(uint) - 1)) ^ key) & (~0U)) ^ seed)
        : (uint(key & (~0U)) ^ seed);
}
