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
#include <QTimer>
#include <QEvent>
#include <QDir>
#include <QLocale>
#include <QTranslator>
#include <QSysInfo>
#include <QFileOpenEvent>
#include <QDesktopServices>
#include <QLocalSocket>
#include <QLocalServer>
#include <QNetworkProxyFactory>
#include <QPlainTextEdit>
#include <QStringBuilder>
#include <QLoggingCategory>
#include <QCommandLineParser>
#include <QToolTip>
#include <QToolBar>
#include <QToolButton>
#include <QStyleFactory>
#include <QMessageBox>
#include <QPixmapCache>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#  ifdef MessageBox
#    undef MessageBox
#  endif
#  include <wininet.h>
#elif defined(Q_OS_MACOS)
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <SystemConfiguration/SCNetworkReachability.h>
#  include <QVersionNumber>
#  include <QProxyStyle>
#  include <QtGui/private/qguiapplication_p.h>
#  include <QtGui/private/qshortcutmap_p.h>
#endif

#include "progressdialog.h"
#include "config.h"
#include "bricklink.h"
#include "ldraw.h"
#include "messagebox.h"
#include "framework.h"
#include "transfer.h"
#include "currency.h"
#include "scriptmanager.h"
#include "version.h"
#include "application.h"
#include "stopwatch.h"
#include "utility.h"
#include "smartvalidator.h"
#include "systeminfo.h"

#if defined(SENTRY_ENABLED)
#  include "sentry.h"
#  include "version.h"
Q_LOGGING_CATEGORY(LogSentry, "sentry")
#endif

using namespace std::chrono_literals;

#if defined(Q_OS_MACOS)
class MacUnderlineStyle : public QProxyStyle
{
public:
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;
    void drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &pal, bool enabled,
                      const QString &text, QPalette::ColorRole textRole = QPalette::NoRole) const override;
};

int MacUnderlineStyle::styleHint(QStyle::StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == SH_UnderlineShortcut)
        return true;
    else if (hint == SH_ItemView_MovementWithoutUpdatingSelection)
        return true;
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void MacUnderlineStyle::drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &pal, bool enabled, const QString &text, QPalette::ColorRole textRole) const
{
    return QCommonStyle::drawItemText(painter, rect, flags, pal, enabled, text, textRole);
}

#endif

Application *Application::s_inst = nullptr;

Application::Application(int &_argc, char **_argv)
    : QObject()
{
    s_inst = this;

    QCoreApplication::setApplicationName(QLatin1String(BRICKSTORE_NAME));
    QCoreApplication::setApplicationVersion(QLatin1String(BRICKSTORE_VERSION));
    QGuiApplication::setApplicationDisplayName(QCoreApplication::applicationName());

    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#  if defined(Q_OS_WINDOWS)
    // the Vista style scales very badly when scaled to non-integer factors
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);
#  else
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#  endif
#endif

#if defined(Q_OS_LINUX)
    const QByteArray xdgDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    for (const auto *gtkBased : { "GNOME", "MATE", "Cinnamon" }) {
        if (xdgDesktop.contains(gtkBased))
            qputenv("QT_QPA_PLATFORMTHEME", "gtk2");
    }
#endif

    (void) new QApplication(_argc, _argv);

    QCommandLineParser clp;
    clp.addHelpOption();
    clp.addVersionOption();
    clp.addOption({ "load-translation"_l1, "Load the specified translation (testing only)."_l1, "qm-file"_l1 });
    clp.addPositionalArgument("files"_l1, "The BSX documents to open, optionally."_l1, "[files...]"_l1);
    clp.process(QCoreApplication::arguments());

    setupLogging();
    setupSentry();
    QCoreApplication::instance()->installEventFilter(this);

#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_MACOS)
    QPixmap pix(":/images/brickstore_icon.png"_l1);
    if (!pix.isNull())
        QGuiApplication::setWindowIcon(pix);
