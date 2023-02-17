// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QStandardPaths>
#include "backendapplication.h"
#include "bricklink/core.h"
#include "utility/exception.h"
#include "rebuilddatabase.h"
#include "version.h"


BackendApplication::BackendApplication(int &argc, char **argv)
{
    QCoreApplication::setApplicationName(QLatin1String(BRICKSTORE_NAME));
    QCoreApplication::setApplicationVersion(QLatin1String(BRICKSTORE_VERSION));

    (void) new QCoreApplication(argc, argv);

    m_clp.addHelpOption();
    m_clp.addVersionOption();
    m_clp.addOption({ u"rebuild-database"_qs, u"Rebuild the BrickLink database (required)."_qs });
    m_clp.addOption({ u"skip-download"_qs, u"Do not download the BrickLink XML database export (optional)."_qs });
    m_clp.process(QCoreApplication::arguments());

    if (!m_clp.isSet(u"rebuild-database"_qs))
        m_clp.showHelp(1);
}

BackendApplication::~BackendApplication()
{ }

void BackendApplication::init()
{
    //TODO5: find out why we are blacklisted ... for now, fake the UA
    Transfer::setDefaultUserAgent(u"Br1ckstore/" + QCoreApplication::applicationVersion()
                                  + u" (" + QSysInfo::prettyProductName() + u')');

    try {
        BrickLink::create(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    } catch (const Exception &e) {
        fprintf(stderr, "Could not initialize the BrickLink kernel:\n%s\n", e.what());
        exit(2);
    }

    auto *rdb = new RebuildDatabase(m_clp.isSet(u"skip-download"_qs), this);

    QMetaObject::invokeMethod(rdb, [rdb]() {
        QCoreApplication::exit(rdb->exec());
    }, Qt::QueuedConnection);
}

void BackendApplication::afterInit()
{ }

void BackendApplication::checkRestart()
{ }

#include "moc_backendapplication.cpp"
