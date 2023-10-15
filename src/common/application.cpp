// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QtCore/QFileInfo>
#include <QtCore/QEvent>
#include <QtCore/QTranslator>
#include <QtCore/QSysInfo>
#include <QtCore/QStringBuilder>
#include <QtCore/QTimer>
#include <QtCore/QDirIterator>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMutexLocker>
#include <QtCore/QStandardPaths>
#include <QtNetwork/QNetworkProxyFactory>
#include <QtGui/QGuiApplication>
#include <QtGui/QFont>
#include <QtGui/QPalette>
#include <QtGui/QFileOpenEvent>
#include <QtGui/QPixmapCache>
#include <QtGui/QImageReader>
#include <QtGui/QImageWriter>
#include <QtGui/QDesktopServices>
#include <QtGui/QWindow>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuick3D/QQuick3D>
#if defined(BS_MOBILE)
#  include <QtQuickControls2Impl/private/qquickiconimage_p.h>
#endif

#include <QCoro/QCoroSignal>
#include <memory>

#include "bricklink/core.h"
#include "bricklink/store.h"
#include "bricklink/cart.h"
#include "bricklink/wantedlist.h"
#include "common/actionmanager.h"
#include "common/announcements.h"
#include "common/application.h"
#include "common/config.h"
#include "common/document.h"
#include "common/documentio.h"
#include "common/documentlist.h"
#include "common/itemscanner.h"
#include "common/onlinestate.h"
#include "common/recentfiles.h"
#include "common/uihelpers.h"
#include "ldraw/library.h"
#include "common/brickstore_wrapper.h"
#include "common/currency.h"
#include "common/eventfilter.h"
#include "utility/exception.h"
#include "common/systeminfo.h"
#include "utility/transfer.h"
#include "common/undo.h"
#include "version.h"

#if defined(SENTRY_ENABLED)
#  include "sentry.h"
#  include "version.h"
Q_LOGGING_CATEGORY(LogSentry, "sentry")
#endif

#if defined(Q_OS_UNIX) || defined(Q_CC_MINGW)
#  define HAS_CXXABI 1
#  include <cxxabi.h>
#endif
#if defined(Q_OS_MACOS)
extern bool macDecodeNSException();
#endif

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(LogQml, "qml")


Application *Application::s_inst = nullptr;


Application::Application(int &argc, char **argv)
    : QObject()
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)

#if (defined(Q_OS_MACOS) || defined(Q_OS_IOS)) && (QT_VERSION < QT_VERSION_CHECK(6, 5, 2))
    // the System locale on macOS is ~1000 times slower than the built-in ones for toString()
    QString sysLocaleName = QLocale::system().name();
    QLocale::setDefault(QLocale(sysLocaleName));
    qInfo() << "Enabled workaround for slow macOS system locale. System:" << sysLocaleName << ", mapped to:" << QLocale().name();
#endif

    s_inst = this;

    QCoreApplication::setApplicationName(u"" BRICKSTORE_NAME ""_qs);
    QCoreApplication::setApplicationVersion(u"" BRICKSTORE_VERSION ""_qs);
    QGuiApplication::setApplicationDisplayName(QCoreApplication::applicationName());

//    QDirIterator dit(u":/"_qs, QDirIterator::Subdirectories);
//    while (dit.hasNext())
//        qWarning() << dit.next();
}