#endif

#if defined(Q_OS_MACOS)
    //QGuiApplication::setAttribute(Qt::AA_DontShowIconsInMenus); // mac style guide

    // macOS style guide doesn't want shortcut keys in dialogs (Alt + underlined character)
    QApplication::setStyle(new MacUnderlineStyle());
    void qt_set_sequence_auto_mnemonic(bool on);
    qt_set_sequence_auto_mnemonic(true);

#  if QT_VERSION <= QT_VERSION_CHECK(5, 15, 2)
    // the new default font San Francisco has rendering problems: QTBUG-88495
    if (QVersionNumber::fromString(QSysInfo::productVersion()).majorVersion() >= 11) {
        QFont f = QApplication::font();
        f.setFamily("Helvetica Neue"_l1);
        QApplication::setFont(f);
    }
#  endif
#endif

    checkNetwork();
    auto *netcheck = new QTimer(this);
    connect(netcheck, &QTimer::timeout,
            this, &Application::checkNetwork);
    netcheck->start(5s);

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    //TODO5: find out why we are blacklisted ... for now, fake the UA
    Transfer::setDefaultUserAgent("Br1ckstore"_l1 % u'/' % QCoreApplication::applicationVersion()
                                  % u" (" + QSysInfo::prettyProductName() % u')');

    // initialize config & resource
    (void) Config::inst()->upgrade(BRICKSTORE_MAJOR, BRICKSTORE_MINOR, BRICKSTORE_PATCH);
    checkSentryConsent();
    setUiTheme();
    connect(Config::inst(), &Config::uiThemeChanged, this, &Application::setUiTheme);
    setIconTheme();
    (void) Currency::inst();
    (void) ScriptManager::inst();
    (void) SystemInfo::inst();

    m_default_fontsize = QGuiApplication::font().pointSizeF();
    QCoreApplication::instance()->setProperty("_bs_defaultFontSize", m_default_fontsize); // the settings dialog needs this

    auto setFontSizePercentLambda = [this](int p) {
        QFont f = QApplication::font();
        f.setPointSizeF(m_default_fontsize * qreal(qBound(50, p, 200)) / 100.);
        QApplication::setFont(f);
    };
    connect(Config::inst(), &Config::fontSizePercentChanged, this, setFontSizePercentLambda);
    int fsp = Config::inst()->fontSizePercent();
    if (fsp != 100)
        setFontSizePercentLambda(fsp);

    // check for an already running instance
    if (isClient()) {
        // we cannot call quit directly, since there is
        // no event loop to quit from...
        QMetaObject::invokeMethod(this, &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }

    if (!initBrickLink()) {
        // we cannot call quit directly, since there is no event loop to quit from...
        QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }
    if (LDraw::create(Config::inst()->ldrawDir(), nullptr)) {
        BrickLink::core()->setLDrawDataPath(LDraw::core()->dataPath());
    }

    updateTranslations(clp.value("load-translation"_l1));
    connect(Config::inst(), &Config::languageChanged,
            this, []() { Application::inst()->updateTranslations(); });

    m_files_to_open << clp.positionalArguments();

    MessageBox::setDefaultParent(FrameWork::inst());

    // tranform . or , into the local decimal separator in all QDoubleSpinBoxes and all
    // QLineEdits with a SmartDoubleValidator set
    DotCommaFilter::install();

    FrameWork::inst()->show();
#if defined(Q_OS_MACOS)
    FrameWork::inst()->raise();

#elif defined(Q_OS_WIN)
    RegisterApplicationRestart(nullptr, 0); // make us restart-able by installers

#endif

#if defined(SENTRY_ENABLED)
    if (Config::inst()->sentryConsent() == Config::SentryConsent::Unknown) {
        QString text = tr("Enable anonymous crash reporting?<br><br>Please consider enabling this feature when available.<br>If you have any doubts about what information is being submitted and how it is used, please <a href='https://github.com/rgriebl/brickstore/wiki/Crash-Reporting'>see here</a>.<br><br>Crash reporting can be enabled or disabled at any time in the Settings dialog.");

        switch (MessageBox::question(nullptr, { }, text)) {
        case QMessageBox::Yes:
            Config::inst()->setSentryConsent(Config::SentryConsent::Given);
            break;
        case QMessageBox::No:
            Config::inst()->setSentryConsent(Config::SentryConsent::Revoked);
            break;
        default:
            break;
        }
    }
#endif
}

