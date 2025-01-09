// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QByteArray>

namespace CredentialsManager {

QByteArray load(const QString &service, const QString &id);
void save(const QString &service, const QString &id, const QByteArray &credential);

} // namespace CredentialsManager