void Application::init()
{
    QSurfaceFormat::setDefaultFormat(QQuick3D::idealSurfaceFormat());

    new EventFilter(qApp, { QEvent::FileOpen }, [this](QObject *, QEvent *e) {
        const QString file = static_cast<QFileOpenEvent *>(e)->file();
        if (m_canEmitOpenDocuments)
            emit openDocument(file);
        else
            m_queuedDocuments.append(file);
        return EventFilter::StopEventProcessing;
    });
    new EventFilter(qApp, { QEvent::LanguageChange }, [this](QObject *, QEvent *) {
        emit languageChanged();
        return EventFilter::ContinueEventProcessing;
    });

    setupTerminateHandler();
    setupLogging();
    setupSentry();

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    //TODO5: find out why we are blacklisted ... for now, fake the UA
    Transfer::setDefaultUserAgent(u"Br1ckstore/" + QCoreApplication::applicationVersion()
                                  + u" (" + QSysInfo::prettyProductName() + u')');

#if defined(Q_CC_MSVC)
    // std::set_terminate is a per-thread setting on Windows
    Transfer::setInitFunction([]() { Application::setupTerminateHandler(); });
#endif

    // initialize config & resource
    (void) Config::inst()->upgrade(BRICKSTORE_MAJOR, BRICKSTORE_MINOR, BRICKSTORE_PATCH);
    checkSentryConsent();

    QIcon::setThemeSearchPaths({ u":/assets/icons"_qs });

    (void) OnlineState::inst();
    (void) Currency::inst();
    (void) SystemInfo::inst();
    (void) RecentFiles::inst();

    connect(RecentFiles::inst(), &RecentFiles::openDocument,
            this, &Application::openDocument);
    connect(this, &Application::openDocument,
            this, &Document::load);

    updateTranslations();
    connect(Config::inst(), &Config::languageChanged,
            Application::inst(), &Application::updateTranslations);

    // sanity check - we might run into endless update loops later on if one of these is missing
    const auto readFmts = QImageReader::supportedImageFormats();
    const auto writeFmts = QImageWriter::supportedImageFormats();
    if (!readFmts.contains("png") || !readFmts.contains("jpg") || !readFmts.contains("gif")
            || !readFmts.contains("webp") || !writeFmts.contains("webp")) {
        m_startupErrors << tr("Your installation is broken: image format plugins are missing!");
    }

    try {
        initBrickLink();
    } catch (const Exception &e) {
        m_startupErrors << tr("Could not initialize the BrickLink kernel:") + u' ' + e.errorString();
    }

    if (QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier) {
        // clean start:
        //  - remove the database
        //  - clear the last open documents
        //  - clear the autosaves
        BrickLink::Database::remove();
        Document::processAutosaves(Document::AutosaveAction::Delete);
        Config::inst()->remove(u"MainWindow/LastSessionDocuments"_qs);
    }

    try {
        BrickLink::core()->database()->read();
    } catch (const Exception &) {
        // this is not a critical error, but expected on the first run, so just ignore it
    }

    LDraw::create(ldrawUrl());

    connect(BrickLink::core(), &BrickLink::Core::authenticationFailed,
            this, [](const QString &userName, const QString &error) {
        UIHelpers::warning(tr("Failed to authenticate with BrickLink as user %1")
                            .arg(userName) + u"<br><b>" + error + u"</b>");
    });

    m_undoGroup = new UndoGroup(this);

    auto *am = ActionManager::inst();
    connect(this, &Application::languageChanged,
            am, &ActionManager::retranslate);
    am->retranslate();

    qInfo() << "UI:";
    qInfo() << "  Device pixel ratio:" << qApp->devicePixelRatio()
            << "/" << QGuiApplication::highDpiScaleFactorRoundingPolicy();
    const auto screenSize = QGuiApplication::primaryScreen()->size();
    const auto screenMMSize = QGuiApplication::primaryScreen()->physicalSize();
    qInfo() << "  Screen size in pix:" << screenSize.width() << "x" << screenSize.height();
    qInfo() << "  Screen size in mm :" << screenMMSize.width() << "x" << screenMMSize.height();

    setupQml();
}