Application::~Application()
{
    exitBrickLink();

    delete ScriptManager::inst();
    delete Currency::inst();
    delete Config::inst();

    s_inst = nullptr;
    delete qApp;

    shutdownSentry();
}

QPlainTextEdit *Application::logWidget() const
{
    return m_logWidget;
}

bool Application::shouldRestart() const
{
    return m_restart;
}

void Application::updateTranslations(const QString &translationOverride)
{
    QString language = Config::inst()->language();
    if (language.isEmpty())
        return;

    QString i18n = ":/i18n"_l1;

    static bool once = false; // always load english
    if (!once) {
        auto transQt = new QTranslator(this);
        if (transQt->load("qtbase_en"_l1, i18n))
            QCoreApplication::installTranslator(transQt);
        auto transBrickStore = new QTranslator(this);
        if (transBrickStore->load("brickstore_en"_l1, i18n))
            QCoreApplication::installTranslator(transBrickStore);
        once = true;
    }

    m_trans_qt.reset(new QTranslator);
    m_trans_brickstore.reset(new QTranslator);

    if (language != "en"_l1) {
        if (m_trans_qt->load("qtbase_"_l1 + language, i18n))
            QCoreApplication::installTranslator(m_trans_qt.get());
    }
    if ((language != "en"_l1) || (!translationOverride.isEmpty())) {
        QString translationFile = "brickstore_"_l1 % language;
        QString translationDir = i18n;

        QFileInfo qm(translationOverride);
        if (qm.isReadable()) {
            translationFile = qm.fileName();
            translationDir = qm.absolutePath();
        }

        if (m_trans_brickstore->load(translationFile, translationDir))
            QCoreApplication::installTranslator(m_trans_brickstore.get());
    }
}

QString Application::applicationUrl() const
{
    return QLatin1String(BRICKSTORE_URL);
}

QString Application::gitHubUrl() const
{
    return QLatin1String(BRICKSTORE_GITHUB_URL);
}

void Application::enableEmitOpenDocument(bool b)
{
    if (b != m_enable_emit) {
        m_enable_emit = b;

        if (b && !m_files_to_open.isEmpty())
            QTimer::singleShot(0, this, SLOT(doEmitOpenDocument()));
    }
}

void Application::doEmitOpenDocument()
{
    while (m_enable_emit && !m_files_to_open.isEmpty())
        emit openDocument(m_files_to_open.takeFirst());
}

bool Application::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::ChildPolished) {
        if (auto *tb = qobject_cast<QToolButton *>(static_cast<QChildEvent *>(e)->child())) {
            if (!qobject_cast<QToolBar *>(o)) {
                QPointer<QToolButton> tbptr(tb);
                QMetaObject::invokeMethod(this, [tbptr]() {
                    if (tbptr && tbptr->autoRaise()) {
#if defined(Q_OS_MACOS)
                        // QToolButtons look really ugly on macOS, so we re-style them
                        static QStyle *fusion = QStyleFactory::create("fusion"_l1);
                        tbptr->setStyle(fusion);
#endif
                        if (qstrcmp(tbptr->style()->metaObject()->className(), "QFusionStyle") == 0) {
                            QPalette pal = tbptr->palette();
                            pal.setColor(QPalette::Button, Utility::premultiplyAlpha(
                                             qApp->palette("QAbstractItemView")
                                             .color(QPalette::Highlight)));
                            tbptr->setPalette(pal);
                        }
                    }
                });
            }
        }
    }
