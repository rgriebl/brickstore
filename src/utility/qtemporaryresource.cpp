/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#include "qtemporaryresource.h"
#include "qtemporaryresource_p.h"
#include <QHash>
#include <QtGlobal>
#include <QVector>
#include <QDateTime>
#include <QBuffer>
#include <QImage>
#include <QByteArray>
#include <QStringList>
//#include <qshareddata.h>
//#include <qplatformdefs.h>
//#include "private/qabstractfileengine_p.h"

class QTemporaryResourcePrivate {
public:
    QHash<QString, QByteArray> contents;
};

QTemporaryResourcePrivate *QTemporaryResource::d_ptr = 0;

QTemporaryResourcePrivate *QTemporaryResource::d_inst()
{
    if (!d_ptr)
        d_ptr = new QTemporaryResourcePrivate;
    return d_ptr;
}

void QTemporaryResource::clear ( )
{
    d_inst()->contents.clear();
}

bool QTemporaryResource::registerResource(const QString &key, const QByteArray &value)
{
    QString realkey = key;
    if (key. startsWith(QLatin1String("#/")))
        realkey = key.mid(2);

    if (realkey.isEmpty())
        return false;

    d_inst()->contents.insert(realkey, value);
    return true;
}

bool QTemporaryResource::registerResource(const QString &key, const QImage &image)
{
    if (image.isNull())
        return unregisterResource(key);

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

   return registerResource(key, ba);
}

bool QTemporaryResource::unregisterResource(const QString &key)
{
    QString realkey = key;
    if (key. startsWith(QLatin1String("#/")))
        realkey = key.mid(2);

    if (realkey.isEmpty())
        return false;

    return d_inst()->contents.remove(realkey) > 0;
}

bool QTemporaryResource::isRegisteredResource(const QString &key)
{
    QString realkey = key;
    if (key. startsWith(QLatin1String("#/")))
        realkey = key.mid(2);

    if (realkey.isEmpty())
        return false;

    return d_inst()->contents.contains(realkey);
}

//file type handler
class QTemporaryResourceFileEngineHandler : public QAbstractFileEngineHandler
{
public:
    QTemporaryResourceFileEngineHandler() { }
    ~QTemporaryResourceFileEngineHandler() { }
    QAbstractFileEngine *create(const QString &path) const;
};
QAbstractFileEngine *QTemporaryResourceFileEngineHandler::create(const QString &path) const
{
    if (path.size() > 0 && path.startsWith(QLatin1String("#/")))
        return new QTemporaryResourceFileEngine(path);
    return 0;
}

//resource engine
class QTemporaryResourceFileEnginePrivate //: public QAbstractFileEnginePrivate
{
protected:
//    Q_DECLARE_PUBLIC(QTemporaryResourceFileEngine)
    friend class QTemporaryResourceFileEngine;
private:
    QString key;
    QByteArray value;
    qint64 offset;
    bool valid;
protected:
    QTemporaryResourceFileEnginePrivate() : offset(0), valid(false) { }
};

bool QTemporaryResourceFileEngine::mkdir(const QString &, bool) const
{
    return false;
}

bool QTemporaryResourceFileEngine::rmdir(const QString &, bool) const
{
    return false;
}

bool QTemporaryResourceFileEngine::setSize(qint64)
{
    return false;
}

QStringList QTemporaryResourceFileEngine::entryList(QDir::Filters, const QStringList &) const
{
    return QStringList();
}

bool QTemporaryResourceFileEngine::caseSensitive() const
{
    return true;
}

QTemporaryResourceFileEngine::QTemporaryResourceFileEngine(const QString &file) :
//    QAbstractFileEngine(*new QTemporaryResourceFileEnginePrivate)
    QAbstractFileEngine(), d(new QTemporaryResourceFileEnginePrivate)
{
    setFileName(file);
}

QTemporaryResourceFileEngine::~QTemporaryResourceFileEngine()
{
}

void QTemporaryResourceFileEngine::setFileName(const QString &file)
{
//    Q_D(QTemporaryResourceFileEngine);

    d->key = file;
    if (d->key.startsWith(QLatin1String("#/")))
        d->key = d->key.mid(2);
}

