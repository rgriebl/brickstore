/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
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
#include <QLibraryInfo>
#include <QSysInfo>
#include <QFileOpenEvent>
#include <QProcess>
#include <QDesktopServices>
#include <QLocalSocket>
#include <QLocalServer>
#include <QNetworkProxyFactory>

#if defined(Q_OS_WINDOWS)
#  include <windows.h>
#  ifdef MessageBox
#    undef MessageBox
#  endif
#  include <wininet.h>
#elif defined(Q_OS_MACOS)
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <SystemConfiguration/SCNetwork.h>
#elif defined(Q_OS_UNIX)
#  include <sys/utsname.h>
#endif

#include "informationdialog.h"
#include "progressdialog.h"
#include "checkforupdates.h"
#include "config.h"
#include "rebuilddatabase.h"
#include "bricklink.h"
#include "ldraw.h"
#include "messagebox.h"
#include "framework.h"
#include "transfer.h"
#include "report.h"
#include "currency.h"

#include "utility.h"
#include "version.h"
#include "application.h"

#define XSTR(a) #a
#define STR(a) XSTR(a)

enum {
#if defined(QT_NO_DEBUG)
    isDeveloperBuild = 0,
#else
    isDeveloperBuild = 1,
#endif
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    isUnix = 1,
#else
    isUnix = 0,
#endif
    is64Bit = (sizeof(void *) / 8)
};

Application *Application::s_inst = 0;

Application::Application(bool rebuild_db_only, bool skip_download, int &_argc, char **_argv)
    : QApplication(_argc, _argv, !rebuild_db_only)
{
    s_inst = this;

    m_default_fontsize = font().pointSizeF();
    setProperty("_bs_defaultFontSize", m_default_fontsize); // the settings dialog needs this

    auto setFontSizePercentLambda = [this](int p) {
        QFont f = font();
        f.setPointSizeF(m_default_fontsize * qreal(qBound(50, p, 200)) / 100.);
        setFont(f);
    };
    connect(Config::inst(), &Config::fontSizePercentChanged, this, setFontSizePercentLambda);
    int fsp = Config::inst()->fontSizePercent();
    if (fsp != 100)
        setFontSizePercentLambda(fsp);

    m_enable_emit = false;
    m_has_alpha = rebuild_db_only ? false : (QPixmap::defaultDepth() >= 15);
    m_trans_qt = 0;
    m_trans_brickstore = 0;

    m_online = true;
    checkNetwork();

    QTimer *netcheck = new QTimer(this);
    connect(netcheck, &QTimer::timeout,
            this, &Application::checkNetwork);
    netcheck->start(5000);

    // check for an already running instance
    if (!rebuild_db_only) {
        if (isClient()) {
            // we cannot call quit directly, since there is
            // no event loop to quit from...
            QMetaObject::invokeMethod(this, &Application::quit, Qt::QueuedConnection);
            return;
        }
    }

    setApplicationName(QLatin1String(BRICKSTORE_NAME));
    setApplicationVersion(QLatin1String(BRICKSTORE_VERSION));

    QNetworkProxyFactory::setUseSystemConfiguration(true);

//    Transfer::setDefaultUserAgent(applicationName() + QLatin1Char('/') + applicationVersion() +
//                                  QLatin1String(" (") + systemName() + QLatin1Char(' ') + systemVersion() +
//                                  QLatin1String("; http://") + applicationUrl() + QLatin1Char(')'));

    //TODO5: find out why we are blacklisted ... for now, fake the UA
    Transfer::setDefaultUserAgent("Br1ckstore" + QLatin1Char('/') + applicationVersion() +
                                  QLatin1String(" (") + QSysInfo::prettyProductName() +
                                  QLatin1Char(')'));


    // initialize config & resource
    (void) Config::inst()->upgrade(BRICKSTORE_MAJOR, BRICKSTORE_MINOR, BRICKSTORE_PATCH);
    (void) Currency::inst();
    (void) ReportManager::inst();

    if (!initBrickLink()) {
        // we cannot call quit directly, since there is
        // no event loop to quit from...
        QTimer::singleShot(0, this, SLOT(quit()));
        return;

    } else if (rebuild_db_only) {
        QMetaObject::invokeMethod(this, [skip_download]() {
            RebuildDatabase rdb(skip_download);
            exit(rdb.exec());
        }, Qt::QueuedConnection);
    }
    else {
#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_MACOS)
        QPixmap pix(":/images/brickstore_icon.png");
        if (!pix.isNull())
            setWindowIcon(pix);
#endif
#if defined(Q_OS_MACOS)
        setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
        updateTranslations();
        connect(Config::inst(), &Config::languageChanged,
                this, &Application::updateTranslations);

        MessageBox::setDefaultTitle(applicationName());

        m_files_to_open << arguments().mid(1);

        FrameWork::inst()->show();
#if defined(Q_OS_MACOS)
        FrameWork::inst()->raise();
#endif
    }
}

