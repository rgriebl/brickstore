// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QByteArray>

namespace PasswordManager {

QByteArray load(const QString &service, const QString &id);
void save(const QString &service, const QString &id, const QByteArray &password);

} // namespace PasswordManager
