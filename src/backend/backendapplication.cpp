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
#include <QtCore/QStandardPaths>
#include "utility/utility.h"
#include "backendapplication.h"
#include "bricklink/core.h"
#include "rebuilddatabase.h"
#include "version.h"


BackendApplication::BackendApplication(int &argc, char **argv)
{
    QCoreApplication::setApplicationName(QLatin1String(BRICKSTORE_NAME));
    QCoreApplication::setApplicationVersion(QLatin1String(BRICKSTORE_VERSION));

    (void) new QCoreApplication(argc, argv);

    m_clp.addHelpOption();
    m_clp.addVersionOption();
    m_clp.addOption({ "rebuild-database"_l1, "Rebuild the BrickLink database (required)."_l1 });
    m_clp.addOption({ "skip-download"_l1, "Do not download the BrickLink XML database export (optional)."_l1 });
    m_clp.process(QCoreApplication::arguments());

    if (!m_clp.isSet("rebuild-database"_l1))
        m_clp.showHelp(1);
}

BackendApplication::~BackendApplication()
{ }

void BackendApplication::init()
{
    //TODO5: find out why we are blacklisted ... for now, fake the UA
    Transfer::setDefaultUserAgent("Br1ckstore"_l1 % u'/' % QCoreApplication::applicationVersion()
                                  % u" (" + QSysInfo::prettyProductName() % u')');

    QString errstring;
    BrickLink::Core *bl = BrickLink::create(QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
                                            &errstring);

    if (!bl) {
        fprintf(stderr, "Could not initialize the BrickLink kernel:\n%s\n", qPrintable(errstring));
        exit(2);
    }

    auto *rdb = new RebuildDatabase(m_clp.isSet("skip-download"_l1), this);

    QMetaObject::invokeMethod(rdb, [rdb]() {
        QCoreApplication::exit(rdb->exec());
    }, Qt::QueuedConnection);
}

void BackendApplication::afterInit()
{ }

void BackendApplication::checkRestart()
{ }

#include "moc_backendapplication.cpp"