#if defined(Q_OS_MACOS)
    // the handling of emacs-style multi-key shortcuts is broken on macOS, because menu
    // shortcuts are handled directly in the QPA plugin, instad of going through the global
    // QShortcutMap. The workaround is override any shortcut while the map is in PartialMatch state.
    if (e->type() == QEvent::ShortcutOverride) {
        auto &scm = QGuiApplicationPrivate::instance()->shortcutMap;
        if (scm.state() == QKeySequence::PartialMatch) {
            e->accept();
            return true;
        }
    }
#endif

    if ((o != qApp) || !e)
        return false;

    switch (e->type()) {
    case QEvent::FileOpen:
        m_files_to_open.append(static_cast<QFileOpenEvent *>(e)->file());
        doEmitOpenDocument();
        return true;
    case QEvent::ApplicationPaletteChange:
        // we need to delay this: otherwise macOS crashes on theme changes
        QMetaObject::invokeMethod(this, [=]() {
            QPixmapCache::clear();
            setIconTheme();
        }, Qt::QueuedConnection);
        break;
    default:
        break;
    }
    return QObject::eventFilter(o, e);
}

bool Application::isClient(int timeout)
{
    enum { Undecided, Server, Client } state = Undecided;
    QString socketName = "BrickStore"_l1;
    QLocalServer *server = nullptr;

#if defined(Q_OS_WINDOWS)
    // QLocalServer::listen() would succeed for any number of callers
    HANDLE semaphore = ::CreateSemaphore(nullptr, 0, 1, L"Local\\BrickStore");
    state = (semaphore && (::GetLastError() == ERROR_ALREADY_EXISTS)) ? Client : Server;
#endif
    if (state != Client) {
        server = new QLocalServer(this);

        bool res = server->listen(socketName);
#if defined(Q_OS_UNIX)
        if (!res && server->serverError() == QAbstractSocket::AddressInUseError) {
            QFile::remove(QDir::cleanPath(QDir::tempPath()) + QLatin1Char('/') + socketName);
            res = server->listen(socketName);
        }
#endif
        if (res) {
            connect(server, &QLocalServer::newConnection,
                    this, &Application::clientMessage);
            state = Server;
        }
    }
    if (state != Server) {
        QLocalSocket client(this);

        for (int i = 0; i < 2; ++i) { // try twice
            client.connectToServer(socketName);
            if (client.waitForConnected(timeout / 2) || i)
                break;
            else
                QThread::msleep(static_cast<unsigned long>(timeout) / 4);
        }

        if (client.state() == QLocalSocket::ConnectedState) {
            QStringList files;
            foreach (const QString &arg, QCoreApplication::arguments().mid(1)) {
                QFileInfo fi(arg);
                if (fi.exists() && fi.isFile())
                    files << fi.absoluteFilePath();
            }
            QByteArray data;
            QDataStream ds(&data, QIODevice::WriteOnly);
            ds << qint32(0) << files;
            ds.device()->seek(0);
            ds << qint32(data.size() - int(sizeof(qint32)));

            bool res = (client.write(data) == data.size());
            res = res && client.waitForBytesWritten(timeout / 2);
            res = res && client.waitForReadyRead(timeout / 2);
            res = res && (client.read(1) == QByteArray(1, 'X'));

            if (res) {
                delete server;
                return true;
            }
        }
    }
    return false;
}

