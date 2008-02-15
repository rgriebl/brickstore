/* Copyright (C) 2004-2005 Robert Griebl.All rights reserved.
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

#include "capplication.h"


int main(int argc, char **argv)
{
    const char *rebuild_db = 0;
    bool show_usage = false;

    if ((argc == 2) && (!strcmp(argv [1], "-h") || !strcmp(argv [1], "--help"))) {
        show_usage = true;
    }
    else if ((argc >= 2) && !strcmp(argv [1], "--rebuild-database")) {
        if ((argc != 3) || !argv [2][0])
            show_usage = true;
        else
            rebuild_db = argv [2];
    }

    if (show_usage) {
#if defined( Q_OS_WIN32 )
        QMessageBox::information(0, "BrickStore", "<b>Usage:</b><br />brickstore.exe [&lt;files&gt;]<br /><br />brickstore.exe --rebuild-database &lt;dbname&gt;<br />");
#else
        printf("Usage: %s [<files>]\n", argv [0]);
        printf("       %s --rebuild-database <dbname>\n", argv [0]);
#endif
        return 1;
    }
    else {
        CApplication a(rebuild_db, argc, argv);
        return a.exec();
    }
}
