/* Copyright (C) 2004-2008 Robert Griebl.  All rights reserved.
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
#ifndef __CUTILITY_H__
#define __CUTILITY_H__

#include <time.h>

#include <qstring.h>
#include <qcolor.h>

class QFontMetrics;
class QDateTime;

// foreach from Qt4
#undef QT_NO_KEYWORDS

#if defined(Q_CC_GNU) && !defined(Q_CC_INTEL)
/* make use of typeof-extension */
template <typename T>
class QForeachContainer {
public:
    inline QForeachContainer(const T& t) : c(t), brk(0), i(c.begin()), e(c.end()) { }
    const T c;
    int brk;
    typename T::const_iterator i, e;
};

#define Q_FOREACH(variable, container)                                \
for (QForeachContainer<__typeof__(container)> _container_(container); \
     !_container_.brk && _container_.i != _container_.e;              \
     __extension__  ({ ++_container_.brk; ++_container_.i; }))                       \
    for (variable = *_container_.i;; __extension__ ({--_container_.brk; break;}))

#else

struct QForeachContainerBase {};

template <typename T>
class QForeachContainer : public QForeachContainerBase {
public:
    inline QForeachContainer(const T& t): c(t), brk(0), i(c.begin()), e(c.end()){};
    const T c;
    mutable int brk;
    mutable typename T::const_iterator i, e;
    inline bool condition() const { return (!brk++ && i != e); }
};

template <typename T> inline T *qForeachPointer(const T &) { return 0; }

template <typename T> inline QForeachContainer<T> qForeachContainerNew(const T& t)
{ return QForeachContainer<T>(t); }

template <typename T>
inline const QForeachContainer<T> *qForeachContainer(const QForeachContainerBase *base, const T *)
{ return static_cast<const QForeachContainer<T> *>(base); }

#define Q_FOREACH(variable, container) \
    for (const QForeachContainerBase &_container_ = qForeachContainerNew(container); \
         qForeachContainer(&_container_, true ? 0 : qForeachPointer(container))->condition();       \
         ++qForeachContainer(&_container_, true ? 0 : qForeachPointer(container))->i)               \
        for (variable = *qForeachContainer(&_container_, true ? 0 : qForeachPointer(container))->i; \
             qForeachContainer(&_container_, true ? 0 : qForeachPointer(container))->brk;           \
             --qForeachContainer(&_container_, true ? 0 : qForeachPointer(container))->brk)

#endif

#define Q_FOREVER for(;;)
#ifndef QT_NO_KEYWORDS
#  ifndef foreach
#    define foreach Q_FOREACH
#  endif
#  ifndef forever
#    define forever Q_FOREVER
#  endif
#endif

template <typename IT> void qDeleteAll(IT begin, IT end)
{
	while ( begin != end ) {
		delete *begin;
		++begin;
	}
}

template <typename C> inline void qDeleteAll ( const C &c )
{
	qDeleteAll ( c. begin ( ), c. end ( ));
}


class CUtility {
public:
	static QString ellipsisText ( const QString &org, const QFontMetrics &fm, int width, int align );

	static int compareColors ( const QColor &c1, const QColor &c2 );
	static QColor gradientColor ( const QColor &c1, const QColor &c2, float f = 0.5f );
	static QColor contrastColor ( const QColor &c, float f = 0.04f );

	static QImage createGradient ( const QSize &size, Qt::Orientation orient, const QColor &c1, const QColor &c2, float f = 100.f );
	static QImage shadeImage ( const QImage &oimg, const QColor &col );

	static bool openUrl ( const QString &url );
	
	static void setPopupPos ( QWidget *w, const QRect &pos );

	static QString weightToString ( double w, bool imperial = false, bool optimize = false, bool show_unit = false );
	static double stringToWeight ( const QString &s, bool imperial = false );

	static QString safeOpen ( const QString &basepath );
	static QString safeRename ( const QString &basepath );

	static time_t toUTC ( const QDateTime &dt, const char *settz = 0 );
};

#endif

