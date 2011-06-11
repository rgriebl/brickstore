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

#if defined(Q_OS_WIN)
#  include <windows.h>
#  ifdef MessageBox
#    undef MessageBox
#  endif
#  include <wininet.h>
#elif defined(Q_OS_MAC)
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

#include "filteredit.h"

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
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    isUnix = 1,
#else
    isUnix = 0,
#endif
    is64Bit = (sizeof(void *) / 8)
};

Application *Application::s_inst = 0;

Application::Application(bool rebuild_db_only, int _argc, char **_argv)
    : QApplication(_argc, _argv, !rebuild_db_only)
{
    s_inst = this;

    m_enable_emit = false;
    m_has_alpha = rebuild_db_only ? false : (QPixmap::defaultDepth() >= 15);
    m_trans_qt = 0;
    m_trans_brickstore = 0;

    m_online = true;
    checkNetwork();

    QTimer *netcheck = new QTimer(this);
    connect(netcheck, SIGNAL(timeout()), this, SLOT(checkNetwork()));
    netcheck->start(5000);

    // check for an already running instance
    if (!rebuild_db_only) {
        if (isClient()) {
            // we cannot call quit directly, since there is
            // no event loop to quit from...
            QTimer::singleShot(0, this, SLOT(quit()));
            return;
        }
    }

    setOrganizationName(QLatin1String("Softforge"));
    setOrganizationDomain(QLatin1String("softforge.de"));
    setApplicationName(QLatin1String(BRICKSTORE_NAME));
    setApplicationVersion(QLatin1String(BRICKSTORE_VERSION));

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    Transfer::setDefaultUserAgent(applicationName() + QLatin1Char('/') + applicationVersion() +
                                  QLatin1String(" (") + systemName() + QLatin1Char(' ') + systemVersion() +
                                  QLatin1String("; http://") + applicationUrl() + QLatin1Char(')'));

    // initialize config & resource
    (void) Config::inst()->upgrade(BRICKSTORE_MAJOR, BRICKSTORE_MINOR, BRICKSTORE_PATCH);
    (void) Currency::inst();
    (void) ReportManager::inst();

    if (!initBrickLink()) {
        // we cannot call quit directly, since there is
        // no event loop to quit from...
        QTimer::singleShot(0, this, SLOT(quit()));
        return;
    }
    else if (rebuild_db_only) {
        QTimer::singleShot(0, this, SLOT(rebuildDatabase()));
    }
    else {
#if defined(Q_WS_X11)
        QPixmap pix(":/images/icon.png");
        if (!pix.isNull())
            setWindowIcon(pix);
#endif
#if defined(Q_OS_WIN)
        int wv = QSysInfo::WindowsVersion;

        // don't use the native file dialogs on Windows < XP, since it
        // (a) may crash on some configurations (not yet checked with Qt4) and
        // (b) the Qt dialog is more powerful on these systems
        extern bool Q_GUI_EXPORT qt_use_native_dialogs;
        qt_use_native_dialogs = !((wv & QSysInfo::WV_DOS_based) || ((wv & QSysInfo::WV_NT_based) < QSysInfo::WV_XP));
#elif defined(Q_OS_MAC)
        setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
        updateTranslations();
        connect(Config::inst(), SIGNAL(languageChanged()), this, SLOT(updateTranslations()));

        MessageBox::setDefaultTitle(applicationName());

        for (int i = 1; i < argc(); i++)
            m_files_to_open << argv()[i];

        FrameWork::inst()->show();
#if defined(Q_OS_MAC)
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
#if defined(Q_OS_WIN)
        baseSearchPath << appdir;

        if (isDeveloperBuild)
            baseSearchPath << appdir + QLatin1String("/..");
#elif defined(Q_OS_MAC)
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
            searchPath << bsp + subdir;
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
    if (qSharedBuild() && (isDeveloperBuild || isUnix))
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

void Application::rebuildDatabase()
{
    RebuildDatabase rdb;
    exit(rdb.exec());
}


QString Application::applicationUrl() const
{
    return QLatin1String(BRICKSTORE_URL);
}

QString Application::systemName() const
{
    QString sys_name = tr("(unknown)");

#if defined(Q_OS_MAC)
    sys_name = QLatin1String("Mac OS X");
#elif defined(Q_OS_WIN)
    sys_name = QLatin1String("Windows");
#elif defined(Q_OS_UNIX)
    sys_name = QLatin1String("Unix");

    struct ::utsname utsinfo;
    if (::uname(&utsinfo) >= 0)
        sys_name = QString::fromLocal8Bit(utsinfo.sysname);
#endif

    return sys_name;
}

QString Application::systemVersion() const
{
    QString sys_version = tr("(unknown)");

#if defined(Q_OS_MAC)
    switch (QSysInfo::MacintoshVersion) {
    case QSysInfo::MV_10_0: sys_version = QLatin1String("10.0 (Cheetah)"); break;
    case QSysInfo::MV_10_1: sys_version = QLatin1String("10.1 (Puma)");    break;
    case QSysInfo::MV_10_2: sys_version = QLatin1String("10.2 (Jaguar)");  break;
    case QSysInfo::MV_10_3: sys_version = QLatin1String("10.3 (Panther)"); break;
    case QSysInfo::MV_10_4: sys_version = QLatin1String("10.4 (Tiger)");   break;
    case QSysInfo::MV_10_5: sys_version = QLatin1String("10.5 (Leopard)"); break;
    case QSysInfo::MV_10_6: sys_version = QLatin1String("10.6 (Snow Leopard)"); break;
    case QSysInfo::MV_10_6 + 1: sys_version = QLatin1String("10.7 (Lion)"); break;
    default               : break;
    }
#elif defined(Q_OS_WIN)
    switch (QSysInfo::WindowsVersion) {
    case QSysInfo::WV_95 : sys_version = QLatin1String("95");    break;
    case QSysInfo::WV_98 : sys_version = QLatin1String("98");    break;
    case QSysInfo::WV_Me : sys_version = QLatin1String("ME");    break;
    case QSysInfo::WV_4_0: sys_version = QLatin1String("NT");    break;
    case QSysInfo::WV_5_0: sys_version = QLatin1String("2000");  break;
    case QSysInfo::WV_5_1: sys_version = QLatin1String("XP");    break;
    case QSysInfo::WV_5_2: sys_version = QLatin1String("2003");  break;
    case QSysInfo::WV_6_0: sys_version = QLatin1String("VISTA"); break;
    case QSysInfo::WV_6_1: sys_version = QLatin1String("7");     break;
    default              : break;
    }
#elif defined(Q_OS_UNIX)
    struct ::utsname utsinfo;
    if (::uname(&utsinfo) >= 0) {
        QByteArray dist, release, nick;
        QProcess lsbrel;

        lsbrel.start(QLatin1String("lsb_release -a"));

        if (lsbrel.waitForStarted(1000) && lsbrel.waitForFinished(2000)) {
            QList<QByteArray> out = lsbrel.readAllStandardOutput().split('\n');

            foreach (QByteArray line, out) {
                QByteArray val = line.mid(line.indexOf(':')+1).simplified();

                if (line.startsWith("Distributor ID:"))
                    dist = val;
                else if (line.startsWith("Release:"))
                    release = val;
                else if (line.startsWith("Codename:"))
                    nick = val;
            }
        }
        if (dist.isEmpty() && release.isEmpty())
            sys_version = QString("%1 (%2)").arg(QString::fromLocal8Bit(utsinfo.machine),
                                                 QString::fromLocal8Bit(utsinfo.release));
        else
            sys_version = QString("%1 (%2 %3/%4)").arg(QString::fromLocal8Bit(utsinfo.machine),
                                                       QString::fromLocal8Bit(dist.constData()),
                                                       QString::fromLocal8Bit(release.constData()),
                                                       QString::fromLocal8Bit(nick.constData()));
    }
#endif

    return sys_version;
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

#if defined(Q_OS_WIN)
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
            connect(server, SIGNAL(newConnection()), this, SLOT(clientMessage()));
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
#if defined(Q_OS_WIN)
                Sleep(timeout / 4);
#else
                struct timespec ts = { (timeout / 4) / 1000, ((timeout / 4) % 1000) * 1000 * 1000 };
                ::nanosleep(&ts, 0);
#endif
            }
        }

        if (client.state() == QLocalSocket::ConnectedState) {
            QStringList files;
            for (int i = 1; i < argc(); i++) {
                QFileInfo fi(argv()[i]);
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

#if defined(Q_OS_WIN)
    defdatadir += QLatin1String("/brickstore-cache/");
#else
    defdatadir += QLatin1String("/.brickstore-cache/");
#endif

    BrickLink::Core *bl = BrickLink::create(Config::inst()->value(QLatin1String("/BrickLink/DataDir"), defdatadir).toString(), &errstring);

    if (!bl)
        MessageBox::critical(0, tr("Could not initialize the BrickLink kernel:<br /><br />%1").arg(errstring));

    bl->setTransfer(new Transfer(10));
    bl->transfer()->setProxy(Config::inst()->proxy());

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
        "<td valign=\"middle\" align=\"right\" width=\"30%\"><img src=\":/images/icon.png\" /></td>"
        "<td align=\"left\" width=\"70%\"><big>"
        "<big><strong>%1</strong></big>"
        "<br />%2<br />"
        "<strong>%3</strong>"
        "</big></td>"
        "</tr></table>"
        "</center><center>"
        "<br />%4<br /><br />%5"
        "</center>%6<p>%7</p>");


    QString page1_link = QLatin1String("<strong>%1</strong> | <a href=\"system\">%2</a>");
    page1_link = page1_link.arg(tr("Legal Info"), tr("System Info"));
    QString page2_link = QLatin1String("<a href=\"index\">%1</a> | <strong>%2</strong>");
    page2_link = page2_link.arg(tr("Legal Info"), tr("System Info"));

    QString copyright = tr("Copyright &copy; %1").arg(BRICKSTORE_COPYRIGHT);
    QString version   = tr("Version %1").arg(BRICKSTORE_VERSION);
    QString support   = tr("Visit %1, or send an email to %2").arg("<a href=\"http://" BRICKSTORE_URL "\">" BRICKSTORE_URL "</a>",
                                                                   "<a href=\"mailto:" BRICKSTORE_MAIL "\">" BRICKSTORE_MAIL "</a>");

    QString qt = qVersion();

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
        "<br />"
        "This program is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE "
        "WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE."
        "<br />"
        "See <a href=\"http://fsf.org/licensing/licenses/gpl.html\">www.fsf.org/licensing/licenses/gpl.html</a> for GPL licensing information."
        "</p><p>"
        "All data from <a href=\"http://www.bricklink.com\">www.bricklink.com</a> is owned by BrickLink<sup>TM</sup>, "
        "which is a trademark of Dan Jezek."
        "</p><p>"
        "Peeron Inventories from <a href=\"http://www.peeron.com\">www.peeron.com</a> are owned by Dan and Jennifer Boger."
        "</p><p>"
        "LEGO<sup>&reg;</sup> is a trademark of the LEGO group of companies, "
        "which does not sponsor, authorize or endorse this software."
        "</p><p>"
        "All other trademarks recognised."
        "</p>");

    QString technical = QLatin1String(
        "<p>"
        "<table>"
        "<th colspan=\"2\" align=\"left\">Build Info</th>"
        "<tr><td>User     </td><td>%1</td></tr>"
        "<tr><td>Host     </td><td>%2</td></tr>"
        "<tr><td>Date     </td><td>%3</td></tr>"
        "<tr><td>Compiler </td><td>%4</td></tr>"
        "</table><br />"
        "<table>"
        "<th colspan=\"2\" align=\"left\">Runtime Info</th>"
        "<tr><td>OS     </td><td>%5</td></tr>"
        "<tr><td>Memory </td><td>%L6 MB</td></tr>"
        "<tr><td>Qt     </td><td>%7</td></tr>"
        "</table>"
        "</p>");

    technical = technical.arg(STR(__USER__), STR(__HOST__), __DATE__ " " __TIME__).arg(
#if defined(_MSC_VER)
                         "Microsoft Visual-C++ "
#  if _MSC_VER >= 1600
                         "2010"
#  elif _MSC_VER >= 1500
                         "2008"
#  elif _MSC_VER >= 1400
                         "2005"
#  elif _MSC_VER >= 1310
                         ".NET 2003"
#  elif _MSC_VER >= 1300
                         ".NET 2002"
#  elif _MSC_VER >= 1200
                         "6.0"
#  else
                         "???"
#  endif
#elif defined(__GNUC__)
                         "GCC " __VERSION__
#else
                         "???"
#endif
                         ).arg(systemName() + QLatin1Char(' ') + systemVersion()).arg(Utility::physicalMemory()/(1024ULL*1024ULL)).arg(qt);

    QString page1 = layout.arg(applicationName(), copyright, version, support).arg(page1_link, legal, translators);
    QString page2 = layout.arg(applicationName(), copyright, version, support).arg(page2_link, technical, QString());

    QMap<QString, QString> pages;
    pages ["index"]  = page1;
    pages ["system"] = page2;

    InformationDialog d(applicationName(), pages, false, FrameWork::inst());
    d.exec();
}

void Application::checkForUpdates()
{
    Transfer trans(1);
    trans.setProxy(Config::inst()->proxy());

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

#elif defined(Q_OS_MAC)
    struct ::sockaddr_in sock;
    sock.sin_family = AF_INET;
    sock.sin_port = htons(80);
    sock.sin_addr.s_addr = inet_addr(CHECK_IP);
    SCNetworkConnectionFlags flags = 0;
    bool result = SCNetworkCheckReachabilityByAddress(reinterpret_cast<sockaddr *>(&sock), sizeof(sock), &flags);

    //qWarning() << "Mac NET change: " << result << flags;
    if (!result || (flags & (kSCNetworkFlagsReachable | kSCNetworkFlagsConnectionRequired)) == kSCNetworkFlagsReachable)
        online = true;

#elif defined(Q_OS_WIN)
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