void Application::clientMessage()
{
    auto *server = qobject_cast<QLocalServer *>(sender());
    if (!server)
        return;
    QLocalSocket *client = server->nextPendingConnection();
    if (!client)
        return;

    QDataStream ds(client);
    QStringList files;
    bool header = true;
    int need = sizeof(qint32);
    while (need) {
        auto got = client->bytesAvailable();
        if (got < need) {
            client->waitForReadyRead();
        } else if (header) {
            need = 0;
            ds >> need;
            header = false;
        } else {
            ds >> files;
            need = 0;
        }
    }
    client->write("X", 1);

    m_files_to_open += files;
    doEmitOpenDocument();

    if (FrameWork::inst()) {
        FrameWork::inst()->setWindowState(FrameWork::inst()->windowState() & ~Qt::WindowMinimized);
        FrameWork::inst()->raise();
        FrameWork::inst()->activateWindow();
    }
}

void Application::setupSentry()
{
#if defined(SENTRY_ENABLED)
    auto *sentry = sentry_options_new();
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
    sentry_options_set_dsn(sentry, "https://335761d80c3042548349ce5e25e12a06@o553736.ingest.sentry.io/5681421");
    sentry_options_set_release(sentry, "brickstore@" BRICKSTORE_BUILD_NUMBER);
    sentry_options_set_require_user_consent(sentry, 1);

    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) % "/.sentry"_l1;
    QString crashHandler = QCoreApplication::applicationDirPath() % "/crashpad_handler"_l1;

#  if defined(Q_OS_WINDOWS)
    crashHandler.append(".exe"_l1);
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

    connect(SystemInfo::inst(), &SystemInfo::initialized, []() {
        auto sysInfo = SystemInfo::inst()->asMap();
        for (auto it = sysInfo.cbegin(); it != sysInfo.cend(); ++it) {
            if (!it.key().startsWith("os."_l1))
                sentry_set_tag(it.key().toUtf8().constData(), it.value().toString().toUtf8().constData());
        }
        sentry_set_tag("language", Config::inst()->language().toUtf8().constData());
    });
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
    msgType = qBound(QtDebugMsg, msgType, QtInfoMsg);
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
    m_logWidget = new QPlainTextEdit();
    m_logWidget->setObjectName("LogWidget"_l1);
    m_logWidget->setReadOnly(true);
    m_logWidget->setMaximumBlockCount(1000);
    m_logWidget->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_defaultMessageHandler = qInstallMessageHandler(&messageHandler);
}

void Application::messageHandler(QtMsgType msgType, const QMessageLogContext &msgCtx, const QString &msg)
{
    if (s_inst && s_inst->m_defaultMessageHandler)
        (*s_inst->m_defaultMessageHandler)(msgType, msgCtx, msg);

    addSentryBreadcrumb(msgType, msgCtx, msg);

    if (!s_inst || !s_inst->m_logWidget)
        return;

    static const char *msgTypeNames[] =   { "DBG ",   "WARN",   "CRIT",   "FATL",   "INFO" };
    static const char *msgTypeColor[] =   { "000000", "000000", "ff0000", "ffffff", "ffffff" };
    static const char *msgTypeBgColor[] = { "00ff00", "ffff00", "000000", "ff0000", "0000ff" };
    static const char *fileColor = "800080";
    static const char *lineColor = "ff00ff";
    static const char *categoryColor[] = { "e81717", "e8e817", "17e817", "17e8e8", "1717e8", "e817e8" };

    msgType = qBound(QtDebugMsg, msgType, QtInfoMsg);
    QString filename;
    if (msgCtx.file && msgCtx.file[0] && msgCtx.line > 1) {
        filename = QString::fromLocal8Bit(msgCtx.file);
        int pos = -1;
#if defined(Q_OS_WINDOWS)
        pos = filename.lastIndexOf('\\'_l1);
#endif
        if (pos < 0)
            pos = int(filename.lastIndexOf('/'_l1));
        filename = filename.mid(pos + 1);
    }

    QString str = "<pre>"_l1;
    const auto lines = msg.splitRef('\n'_l1);
    for (int i = 0; i < lines.count(); ++i) {
        str = str % R"(<span style="color:#)"_l1 % QLatin1String(msgTypeColor[msgType])
                % R"(;background-color:#)"_l1 % QLatin1String(msgTypeBgColor[msgType]) % R"(;">)"_l1
                % QLatin1String(msgTypeNames[msgType]) % R"(</span>)"_l1
                % R"(&nbsp;<span style="color:#)"_l1
                % QLatin1String(categoryColor[qHashBits(msgCtx.category, qstrlen(msgCtx.category), 1) % 6])
                % R"(;font-weight:bold;">)"_l1
                % QLatin1String(msgCtx.category) % R"(</span>)"_l1 % ":&nbsp;"_l1
                % lines.at(i).toString().toHtmlEscaped();
        if (i == (lines.count() - 1)) {
            if (!filename.isEmpty()) {
                str = str % R"( at <span style="color:#)"_l1 % QLatin1String(fileColor)
                        % R"(;font-weight:bold;">)"_l1 % filename
                        % R"(</span>, line <span style="color:#)"_l1 % QLatin1String(lineColor)
                        % R"(;font-weight:bold;">)"_l1 % QString::number(msgCtx.line) % R"(</span></pre>)"_l1;
            } else {
                str = str % "</pre>"_l1;
            }
        } else {
            str = str % "&#x21a9;<br>"_l1;
        }
    }
    QMetaObject::invokeMethod(s_inst->m_logWidget, "appendHtml", Qt::QueuedConnection,
                              Q_ARG(QString, str));
}

