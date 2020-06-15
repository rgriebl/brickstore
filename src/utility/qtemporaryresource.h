/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#ifndef QTEMPORARYRESOURCE_H
#define QTEMPORARYRESOURCE_H

#include <QtGlobal>

QT_BEGIN_HEADER

QT_MODULE(Core)

class QTemporaryResourcePrivate;
class QTemporaryResourceFileEngine;

class QByteArray;
class QString;
class QImage;

class /*Q_CORE_EXPORT*/ QTemporaryResource
{
public:
    static bool registerResource(const QString &key, const QByteArray &value);
    static bool registerResource(const QString &key, const QImage &value);
    static bool unregisterResource(const QString &key);
    static bool isRegisteredResource(const QString &key);

    static void clear();

protected:
    friend class QTemporaryResourceFileEngine;

    static QTemporaryResourcePrivate *d_inst();
    static QTemporaryResourcePrivate *d_ptr;
};

QT_END_HEADER

#endif // QTEMPORARYRESOURCE_H