Application::~Application()
{
    exitBrickLink();

    delete ReportManager::inst();
    delete Currency::inst();
    delete Config::inst();
}

bool Application::pixmapAlphaSupported() const
{
    return m_has_alpha;
}

QStringList Application::externalResourceSearchPath(const QString &subdir) const
{
    static QStringList baseSearchPath;

    if (baseSearchPath.isEmpty()) {
        QString appdir = applicationDirPath();
#if defined(Q_OS_WINDOWS)
        baseSearchPath << appdir;

        if (isDeveloperBuild)
            baseSearchPath << appdir + QLatin1String("/..");
#elif defined(Q_OS_MACOS)
        baseSearchPath << appdir + QLatin1String("/../Resources");
#elif defined(Q_OS_UNIX)
        baseSearchPath << QLatin1String(STR(INSTALL_PREFIX) "/share/brickstore2");

        if (isDeveloperBuild)
            baseSearchPath << appdir;
#endif
    }
    if (subdir.isEmpty()) {
        return baseSearchPath;
    } else {
        QStringList searchPath;
        foreach (const QString &bsp, baseSearchPath)
            searchPath << bsp + QDir::separator() + subdir;
        return searchPath;
    }
}

void Application::updateTranslations()
{
    QString locale = Config::inst()->language();
    if (locale.isEmpty())
        locale = QLocale::system().name();
    QLocale::setDefault(QLocale(locale));

    if (m_trans_qt)
        removeTranslator(m_trans_qt);
    if (m_trans_brickstore)
        removeTranslator(m_trans_brickstore);

    m_trans_qt = new QTranslator(this);
    m_trans_brickstore = new QTranslator(this);

    QStringList spath = externalResourceSearchPath("/translations");
    if (qSharedBuild() && (isDeveloperBuild | isUnix))
        spath << QLibraryInfo::location(QLibraryInfo::TranslationsPath);

    bool qtLoaded = false, bsLoaded = false;

    foreach (const QString &sp, spath) {
        qtLoaded |= m_trans_qt->load(QLatin1String("qt_") + locale, sp);
        bsLoaded |= m_trans_brickstore->load(QLatin1String("brickstore_") + locale, sp);
    }
    if (qtLoaded)
        installTranslator(m_trans_qt);
    if (bsLoaded)
        installTranslator(m_trans_brickstore);
}

QString Application::applicationUrl() const
{
    return QLatin1String(BRICKSTORE_URL);
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
    while (m_enable_emit && !m_files_to_open.isEmpty()) {
        QString file = m_files_to_open.front();
        m_files_to_open.pop_front();

        emit openDocument(file);
    }
}

bool Application::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::FileOpen:
        m_files_to_open.append(static_cast<QFileOpenEvent *>(e)->file());
        doEmitOpenDocument();
        return true;
    default:
        return QApplication::event(e);
    }
}

