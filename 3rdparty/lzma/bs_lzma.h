/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once

#include <functional>

#include <QCoreApplication>
#include <QCryptographicHash>


class LZMA
{
    Q_DECLARE_TR_FUNCTIONS(MiniZip)

public:
    static QString decompress(const QString &src, const QString &dst,
                              std::function<void (int, int)> progress = { },
                              QCryptographicHash::Algorithm alg = QCryptographicHash::Sha512);
};