void Application::afterInit()
{
    if (!m_startupErrors.isEmpty()) {
        QCoro::waitFor(UIHelpers::critical(m_startupErrors.join(u"\n\n")));

        // we cannot call quit directly, since there is no event loop to quit from...
        QMetaObject::invokeMethod(this, &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }
    if (!m_startupMessages.isEmpty()) {
        QCoro::waitFor(UIHelpers::information(m_startupMessages.join(u"\n\n")));
    }

    ActionManager::ActionTable applicationActionTable = {
        { "document_new", [](auto) { new Document(); } },
        { "document_open", [](auto) { Document::load(); } },
        { "document_import_bl_xml", [](auto) { DocumentIO::importBrickLinkXML(); } },
        { "document_import_ldraw_model", [](auto) { DocumentIO::importLDrawModel(); } },
        { "document_import_bl_store_inv", [this](auto) -> QCoro::Task<> {
              if (!co_await checkBrickLinkLogin())
                  co_return;

              auto store = BrickLink::core()->store();
              if (store->updateStatus() == BrickLink::UpdateStatus::Updating)
                  co_return;  // no caching right now

              bool success = co_await UIHelpers::progressDialog(tr("Import BrickLink Store"),
                                                                tr("Importing BrickLink Store"),
                                                                store,
                                                                &BrickLink::Store::updateProgress,
                                                                &BrickLink::Store::updateFinished,
                                                                &BrickLink::Store::startUpdate,
                                                                &BrickLink::Store::cancelUpdate);

              if (success && store->isValid())
                  DocumentIO::importBrickLinkStore(store);
          } },
        { "view_show_input_errors", [](bool b) {
              Config::inst()->setShowInputErrors(b);
              ActionManager::inst()->qAction("view_goto_next_input_error")->setEnabled(b);
          } },
        { "view_show_diff_indicators", [](bool b) {
              Config::inst()->setShowDifferenceIndicators(b);
              ActionManager::inst()->qAction("view_goto_next_diff")->setEnabled(b);
          } },
        { "configure", [this](auto) { emit showSettings(); } },
        { "help_extensions", [](auto) {
              QString url = u"https://" + Application::inst()->gitHubPagesUrl() + u"/extensions/";
              openUrl(url);
          } },
        { "help_reportbug", [](auto) {
              QString url = u"https://" + Application::inst()->gitHubUrl() + u"/issues/new";
              openUrl(url);
          } },
        { "help_releasenotes", [](auto) {
              QString url = u"https://" + Application::inst()->gitHubUrl() + u"/releases";
              openUrl(url);
          } },
        { "help_announcements", [](auto) {
              QString url = Application::inst()->announcements()->announcementsWikiUrl();
              openUrl(url);
          } },
    };

    ActionManager::inst()->connectActionTable(applicationActionTable);

    connect(DocumentList::inst(), &DocumentList::documentCreated,
            this, [this](Document *document) {
        m_undoGroup->addStack(document->model()->undoStack());
    });

    if (!BrickLink::core()->database()->isValid()
            || (BrickLink::core()->database()->isUpdateNeeded() && OnlineState::inst()->isOnline())) {
        if (!QCoro::waitFor(updateDatabase())) {
            if (!BrickLink::core()->database()->isValid())
                QCoro::waitFor(UIHelpers::warning(tr("Could not load the BrickLink database files.<br /><br />The program is not functional without these files.")));
        }
    }
    if (BrickLink::core()->database()->isValid()) {
        openQueuedDocuments();

        // restore autosaves and/or last session
        QMetaObject::invokeMethod(this, [this]() { restoreLastSession(); }, Qt::QueuedConnection);

        //TODO: if we haven't opened any documents, but the DB is outdated, we might want to
        //      retry updating it after a few seconds. The problem here is, that OnlineState uses
        //      a background thread which takes at least 5sec to update.

        //        if (BrickLink::core()->database()->isUpdateNeeded() && !OnlineState::inst()->isOnline()) {
        //            QTimer::singleShot(6000, this, []() { // 6000 might not be enough, but is actually to long
        //                if (DocumentList::inst()->count() == 0) {
        //                    if (OnlineState::inst()->isOnline()) {
        //                        updateDatabase();
        //                    }
        //                }
        //            });
        //        }
    }

    connect(Currency::inst(), &Currency::updateRatesFailed,
            this, [](const QString &errorString) {
        UIHelpers::toast(errorString);
    });

    Currency::inst()->updateRates();
    auto *currencyUpdateTimer = new QTimer(this);
    currencyUpdateTimer->start(4h);
    currencyUpdateTimer->callOnTimeout(Currency::inst(),
                                       []() { Currency::inst()->updateRates(true /*silent*/); });

    QMetaObject::invokeMethod(this, [this]() { setupLDraw(); }, Qt::QueuedConnection);

    ItemScanner::inst();
}

QCoro::Task<> Application::restoreLastSession()
{
    bool autosavesRestored = false;
    int restorable = Document::restorableAutosaves();
    if (restorable > 0) {
        bool b = (co_await UIHelpers::question(tr("It seems like BrickStore crashed while %n document(s) had unsaved modifications.", nullptr, restorable)
                                               + u"<br><br>" + tr("Should these documents be restored from their last available auto-save state?"),
                                               UIHelpers::Yes | UIHelpers::No, UIHelpers::Yes, tr("Restore Documents"))
                  == UIHelpers::Yes);

        int restoredCount = Document::processAutosaves(b ? Document::AutosaveAction::Restore
                                                         : Document::AutosaveAction::Delete);
        autosavesRestored = (restoredCount > 0);
    }

    if (!autosavesRestored) {
        if (Config::inst()->restoreLastSession()) {
            const auto files = Config::inst()->value(u"MainWindow/LastSessionDocuments"_qs).toStringList();
            for (const auto &file : files)
                Document::load(file);
        }
    }
}

QCoro::Task<> Application::setupLDraw()
{
    auto ldrawDir = Config::inst()->ldrawDir();

    if (Config::inst()->value(u"General/LDrawTransition"_qs).toBool()) {
        if (ldrawDir.isEmpty())
            ldrawDir = LDraw::Library::potentialLDrawDirs().value(0);

        auto msg = tr("The way BrickStore uses LDraw to render 3D models for parts has changed: "
                      "by default it will now download and maintain its own LDraw installation."
                      "<br><br>Please check the LDraw page in Settings if you still want to use "
                      "a custom LDraw installation.");

        UIHelpers::information(msg);

        ldrawDir.clear();
        Config::inst()->remove(u"General/LDrawTransition"_qs);
        Config::inst()->setLDrawDir(ldrawDir);
    }

    connect(LDraw::library(), &LDraw::Library::updateStarted,
            this, []() {
        UIHelpers::toast(tr("Started downloading an LDraw library update"));
    });
    connect(LDraw::library(), &LDraw::Library::updateFinished,
            this, [](bool success, const QString &message) {
        if (success)
            UIHelpers::toast(tr("Finished downloading an LDraw library update"));
        else
            UIHelpers::toast(tr("Failed to download a LDraw library update") + u":<br>" + message);
    });

    auto loadLibrary = [](QString ldrawDirLoad) -> QCoro::Task<> {
        bool isInternalZip = ldrawDirLoad.isEmpty();

        if (isInternalZip)
            ldrawDirLoad = Config::inst()->cacheDir() + u"/ldraw/complete.zip";

        co_await LDraw::library()->setPath(ldrawDirLoad);

        if (isInternalZip)
            LDraw::library()->startUpdate();
    };

    connect(Config::inst(), &Config::ldrawDirChanged,
            this, loadLibrary);

    return loadLibrary(ldrawDir);
}

void Application::mimeClipboardClear()
{
    QGuiApplication::clipboard()->clear();
#if defined(BS_MOBILE)
    m_clipboardMimeData.reset();
#endif
}

const QMimeData *Application::mimeClipboardGet() const
{
#if defined(BS_MOBILE)
    return m_clipboardMimeData.get();
#else
    return QGuiApplication::clipboard()->mimeData();
#endif
}

void Application::mimeClipboardSet(QMimeData *data)
{
#if defined(BS_MOBILE)
    mimeClipboardClear();
    if (!data)
        return;
    if (data->hasText())
        QGuiApplication::clipboard()->setText(data->text());
    m_clipboardMimeData.reset(data);
#else
    QGuiApplication::clipboard()->setMimeData(data);
#endif
}

QCoro::Task<bool> Application::closeAllDocuments()
{
    const auto docs = DocumentList::inst()->documents();

    for (const auto doc : docs) {
        if (doc->model()->isModified()) {
            // bring a View of the doc to the front, preferably in the active ViewPane

            emit doc->requestActivation();
        }
        if (!co_await doc->requestClose())
            co_return false;
    }
    co_return true;
}

Application::~Application()
{
    delete BrickLink::core();
    delete LDraw::library();
    delete SystemInfo::inst();
    delete Currency::inst();
//    delete DocumentList::inst();
    delete Config::inst();

    s_inst = nullptr;
    delete m_app;

    shutdownSentry();
}

QString Application::buildNumber() const
{
    return u"" BRICKSTORE_BUILD_NUMBER ""_qs;
}

QString Application::applicationUrl() const
{
    return u"" BRICKSTORE_URL ""_qs;
}

QString Application::gitHubUrl() const
{
    return u"" BRICKSTORE_GITHUB_URL ""_qs;
}

QString Application::gitHubPagesUrl() const
{
    const QStringList sections = QString::fromLatin1(BRICKSTORE_GITHUB_URL).split(u'/');
    Q_ASSERT(sections.count() == 3);
    return sections[1] + u".github.io/" + sections[2];
}

QString Application::databaseUrl() const
{
    return u"" BRICKSTORE_DATABASE_URL ""_qs;
}

QString Application::ldrawUrl() const
{
    return u"" BRICKSTORE_DATABASE_URL ""_qs;
}

void Application::openUrl(const QUrl &url)
{
    // decouple from the UI, necessary on iOS
    QMetaObject::invokeMethod(qApp, [=]() { QDesktopServices::openUrl(url); }, Qt::QueuedConnection);
}

void Application::checkRestart()
{ }

QCoro::Task<bool> Application::checkBrickLinkLogin()
{
    if (!Config::inst()->brickLinkUsername().isEmpty()) {
        if (!Config::inst()->brickLinkPassword().isEmpty()) {
            co_return true;
        } else {
            if (auto pw = co_await UIHelpers::getPassword(tr("Please enter the password for the BrickLink account %1:")
                                                          .arg(u"<b>" + Config::inst()->brickLinkUsername() + u"</b>"))) {
                Config::inst()->setBrickLinkPassword(*pw, true /*do not save*/);
                co_return true;
            }
        }
    } else {
        if (co_await UIHelpers::question(tr("No valid BrickLink login settings found.<br /><br />Do you want to change the settings now?")
                                         ) == UIHelpers::Yes) {
            emit showSettings(u"bricklink"_qs);
        }
    }
    co_return false;
}

QCoro::Task<bool> Application::updateDatabase()
{
    if (BrickLink::core()->database()->updateStatus() == BrickLink::UpdateStatus::Updating)
        co_return false;

    //TODO: block UI here

    QStringList files = DocumentList::inst()->allFiles();

    if (co_await closeAllDocuments()) {
        if (DocumentList::inst()->count())
            co_await qCoro(DocumentList::inst(), &DocumentList::lastDocumentClosed);

        if (DocumentList::inst()->count())
            co_return false;

        bool success = co_await UIHelpers::progressDialog(tr("Update Database"),
                                                          tr("Updating the BrickLink database"),
                                                          BrickLink::core()->database(),
                                                          &BrickLink::Database::updateProgress,
                                                          &BrickLink::Database::updateFinished,
                                                          qOverload<>(&BrickLink::Database::startUpdate),
                                                          &BrickLink::Database::cancelUpdate);
        for (const auto &file : files)
            Document::load(file);

        co_return success;
    }
    co_return false;
}

Announcements *Application::announcements()
{
    if (!m_announcements) {
        m_announcements = new Announcements(gitHubUrl(), this);
        m_announcements->check();
        auto checkTimer = new QTimer(this);
        checkTimer->callOnTimeout(this, [this]() { m_announcements->check(); });
        checkTimer->start(12h);
    }
    return m_announcements;
}

UndoGroup *Application::undoGroup()
{
    return m_undoGroup;
}

QQmlApplicationEngine *Application::qmlEngine()
{
    return m_engine;
}

void Application::raise()
{
    const auto tlWindows = qApp->topLevelWindows();
    if (!tlWindows.isEmpty()) {
        QWindow *win = tlWindows.constFirst();
        if (win->windowState() == Qt::WindowMinimized)
            win->setWindowStates(win->windowStates() & ~Qt::WindowMinimized);
        win->raise();
        if (!win->isActive())
            win->requestActivate();
    }
}

void Application::setupTerminateHandler()
{
    std::set_terminate([]() {
        char buffer [1024];

        std::exception_ptr e = std::current_exception();

        if (e) {
            const char *typeName = nullptr;
#if defined(HAS_CXXABI)
            static size_t demangleBufferSize = 768;
            static char *demangleBuffer = static_cast<char *>(malloc(demangleBufferSize));

            if (auto type = abi::__cxa_current_exception_type()) {
                typeName = type->name();
                if (typeName) {
                    int status;
                    demangleBuffer = abi::__cxa_demangle(typeName, demangleBuffer, &demangleBufferSize, &status);
                    if (status == 0 && *demangleBuffer)
                        typeName = demangleBuffer;
                }
            }
#endif
#if defined(Q_OS_MACOS)
            if (typeName && (strcmp(typeName, "NSException") == 0)) {
                // we cannot get the reason from the C++ side, so have to get it via Obj-C
                macDecodeNSException();
                abort();
            }
#endif
            try {
                std::rethrow_exception(e);
            } catch (const std::exception &exc) {
                snprintf(buffer, sizeof(buffer), "uncaught exception of type %s (%s)", typeName ? typeName : typeid(exc).name(), exc.what());
            } catch (const std::exception *exc) {
                snprintf(buffer, sizeof(buffer), "uncaught exception of type %s (%s)", typeName ? typeName : typeid(exc).name(), exc->what());
            } catch (const char *exc) {
                snprintf(buffer, sizeof(buffer), "uncaught exception of type 'const char *' (%s)", exc);
            } catch (...) {
                snprintf(buffer, sizeof(buffer), "uncaught exception of type %s", typeName ? typeName : "<unknown type>");
            }
        } else {
            snprintf(buffer, sizeof(buffer), "terminate was called although no exception was thrown");
        }
        qWarning().noquote() << buffer;
        abort();
    });
}

void Application::openQueuedDocuments()
{
    m_canEmitOpenDocuments = true;

    for (const auto &f : std::as_const(m_queuedDocuments))
        QCoreApplication::postEvent(qApp, new QFileOpenEvent(f), Qt::LowEventPriority);
    m_queuedDocuments.clear();
}

void Application::updateTranslations()
{
    QString language = Config::inst()->language();
    if (language.isEmpty())
        return;

    QString i18n = u":/translations"_qs;

    static bool once = false; // always load english
    if (!once) {
        auto transQt = new QTranslator(this);
        if (transQt->load(u"qtbase_en"_qs, i18n))
            QCoreApplication::installTranslator(transQt);

        auto transBrickStore = new QTranslator(this);
        if (transBrickStore->load(u"brickstore_en"_qs, i18n))
            QCoreApplication::installTranslator(transBrickStore);
        once = true;
    }

    m_trans_qt = std::make_unique<QTranslator>();
    m_trans_brickstore = std::make_unique<QTranslator>();

    if (language != u"en") {
        if (m_trans_qt->load(u"qtbase_"_qs + language, i18n))
            QCoreApplication::installTranslator(m_trans_qt.get());
    }
    if ((language != u"en") || (!m_translationOverride.isEmpty())) {
        QString translationFile = u"brickstore_"_qs + language;
        QString translationDir = i18n;

        QFileInfo qm(m_translationOverride);
        if (qm.isReadable()) {
            translationFile = qm.fileName();
            translationDir = qm.absolutePath();
        }

        if (m_trans_brickstore->load(translationFile, translationDir))
            QCoreApplication::installTranslator(m_trans_brickstore.get());
    }
}

QVariantMap Application::about() const
{
    QString header = uR"(<p style="line-height: 150%;">)"_qs
            + uR"(<span style="font-size: large"><b>)" BRICKSTORE_NAME "</b></span><br>"
            + u"<b>" + tr("Version %1 (build: %2)").arg(BRICKSTORE_VERSION u"", BRICKSTORE_BUILD_NUMBER u"")
            + u"</b><br>"
            + tr("Copyright &copy; %1").arg(BRICKSTORE_COPYRIGHT u"") + u"<br>"
            + tr("Visit %1").arg(uR"(<a href="https://)" BRICKSTORE_URL R"(">)" BRICKSTORE_URL R"(</a>)")
            + u"</p>";

    auto gplLink = uR"(<a href="https://www.gnu.org/licenses/gpl-3.0.html">GNU GPL version 3</a>)";
    auto sourceLink = uR"(<a href="https://)" u"" BRICKSTORE_GITHUB_URL uR"(">GitHub</a>)";
    auto trademarkLink = uR"(<a href="https://www.bricklink.com">www.bricklink.com</a>)";
    auto danLink = uR"(<a href="https://www.danjezek.com/">Dan Jezek</a>)";

    auto gpl = tr("BrickStore is free software licensed under the %1.").arg(gplLink);
    auto source = tr("The source code is available on %1.").arg(sourceLink);
    auto trademark = tr("All data from %1 is owned by BrickLink. Both BrickLink and LEGO are trademarks of the LEGO group, which does not sponsor, authorize or endorse this software. All other trademarks are the property of their respective owners.").arg(trademarkLink);
    auto dan = tr("Only made possible by the support of %1.").arg(danLink);
    QString license = u"<b>" + tr("License") + uR"(</b><div style="margin-left: 10px"><p>)"
            + gpl + u"<br>" + source + u"</p><p>" + trademark + u"</p><p>" + dan + u"</p></div>";

    QString translators;
    const QString transRow = uR"(<tr><td>%1</td><td width="2em">&nbsp;</td><td>%2 <a href="mailto:%3">%4</a></td></tr>)"_qs;
    const auto translations = Config::inst()->translations();
    for (const Config::Translation &trans : translations) {
        if ((trans.language != u"en") && !trans.author.isEmpty()) {
            QString langname = trans.localName + u" (" + trans.name + u")";
            translators = translators + transRow.arg(langname, trans.author, trans.authorEmail, trans.authorEmail);
        }
    }
    translators = u"<b>" + tr("Translators") + uR"(</b><div style="margin-left: 10px">)"
            + uR"(<p><table border="0">)" + translators + uR"(</p></table>)" + u"</div>";

    return QVariantMap {
        { u"header"_qs, header },
        { u"license"_qs, license },
        { u"translators"_qs, translators },
    };
}


void Application::setupSentry()
{
#if defined(SENTRY_ENABLED)
    auto *sentry = sentry_options_new();
#  if defined(SENTRY_DEBUG)
    sentry_options_set_debug(sentry, 1);
    sentry_options_set_logger(sentry, [](sentry_level_t level, const char *message, va_list args, void *) {
        QDebug dbg(static_cast<QString *>(nullptr));
        switch (level) {
        default:
        case SENTRY_LEVEL_DEBUG:   dbg = QMessageLogger().debug(LogSentry); break;
        case SENTRY_LEVEL_INFO:    dbg = QMessageLogger().info(LogSentry); break;
        case SENTRY_LEVEL_WARNING: dbg = QMessageLogger().warning(LogSentry); break;
        case SENTRY_LEVEL_ERROR:
        case SENTRY_LEVEL_FATAL:   dbg = QMessageLogger().critical(LogSentry); break;
        }
        dbg.noquote() << QString::vasprintf(QByteArray(message).replace("%S", "%ls").constData(), args);
    }, nullptr);
#  endif
    sentry_options_set_dsn(sentry, "https://335761d80c3042548349ce5e25e12a06@o553736.ingest.sentry.io/5681421");
    sentry_options_set_release(sentry, "brickstore@" BRICKSTORE_BUILD_NUMBER);
    sentry_options_set_require_user_consent(sentry, 1);

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/.sentry";
    QString crashHandler = QCoreApplication::applicationDirPath() + u"/crashpad_handler";

#  if defined(Q_OS_WINDOWS)
    crashHandler.append(u".exe"_qs);
    sentry_options_set_handler_pathw(sentry, reinterpret_cast<const wchar_t *>(crashHandler.utf16()));
    sentry_options_set_database_pathw(sentry, reinterpret_cast<const wchar_t *>(dbPath.utf16()));
#  else
    sentry_options_set_handler_path(sentry, crashHandler.toLocal8Bit().constData());
    sentry_options_set_database_path(sentry, dbPath.toLocal8Bit().constData());
#  endif
    if (sentry_init(sentry))
        qCWarning(LogSentry) << "Could not initialize sentry.io!";
    else
        qCInfo(LogSentry) << "Successfully initialized sentry.io";

    auto sysInfo = SystemInfo::inst()->asMap();
    for (auto it = sysInfo.cbegin(); it != sysInfo.cend(); ++it) {
        if (!it.key().startsWith(u"os.") || (it.key() == u"os.productname"))
            sentry_set_tag(it.key().toUtf8().constData(), it.value().toString().toUtf8().constData());
    }
    sentry_set_tag("language", Config::inst()->language().toUtf8().constData());
#endif
}

void Application::shutdownSentry()
{
#if defined(SENTRY_ENABLED)
    sentry_shutdown();
#endif
}

void Application::checkSentryConsent()
{
#if defined(SENTRY_ENABLED)
    auto check = []() {
        switch (Config::inst()->sentryConsent()) {
        case Config::SentryConsent::Unknown:
            sentry_user_consent_reset();
            break;
        case Config::SentryConsent::Given:
            sentry_user_consent_give();
            break;
        case Config::SentryConsent::Revoked:
            sentry_user_consent_revoke();
            break;
        }
    };
    connect(Config::inst(), &Config::sentryConsentChanged, check);
    check();
#endif
}

void Application::addSentryBreadcrumb(QtMsgType msgType, const QMessageLogContext &msgCtx, const QString &msg)
{
#if defined(SENTRY_ENABLED)
    sentry_value_t crumb = sentry_value_new_breadcrumb("default", msg.toUtf8());
    if (msgCtx.category)
        sentry_value_set_by_key(crumb, "category", sentry_value_new_string(msgCtx.category));
    msgType = std::clamp(msgType, QtDebugMsg, QtInfoMsg);
    static const char *msgTypeLevels[5] = { "debug", "warning", "error", "fatal", "info" };
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string(msgTypeLevels[msgType]));
    const auto now = QDateTime::currentSecsSinceEpoch();
    sentry_value_set_by_key(crumb, "timestamp", sentry_value_new_int32(int32_t(now)));
    sentry_add_breadcrumb(crumb);
#else
    Q_UNUSED(msgType)
    Q_UNUSED(msgCtx)
    Q_UNUSED(msg)
#endif
}

void Application::setupLogging()
{
    qSetMessagePattern(u"%{if-category}%{category}: %{endif}%{message}%{if-warning} (at %{file}, %{line})%{endif}"_qs);

    m_defaultLoggingFilter = QLoggingCategory::installFilter([](QLoggingCategory *lc) {
        if (qstrcmp(lc->categoryName(), "qt.qml.typeregistration") == 0) {
            // suppress the "value types have to be lower case" typeregistration warnings
            lc->setEnabled(QtWarningMsg, false);
        } else if (qstrcmp(lc->categoryName(), "qt.gui.imageio") == 0) {
            // suppress the "incorrect color profile" messages from libpng
            lc->setEnabled(QtInfoMsg, false);
        } else if (s_inst && s_inst->m_defaultLoggingFilter) {
            s_inst->m_defaultLoggingFilter(lc);
        }
    });

    auto messageHandler = [](QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
    {
        if (s_inst && s_inst->m_defaultMessageHandler)
            (*s_inst->m_defaultMessageHandler)(type, ctx, msg);

        addSentryBreadcrumb(type, ctx, msg);

        if (!s_inst)
            return;

        try {
            // we may not be in the main thread here, but even if we are, we could be recursing

            QMutexLocker locker(&s_inst->m_loggingMutex);
            s_inst->m_loggingMessages.emplace_back(type, QString::fromLatin1(ctx.category),
                                                   QString::fromLatin1(ctx.file), ctx.line, msg);
            locker.unlock();

            if (s_inst->m_uiMessageHandler) {
                // can't start a timer from another thread
                QMetaObject::invokeMethod(s_inst, []() {
                    if (!s_inst->m_loggingTimer.isActive())
                        s_inst->m_loggingTimer.start();
                }, Qt::QueuedConnection);
            }
        } catch (const std::bad_alloc &) {
            // swallow bad-allocs and hope for sentry to log something useful
        }
    };

    s_inst->m_loggingTimer.setInterval(100);
    s_inst->m_loggingTimer.setSingleShot(true);

    connect(&s_inst->m_loggingTimer, &QTimer::timeout, s_inst, []() {
        if (!s_inst->m_uiMessageHandler)
            return;

        QMutexLocker locker(&s_inst->m_loggingMutex);
        if (s_inst->m_loggingMessages.isEmpty())
            return;
        auto messages = s_inst->m_loggingMessages.mid(0, 100); // don't overload the UI
        s_inst->m_loggingMessages.remove(0, messages.size());
        bool restartTimer = !s_inst->m_loggingMessages.isEmpty();
        locker.unlock();

        for (const auto &lm : messages)
            s_inst->m_uiMessageHandler(lm);

        if (restartTimer)
            s_inst->m_loggingTimer.start();

    });

    m_defaultMessageHandler = qInstallMessageHandler(messageHandler);
}

void Application::setUILoggingHandler(UIMessageHandler callback)
{
    m_uiMessageHandler = callback;
}

void Application::setIconTheme(Theme theme)
{
    QPixmapCache::clear();
    QIcon::setThemeName(theme == DarkTheme ? u"brickstore-breeze-dark"_qs : u"brickstore-breeze"_qs);

#if defined(BS_MOBILE)
    // we need to delay this, because we are called during the construction of the QML root item
    QMetaObject::invokeMethod(this, [this]() {
        auto roots = m_engine->rootObjects();
        if (!roots.isEmpty()) {
            QObject *root = roots.constFirst();

            // force all icons to update by re-setting the name
            const auto icons = root->findChildren<QQuickIconImage *>();
            for (const auto &icon : icons) {
                QString name = icon->name();
                icon->setName(u"foo"_qs);
                icon->setName(name);
            }
        }
    }, Qt::QueuedConnection);
#endif
}


bool Application::initBrickLink()
{
    BrickLink::Core *bl = BrickLink::create(Config::inst()->cacheDir(), databaseUrl(),
                                            SystemInfo::inst()->physicalMemory());

    BrickLink::core()->setUpdateIntervals(Config::inst()->updateIntervals());
    connect(Config::inst(), &Config::updateIntervalsChanged,
            BrickLink::core(), &BrickLink::Core::setUpdateIntervals);

    QString lastRetrieverId = Config::inst()->value(u"BrickLink/VAT/LastRetrieverId"_qs).toString();
    QString retrieverId = BrickLink::core()->priceGuideCache()->retrieverId();
    int vatType = Config::inst()->value(u"BrickLink/VAT/"_qs + retrieverId,
                                        int(BrickLink::VatType::Excluded)).toInt();
    BrickLink::core()->priceGuideCache()->setCurrentVatType(static_cast<BrickLink::VatType>(vatType));

    connect(BrickLink::core()->priceGuideCache(), &BrickLink::PriceGuideCache::currentVatTypeChanged,
            this, [](BrickLink::VatType vatType) {
        QString retrieverId = BrickLink::core()->priceGuideCache()->retrieverId();
        Config::inst()->setValue(u"BrickLink/VAT/"_qs + retrieverId, int(vatType));
    });

    if (retrieverId != lastRetrieverId) {
        Config::inst()->setValue(u"BrickLink/VAT/LastRetrieverId"_qs, retrieverId);

        m_startupMessages << tr("The price-guide download mechanism changed. Please make sure "
                                "your VAT setup is still correct on the BrickLink page in the "
                                "Settings dialog.");
    }

    BrickLink::core()->setCredentials({ Config::inst()->brickLinkUsername(),
                                        Config::inst()->brickLinkPassword() });
    connect(Config::inst(), &Config::brickLinkCredentialsChanged,
            this, []() {
        BrickLink::core()->setCredentials({ Config::inst()->brickLinkUsername(),
                                            Config::inst()->brickLinkPassword() });
    });

    connect(BrickLink::core()->carts(), &BrickLink::Carts::fetchLotsFinished,
            this, [](BrickLink::Cart *cart, bool success, const QString &message) {
        Q_ASSERT(cart);

        if (!success) {
            UIHelpers::warning(message);
        } else {
            DocumentIO::importBrickLinkCart(cart);

            if (!message.isEmpty())
                UIHelpers::information(message);
        }
    });
    connect(BrickLink::core()->wantedLists(), &BrickLink::WantedLists::fetchLotsFinished,
            this, [](BrickLink::WantedList *wanted, bool success, const QString &message) {
        Q_ASSERT(wanted);

        if (!success) {
            UIHelpers::warning(message);
        } else {
            DocumentIO::importBrickLinkWantedList(wanted);

            if (!message.isEmpty())
                UIHelpers::information(message);
        }
    });

    return bl;
}

void Application::setupQml()
{
    // add all relevant QML modules here
    extern void qml_register_types_LDraw(); qml_register_types_LDraw();
    extern void qml_register_types_BrickLink(); qml_register_types_BrickLink();
    extern void qml_register_types_BrickStore(); qml_register_types_BrickStore();

    m_engine = new QQmlApplicationEngine(this);
    redirectQmlEngineWarnings(LogQml());

    connect(BrickLink::core()->database(), &BrickLink::Database::databaseAboutToBeReset,
            m_engine, &QQmlEngine::collectGarbage);

    connect(this, &Application::languageChanged,
            m_engine, &QQmlEngine::retranslate);

    m_engine->setBaseUrl(QUrl(u"qrc:/"_qs));
    m_engine->addImportPath(u"qrc:/"_qs);

    // we need to call this early for the QmlDocumentList to be in sync
    QmlBrickStore::create(m_engine, m_engine);
}

void Application::redirectQmlEngineWarnings(const QLoggingCategory &cat)
{
    m_engine->setOutputWarningsToStandardError(false);

    QObject::connect(m_engine, &QQmlEngine::warnings,
                     qApp, [&cat](const QList<QQmlError> &list) {
        for (auto &err : list) {
            if (!cat.isEnabled(err.messageType()))
                continue;

            QByteArray func;
            if (err.object())
                func = err.object()->objectName().toLocal8Bit();
            QByteArray file;
            if (err.url().scheme() == u"file"_qs)
                file = err.url().toLocalFile().toLocal8Bit();
            else
                file = err.url().toDisplayString().toLocal8Bit();

            qt_message_output(err.messageType(),
                              { file.constData(), err.line(), func, cat.categoryName() },
                              err.description());
        }
    });
}

#include "moc_application.cpp"