bool Application::isClient(int timeout)
{
    enum { Undecided, Server, Client } state = Undecided;
    QString socketName = QLatin1String("BrickStore");
    QLocalServer *server = 0;

#if defined(Q_OS_WINDOWS)
    // QLocalServer::listen() would succeed for any number of callers
    HANDLE semaphore = ::CreateSemaphore(0, 0, 1, L"Local\\BrickStore");
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
            if (client.waitForConnected(timeout / 2) || i) {
                break;
            } else {
#if defined(Q_OS_WINDOWS)
                Sleep(timeout / 4);
#else
                struct timespec ts = { (timeout / 4) / 1000, ((timeout / 4) % 1000) * 1000 * 1000 };
                ::nanosleep(&ts, 0);
#endif
            }
        }

        if (client.state() == QLocalSocket::ConnectedState) {
            QStringList files;
            Q_FOREACH(const QString &arg, arguments()) {
                QFileInfo fi(arg);
                if (fi.exists() && fi.isFile())
                    files << fi.absoluteFilePath();
            }
            QByteArray data;
            QDataStream ds(&data, QIODevice::WriteOnly);
            ds << qint32(0) << files;
            ds.device()->seek(0);
            ds << qint32(data.size() - sizeof(qint32));

            bool res = (client.write(data) == data.size());
            res &= client.waitForBytesWritten(timeout / 2);
            res &= client.waitForReadyRead(timeout / 2);
            res &= (client.read(1) == QByteArray(1, 'X'));

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
    QLocalServer *server = qobject_cast<QLocalServer *>(sender());
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
        int got = client->bytesAvailable();
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

bool Application::initBrickLink()
{
    QString errstring;
    QString defdatadir = QDir::homePath();

#if defined(Q_OS_WINDOWS)
    defdatadir += QLatin1String("/brickstore-cache/");
#else
    defdatadir += QLatin1String("/.brickstore-cache/");
#endif

    BrickLink::Core *bl = BrickLink::create(Config::inst()->dataDir(), &errstring);

    if (!bl)
        QMessageBox::critical(0, applicationName(), tr("Could not initialize the BrickLink kernel:<br /><br />%1").arg(errstring), QMessageBox::Ok);

    bl->setTransfer(new Transfer);

    /*LDraw::Core *ld =*/ LDraw::create(QString(), &errstring);

//    if (!ld)
//        MessageBox::critical(0, tr("Could not initialize the LDraw kernel:<br /><br />%1").arg(errstring));

    return (bl != 0); // && (ld != 0);
}


void Application::exitBrickLink()
{
    delete BrickLink::core();
    delete LDraw::core();
}


void Application::about()
{
    QString layout = QLatin1String(
        "<center>"
        "<table border=\"0\"><tr>"
        "<td valign=\"middle\" align=\"center\" width=\"168\">"
        "<img src=\":/images/brickstore_icon.png\" width=\"128\" style=\"margin: 20\"/></td>"
        "<td align=\"left\">"
        "<strong style=\"font-size: x-large\">%1</strong><br>"
        "<strong style=\"font-size: large\">%3</strong><br>"
        "<span style=\"font-size: large\">%2</strong><br>"
        "<br>%4</td>"
        "</tr></table>"
        "</center><center>"
        "<br><big>%5</big>"
        "</center>%6<p>%7</p>");


    QString page1_link = QLatin1String("<strong>%1</strong> | <a href=\"system\">%2</a>");
    page1_link = page1_link.arg(tr("Legal Info"), tr("System Info"));
    QString page2_link = QLatin1String("<a href=\"index\">%1</a> | <strong>%2</strong>");
    page2_link = page2_link.arg(tr("Legal Info"), tr("System Info"));

    QString copyright = tr("Copyright &copy; %1").arg(BRICKSTORE_COPYRIGHT);
    QString version   = tr("Version %1").arg(BRICKSTORE_VERSION);
    QString support   = tr("Visit %1, or send an email to %2").arg("<a href=\"http://" BRICKSTORE_URL "\">" BRICKSTORE_URL "</a>",
                                                                   "<a href=\"mailto:" BRICKSTORE_MAIL "\">" BRICKSTORE_MAIL "</a>");

    QString qt = QLibraryInfo::version().toString();
    if (QLibraryInfo::isDebugBuild())
        qt += QLatin1String(" (") + tr("debug build") + QLatin1String(")");

    QString translators = QLatin1String("<b>") + tr("Translators") + QLatin1String("</b><table border=\"0\">");

    QString translators_html = QLatin1String("<tr><td>%1</td><td width=\"2em\"></td><td>%2 &lt;<a href=\"mailto:%3\">%4</a>&gt;</td></tr>");
    foreach (const Config::Translation &trans, Config::inst()->translations()) {
        if (trans.language != QLatin1String("en")) {
            QString langname = trans.languageName.value(QLocale().name().left(2), trans.languageName[QLatin1String("en")]);
            translators += translators_html.arg(langname, trans.author, trans.authorEMail, trans.authorEMail);
        }
    }

    translators += QLatin1String("</table>");

    QString legal = tr(
        "<p>"
        "This program is free software; it may be distributed and/or modified "
        "under the terms of the GNU General Public License version 2 as published "
        "by the Free Software Foundation and appearing in the file LICENSE.GPL "
        "included in this software package."
        "<br>"
        "This program is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE "
        "WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE."
        "<br>"
        "See <a href=\"http://fsf.org/licensing/licenses/gpl.html\">www.fsf.org/licensing/licenses/gpl.html</a> for GPL licensing information."
        "</p><p>"
        "All data from <a href=\"https://www.bricklink.com\">www.bricklink.com</a> is owned by BrickLink<sup>TM</sup>, "
        "which is a trademark of Dan Jezek."
        "</p><p>"
        "LEGO<sup>&reg;</sup> is a trademark of the LEGO group of companies, "
        "which does not sponsor, authorize or endorse this software."
        "</p><p>"
        "All other trademarks recognised."
        "</p>");

    QString technical = QLatin1String(
        //"<p>"
        "<table>"
        "<th colspan=\"2\" align=\"left\">Build Info</th>"
        "<tr><td>User     </td><td>%1</td></tr>"
        "<tr><td>Host     </td><td>%2</td></tr>"
        "<tr><td>Date     </td><td>%3</td></tr>"
        "<tr><td>Architecture  </td><td>%4</td></tr>"
        "<tr><td>Compiler </td><td>%5</td></tr>"
        "</table><br>"
        "<table>"
        "<th colspan=\"2\" align=\"left\">Runtime Info</th>"
        "<tr><td>OS     </td><td>%6</td></tr>"
        "<tr><td>Architecture  </td><td>%7</td></tr>"
        "<tr><td>Memory </td><td>%L8 MB</td></tr>"
        "<tr><td>Qt     </td><td>%9</td></tr>"
        "</table>"
        /*"</p>"*/);

    technical = technical.arg(STR(__USER__), STR(__HOST__), __DATE__ " " __TIME__)
            .arg(QSysInfo::buildCpuArchitecture()).arg(
#if defined(_MSC_VER)
                "Microsoft Visual-C++ "
#  if _MSC_VER >= 1920
                "2019 (16." _BS_STR(_MSC_VER) ")"
#  elif _MSC_VER >= 1910
                "2017 (15." _BS_STR(_MSC_VER) ")"
#  elif _MSC_VER >= 1900
                "2015 (14.0)"
#  else
                "???"
#  endif
#elif defined(__GNUC__)
                "GCC " __VERSION__
#else
                "???"
#endif
                )
            .arg(QSysInfo::prettyProductName()).arg(QSysInfo::currentCpuArchitecture())
            .arg(Utility::physicalMemory()/(1024ULL*1024ULL)).arg(qt);

    QString page1 = layout.arg(applicationName(), copyright, version, support).arg(page1_link, legal, translators);
    QString page2 = layout.arg(applicationName(), copyright, version, support).arg(page2_link, technical, QString());

    QMap<QString, QString> pages;
    pages ["index"]  = page1;
    pages ["system"] = page2;

    InformationDialog d(applicationName(), pages, FrameWork::inst());
    d.exec();
}

void Application::checkForUpdates()
{
    Transfer trans;

    ProgressDialog d(&trans, FrameWork::inst());
    CheckForUpdates cfu(&d);
    d.exec();
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
    struct ::sockaddr_in sock;
    sock.sin_family = AF_INET;
    sock.sin_port = htons(80);
    sock.sin_addr.s_addr = inet_addr(CHECK_IP);
    SCNetworkConnectionFlags flags = 0;
    bool result = SCNetworkCheckReachabilityByAddress(reinterpret_cast<sockaddr *>(&sock), sizeof(sock), &flags);

    //qWarning() << "Mac NET change: " << result << flags;
    if (!result || (flags & (kSCNetworkFlagsReachable | kSCNetworkFlagsConnectionRequired)) == kSCNetworkFlagsReachable)
        online = true;

#elif defined(Q_OS_WINDOWS)
    // this function is buggy/unreliable
    //online = InternetCheckConnectionW(L"http://" TEXT(CHECK_IP), 0, 0);
    DWORD flags;
    online = InternetGetConnectedStateEx(&flags, 0, 0, 0);
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