void Application::setIconTheme()
{
    static bool once = false;
    if (!once) {
        QIcon::setThemeSearchPaths({ ":/assets/icons"_l1 });
        once = true;
    }

    const auto pal = QGuiApplication::palette();
    auto winColor = pal.color(QPalette::Active, QPalette::Window);
    bool dark = ((winColor.lightnessF() * winColor.alphaF()) < 0.5);

    QIcon::setThemeName(dark ? "brickstore-breeze-dark"_l1 : "brickstore-breeze"_l1);
}

void Application::setUiTheme()
{
    static bool once = false;
    bool startup = !once;
    if (!once)
        once = true;
    auto theme = Config::inst()->uiTheme();

#if defined(Q_OS_MACOS)
    extern bool hasMacThemes();
    extern void setCurrentMacTheme(Config::UiTheme);

    // on macOS, we are using the native theme switch
    if (!hasMacThemes()) {
        if (!startup)
            QMessageBox::information(FrameWork::inst(), QCoreApplication::applicationName(),
                                     tr("Your macOS version is too old to support theme changes."));
    } else if (!startup || (theme != Config::UiTheme::SystemDefault)) {
        if (startup)
            setCurrentMacTheme(theme);
        else
            QTimer::singleShot(0, qApp, [=]() { setCurrentMacTheme(theme); });
    }
#else
    // on Windows and Linux, we have to instantiate a Fusion style with a matching palette for
    // the Light and Dark styles. Switching on the fly is hard because of possible QProxyStyle
    // instances, so we better restart the app.

    if (!startup) {
        QMessageBox mb(QMessageBox::Question, QCoreApplication::applicationName(),
                       tr("The theme change will take effect after a restart."),
                       QMessageBox::Yes | QMessageBox::No, FrameWork::inst());
        mb.setDefaultButton(QMessageBox::No);
        mb.setButtonText(QMessageBox::Yes, tr("Restart now"));
        mb.setButtonText(QMessageBox::No, tr("Later"));
        mb.setTextFormat(Qt::RichText);

        if (mb.exec() == QMessageBox::Yes) {
            m_restart = true;
            QCoreApplication::quit();
        }
        return;
    }

    if (theme != Config::UiTheme::SystemDefault) {
        QApplication::setStyle("fusion"_l1);
        auto palette = QApplication::style()->standardPalette();

        if (theme == Config::UiTheme::Dark) {
            // from https://github.com/Jorgen-VikingGod/Qt-Frameless-Window-DarkStyle
            palette.setColor(QPalette::Window, QColor(53, 53, 53));
            palette.setColor(QPalette::WindowText, Qt::white);
            palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
            palette.setColor(QPalette::Base, QColor(42, 42, 42));
            palette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
            palette.setColor(QPalette::Inactive, QPalette::ToolTipText, Qt::white);
            palette.setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor(53, 53, 53));
            palette.setColor(QPalette::Text, Qt::white);
            palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
            palette.setColor(QPalette::Dark, QColor(35, 35, 35));
            palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
            palette.setColor(QPalette::Button, QColor(53, 53, 53));
            palette.setColor(QPalette::ButtonText, Qt::white);
            palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
            palette.setColor(QPalette::BrightText, Qt::red);
            palette.setColor(QPalette::Link, QColor(42, 130, 218));
            palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
            palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
            palette.setColor(QPalette::HighlightedText, Qt::white);
            palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
        }

        QApplication::setPalette(palette);
        QToolTip::setPalette(palette);
    }
