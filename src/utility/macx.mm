#include <qglobal.h>
#include "qmacdefines_mac.h"

#if QT_MAC_USE_COCOA
#  include <Cocoa/Cocoa.h>
#else
#  include <Carbon/Carbon.h>
#endif

class QWidget;
extern OSWindowRef qt_mac_window_for(const QWidget *);

namespace MacExtra {

void setWindowShadow(QWidget *w, bool hasShadow)
{
    OSWindowRef wnd = qt_mac_window_for(w);
#if QT_MAC_USE_COCOA
	[wnd setHasShadow:BOOL(hasShadow)];
#else
	if (hasShadow)
		ChangeWindowAttributes(wnd, 0, kWindowNoShadowAttribute);
	else
		ChangeWindowAttributes(wnd, kWindowNoShadowAttribute, 0);
#endif
}

}