bool QTemporaryResourceFileEngine::open(QIODevice::OpenMode flags)
{
//    Q_D(QTemporaryResourceFileEngine);
    if (d->key.isEmpty()) {
        qWarning("QTemporaryResourceFileEngine::open: Missing file name");
        return false;
    }
    if (flags & QIODevice::WriteOnly)
        return false;
    if (!QTemporaryResource::d_inst()->contents.contains(d->key))
       return false;

    d->valid = true;
    d->value = QTemporaryResource::d_inst()->contents.value(d->key);
    return true;
}

bool QTemporaryResourceFileEngine::close()
{
    return true;
}

bool QTemporaryResourceFileEngine::flush()
{
    return false;
}

qint64 QTemporaryResourceFileEngine::read(char *data, qint64 len)
{
//    Q_D(QTemporaryResourceFileEngine);
    if(len > d->value.size()-d->offset)
        len = d->value.size()-d->offset;
    if(len <= 0)
        return 0;
    memcpy(data, d->value.constData()+d->offset, len);
    d->offset += len;
    return len;
}

qint64 QTemporaryResourceFileEngine::write(const char *, qint64)
{
    return -1;
}

bool QTemporaryResourceFileEngine::remove()
{
    return false;
}

bool QTemporaryResourceFileEngine::copy(const QString &)
{
    return false;
}

bool QTemporaryResourceFileEngine::rename(const QString &)
{
    return false;
}

bool QTemporaryResourceFileEngine::link(const QString &)
{
    return false;
}

qint64 QTemporaryResourceFileEngine::size() const
{
//    Q_D(const QTemporaryResourceFileEngine);
    if(!d->valid)
        return 0;
    return d->value.size();
}

qint64 QTemporaryResourceFileEngine::pos() const
{
//    Q_D(const QTemporaryResourceFileEngine);
    return d->offset;
}

bool QTemporaryResourceFileEngine::atEnd() const
{
//    Q_D(const QTemporaryResourceFileEngine);
    if(!d->valid)
        return true;
    return d->offset == d->value.size();
}

bool QTemporaryResourceFileEngine::seek(qint64 pos)
{
//    Q_D(QTemporaryResourceFileEngine);
    if(!d->valid)
        return false;

    if(d->offset > d->value.size())
        return false;
    d->offset = pos;
    return true;
}

bool QTemporaryResourceFileEngine::isSequential() const
{
    return false;
}

QAbstractFileEngine::FileFlags QTemporaryResourceFileEngine::fileFlags(QAbstractFileEngine::FileFlags type) const
{
//    Q_D(const QTemporaryResourceFileEngine);
    QAbstractFileEngine::FileFlags ret = 0;
    if(!d->valid)
        return ret;

    if(type & PermsMask)
        ret |= QAbstractFileEngine::FileFlags(ReadOwnerPerm|ReadUserPerm|ReadGroupPerm|ReadOtherPerm);
    if(type & TypesMask)
        ret |= FileType;
    if(type & FlagsMask)
        ret |= ExistsFlag;
    return ret;
}

bool QTemporaryResourceFileEngine::setPermissions(uint)
{
    return false;
}

QString QTemporaryResourceFileEngine::fileName(FileName) const
{
//    Q_D(const QTemporaryResourceFileEngine);
    return d->key;
}

bool QTemporaryResourceFileEngine::isRelativePath() const
{
    return false;
}

uint QTemporaryResourceFileEngine::ownerId(FileOwner) const
{
    static const uint nobodyID = (uint) -2;
    return nobodyID;
}

QString QTemporaryResourceFileEngine::owner(FileOwner) const
{
    return QString();
}

QDateTime QTemporaryResourceFileEngine::fileTime(FileTime) const
{
    return QDateTime();
}

bool QTemporaryResourceFileEngine::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
    Q_UNUSED(extension);
    Q_UNUSED(option);
    Q_UNUSED(output);
    return false;
}

bool QTemporaryResourceFileEngine::supportsExtension(Extension extension) const
{
    Q_UNUSED(extension);
    return false;
}

//Initialization and cleanup
Q_GLOBAL_STATIC(QTemporaryResourceFileEngineHandler, resource_file_handler)

static int qt_force_resource_init() { resource_file_handler(); return 1; }
/*Q_CORE_EXPORT*/ void qInitTemporaryResourceIO() { resource_file_handler(); }
static int qt_forced_resource_init = qt_force_resource_init();
Q_CONSTRUCTOR_FUNCTION(qt_force_resource_init)



