// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include "backendapplication.h"
#include <QtCore/QStandardPaths>
#include "bricklink/core.h"
#include "bricklink/textimport.h"
#include "utility/exception.h"
#include "utility/transfer.h"
#include "version.h"
#include <cstdio>

BackendApplication::BackendApplication(int &argc, char **argv)
{
    // disable buffering on stdout
    setvbuf(stdout, nullptr, _IONBF, 0);

    QCoreApplication::setApplicationName(QString::fromLatin1(BRICKSTORE_NAME));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(BRICKSTORE_VERSION));

    (void) new QCoreApplication(argc, argv);

    m_clp.addHelpOption();
    m_clp.addOption({ { u"v"_qs, u"version"_qs }, u"Display version information."_qs });
    m_clp.addOption({ u"rebuild-database"_qs, u"Rebuild the BrickLink database (required)."_qs });
    m_clp.addOption({ u"skip-download"_qs, u"Do not download the BrickLink XML database export (optional)."_qs });
    m_clp.process(QCoreApplication::arguments());

    if (m_clp.isSet(u"version"_qs)) {
        puts(BRICKSTORE_NAME " " BRICKSTORE_VERSION " (" BRICKSTORE_BUILD_NUMBER ")");
        exit(0);
    }

    if (!m_clp.isSet(u"rebuild-database"_qs))
        m_clp.showHelp(1);
}

BackendApplication::~BackendApplication()
{
    delete BrickLink::core();
}

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

    QMetaObject::invokeMethod(this, [this]() -> QCoro::Task<> {
            QCoreApplication::exit(co_await rebuildDatabase());
        }, Qt::QueuedConnection);
}

QCoro::Task<int> BackendApplication::rebuildDatabase()
{
    printf("\n Rebuilding database ");
    printf("\n=====================\n");

    BrickLink::core();
    bool skipDownload = m_clp.isSet(u"skip-download"_qs);
    BrickLink::TextImport blti;

    try {
        blti.initialize(skipDownload);

        const QString affiliateApiKey = qEnvironmentVariable("BRICKLINK_AFFILIATE_APIKEY");
        if (affiliateApiKey.isEmpty())
            throw Exception("Missing BrickLink Affiliate API key: please set $BRICKLINK_AFFILIATE_APIKEY.");
        blti.setApiKeys({ { "affiliate", affiliateApiKey } });

        auto apiQuirks = BrickLink::Core::knownApiQuirks();
        // remove any fixed quirks here
        blti.setApiQuirks(apiQuirks);

        if (!skipDownload) {
            const QString rebrickableApiKey = qEnvironmentVariable("REBRICKABLE_APIKEY");
            if (rebrickableApiKey.isEmpty())
                throw Exception("Missing Rebrickable API key: please set $REBRICKABLE_APIKEY.");

            const QString username = qEnvironmentVariable("BRICKLINK_USERNAME");
            const QString password = qEnvironmentVariable("BRICKLINK_PASSWORD");
            if (username.isEmpty() || password.isEmpty())
                throw Exception("Missing BrickLink login credentials: please set $BRICKLINK_USERNAME and $BRICKLINK_PASSWORD.");

            co_await blti.login(username, password, rebrickableApiKey);
        }

        co_await blti.importCatalog();
        co_await blti.importInventories();

        blti.finalize();
        blti.exportDatabase();

    } catch (const Exception &e) {
        QString error = e.errorString();
        if (error.isEmpty())
            printf("\n /!\\ FAILED.\n");
        else
            printf("\n /!\\ FAILED: %s\n", qPrintable(error));

        co_return 2;
    }

    printf("\n FINISHED.\n\n");
    co_return 0;
}

int BackendApplication::exec()
{
    return QCoreApplication::exec();
}

#include "moc_backendapplication.cpp"
