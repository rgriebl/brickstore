/* Copyright (C) 2004-2011 Robert Griebl. All rights reserved.
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
#include <cstdio>
#include <cstring>

#include <QMessageBox>

#include "application.h"


// needed for themed common controls (e.g. file open dialogs)
#if defined(Q_CC_MSVC)
#  pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#if defined(STATIC_QT_BUILD) // this should be needed for linking against a static Qt, but it works without
#include <QtPlugin>
Q_IMPORT_PLUGIN(qjpeg)
Q_IMPORT_PLUGIN(qgif)
#endif

int main(int argc, char **argv)
{
    bool rebuild_db = false;
    bool show_usage = false;

    if ((argc == 2) && (!strcmp(argv [1], "-h") || !strcmp(argv [1], "--help"))) {
        show_usage = true;
    }
    else if ((argc >= 2) && !strcmp(argv [1], "--rebuild-database")) {
        rebuild_db = true;
        show_usage = (argc != 2); // bail out if a db name is specified on the cmd line (BS1.1)
    }

    if (show_usage) {
#if defined(Q_OS_WIN32)
        QApplication a(argc, argv);
        QMessageBox::information(0, QLatin1String("BrickStore"), QLatin1String("<b>Usage:</b><br />brickstore.exe [&lt;files&gt;]<br /><br />brickstore.exe --rebuild-database<br />"));
#else
        printf("Usage: %s [<files>]\n", argv [0]);
        printf("       %s --rebuild-database\n", argv [0]);
#endif
        return 1;
    }
    else {
        Application a(rebuild_db, argc, argv);
        return a.exec();
    }
}
