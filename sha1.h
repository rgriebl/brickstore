#ifndef __SHA1_H__
#define __SHA1_H__

#include <qbytearray.h>

namespace sha1 {
    QByteArray calc ( const QByteArray &data );
    QByteArray calc ( const char *data, unsigned int len );
}

#endif