#endif
}

bool Application::initBrickLink()
{
    QString errstring;

    BrickLink::Core *bl = BrickLink::create(Config::inst()->brickLinkCacheDir(), &errstring);
    if (!bl) {
        MessageBox::critical(nullptr, { }, tr("Could not initialize the BrickLink kernel:<br /><br />%1").arg(errstring));
    } else {
        bl->setItemImageScaleFactor(Config::inst()->itemImageSizePercent() / 100.);
        connect(Config::inst(), &Config::itemImageSizePercentChanged,
                this, [](qreal p) { BrickLink::core()->setItemImageScaleFactor(p / 100.); });

        connect(Config::inst(), &Config::updateIntervalsChanged,
                BrickLink::core(), &BrickLink::Core::setUpdateIntervals);
        BrickLink::core()->setUpdateIntervals(Config::inst()->updateIntervals());
    }
    return bl;
}


void Application::exitBrickLink()
{
    delete BrickLink::core();
    delete LDraw::core();
}


#define CHECK_IP "178.63.92.134" // brickforge.de

void Application::checkNetwork()
{
    bool online = false;

#if defined(Q_OS_LINUX)
    int res = ::system("ip route get " CHECK_IP "/32 >/dev/null 2>/dev/null");

    //qWarning() << "Linux NET change: " << res << WIFEXITED(res) << WEXITSTATUS(res);
    if (!WIFEXITED(res) || (WEXITSTATUS(res) == 0 || WEXITSTATUS(res) == 127))
        online = true;

#elif defined(Q_OS_MACOS)
    static SCNetworkReachabilityRef target = nullptr;

    if (!target) {
        struct ::sockaddr_in sock;
        sock.sin_family = AF_INET;
        sock.sin_port = htons(80);
        sock.sin_addr.s_addr = inet_addr(CHECK_IP);

        target = SCNetworkReachabilityCreateWithAddress(nullptr, reinterpret_cast<sockaddr *>(&sock));
        // in theory we should CFRelease(target) when exitting
    }

    SCNetworkReachabilityFlags flags = 0;
    if (!target || !SCNetworkReachabilityGetFlags(target, &flags)
            || ((flags & (kSCNetworkReachabilityFlagsReachable | kSCNetworkReachabilityFlagsConnectionRequired))
                         == kSCNetworkReachabilityFlagsReachable)) {
        online = true;
    }

#elif defined(Q_OS_WINDOWS)
    // this function is buggy/unreliable
    //online = InternetCheckConnectionW(L"http://" TEXT(CHECK_IP), 0, 0);
    DWORD flags;
    online = InternetGetConnectedStateEx(&flags, nullptr, 0, 0);
    //qWarning() << "Win NET change: " << online;

#else
    online = true;
#endif

    if (online != m_online) {
        m_online = online;
        emit onlineStateChanged(m_online);
    }
}

bool Application::isOnline() const
{
    return m_online;
}
