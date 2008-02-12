/****************************************************************************
**
** Copyright (C) 1992-$THISYEAR$ $TROLLTECH$. All rights reserved.
**
** This file is part of the $MODULE$ of the Qt Toolkit.
**
** $TROLLTECH_DUAL_LICENSE$
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef QTEMPORARYRESOURCE_H
#define QTEMPORARYRESOURCE_H

#include <QtCore/qglobal.h>

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

    static void clear();

protected:
    friend class QTemporaryResourceFileEngine;

    static QTemporaryResourcePrivate *d_inst();
    static QTemporaryResourcePrivate *d_ptr;
};

QT_END_HEADER

#endif // QTEMPORARYRESOURCE_H
