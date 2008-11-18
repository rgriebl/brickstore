/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#if defined( Q_OS_UNIX )
#  include <sys/utsname.h>
#endif

#include "informationdialog.h"
#include "registrationdialog.h"
#include "progressdialog.h"
#include "checkforupdates.h"
#include "config.h"
#include "money.h"
#include "rebuilddatabase.h"
#include "bricklink.h"
#include "ldraw.h"
#include "splash.h"
#include "messagebox.h"
#include "framework.h"
#include "transfer.h"
//#include "report.h"

#include "filteredit.h"

#include "utility.h"
#include "version.h"

#include "application.h"


#define XSTR(a) #a
#define STR(a) XSTR(a)


Application *cApp = 0;

Application::Application(bool rebuild_db_only, int _argc, char **_argv)
    : QApplication(_argc, _argv, !rebuild_db_only)
{
    cApp = this;

    m_enable_emit = false;
    m_has_alpha = rebuild_db_only ? false : (QPixmap::defaultDepth() >= 15);

    setOrganizationName("Softforge");
    setOrganizationDomain("softforge.de");
    setApplicationName(appName());

#if defined(Q_WS_X11)
    QPixmap pix(":/images/icon");
    if (!pix.isNull())
        setWindowIcon(pix);
    else
        qWarning("No window icon");
#endif


    Transfer::setDefaultUserAgent(appName() + "/" + cApp->appVersion() + " (" + cApp->sysName() + " " + cApp->sysVersion() + "; http://" + cApp->appURL() + ")");

    if (!rebuild_db_only)
        Splash::inst();

#if defined( Q_WS_WIN )
    int wv = QSysInfo::WindowsVersion;

    // don't use the native file dialogs on Windows < XP, since it
    // (a) may crash on some configurations (not yet checked with Qt4) and
    // (b) the Qt dialog is more powerful on these systems
    extern bool Q_GUI_EXPORT qt_use_native_dialogs;
    qt_use_native_dialogs = !((wv & QSysInfo::WV_DOS_based) || ((wv & QSysInfo::WV_NT_based) < QSysInfo::WV_XP));

#endif

    // initialize config & resource
    (void) Config::inst()->upgrade(BRICKSTORE_MAJOR, BRICKSTORE_MINOR, BRICKSTORE_PATCH);
    (void) Money::inst();
//    (void) CReportManager::inst ( );

    m_trans_qt = 0;
    m_trans_brickstore = 0;

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
        updateTranslations();
        connect(Config::inst(), SIGNAL(languageChanged()), this, SLOT(updateTranslations()));

        Splash::inst()->message(qApp->translate("Splash", "Initializing..."));

        MessageBox::setDefaultTitle(appName());

        for (int i = 1; i < argc(); i++)
            m_files_to_open << argv()[i];

        Splash::inst()->finish(FrameWork::inst());
        FrameWork::inst()->show();

        while (Config::inst()->registration() == Config::None) {
            RegistrationDialog d(true, FrameWork::inst());
            d.exec();
        }
        demoVersion();
        connect(Config::inst(), SIGNAL(registrationChanged(Config::Registration)), this, SLOT(demoVersion()));
    }
}

Application::~Application()
{
    exitBrickLink();

//    delete CReportManager::inst ( );
    delete Money::inst();
    delete Config::inst();
}

bool Application::pixmapAlphaSupported() const
{
    return m_has_alpha;
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

    if (qSharedBuild())
        m_trans_qt->load(QLibraryInfo::location(QLibraryInfo::TranslationsPath) + "/qt_" + locale);
    else
        m_trans_qt->load(":/translations/qt_" + locale);

    m_trans_brickstore->load(":/translations/brickstore_" + locale);

    if (!m_trans_qt->isEmpty())
        installTranslator(m_trans_qt);
    if (!m_trans_brickstore->isEmpty())
        installTranslator(m_trans_brickstore);
}

void Application::rebuildDatabase()
{
    RebuildDatabase rdb;
    exit(rdb.exec());
}


QString Application::appName() const
{
    return "BSE";
}

QString Application::appVersion() const
{
    return BRICKSTORE_VERSION;
}

QString Application::appURL() const
{
    return BRICKSTORE_URL;
}

QString Application::sysName() const
{
    QString sys_name = "(unknown)";

#if defined( Q_OS_MACX )
    sys_name = "Mac OS X";
#elif defined( Q_OS_WIN )
    sys_name = "Windows";
#elif defined( Q_OS_UNIX )
    sys_name = "Unix";

    struct ::utsname utsinfo;
    if (::uname(&utsinfo) >= 0)
        sys_name = utsinfo.sysname;
#endif

    return sys_name;
}

QString Application::sysVersion() const
{
    QString sys_version = "(unknown)";

#if defined( Q_OS_MACX )
    switch (QSysInfo::MacintoshVersion) {
    case QSysInfo::MV_10_0  : sys_version = "10.0 (Cheetah)"; break;
    case QSysInfo::MV_10_1  : sys_version = "10.1 (Puma)";    break;
    case QSysInfo::MV_10_2  : sys_version = "10.2 (Jaguar)";  break;
    case QSysInfo::MV_10_3  : sys_version = "10.3 (Panther)"; break;
    case QSysInfo::MV_10_4  : sys_version = "10.4 (Tiger)";   break;
    case QSysInfo::MV_10_5  : sys_version = "10.5 (Leopard)"; break;
    case QSysInfo::MV_10_5+1: sys_version = "10.6 (Snow Leopard)"; break; //TODO: replace with MV_10_6 in Qt 4.6
    default               : break;
    }
#elif defined( Q_OS_WIN )
    switch (QSysInfo::WindowsVersion) {
    case QSysInfo::WV_95   : sys_version = "95";   break;
    case QSysInfo::WV_98   : sys_version = "98";   break;
    case QSysInfo::WV_Me   : sys_version = "ME";   break;
    case QSysInfo::WV_NT   : sys_version = "NT";   break;
    case QSysInfo::WV_2000 : sys_version = "2000"; break;
    case QSysInfo::WV_XP   : sys_version = "XP";   break;
    case QSysInfo::WV_2003 : sys_version = "2003"; break;
    case QSysInfo::WV_VISTA: sys_version = "VISTA"; break;
    default                : break;
    }
#elif defined( Q_OS_UNIX )
    struct ::utsname utsinfo;
    if (::uname(&utsinfo) >= 0) {
        QByteArray dist, release, nick;
        QProcess lsbrel;

        lsbrel.start("lsb_release -a");

        if (lsbrel.waitForStarted(1000) && lsbrel.waitForFinished(2000)) {
            QList<QByteArray> out = lsbrel.readAllStandardOutput().split('\n');

            foreach (QByteArray line, out) {
                QByteArray val = line.mid(line.indexOf(':')).simplified();

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

bool Application::initBrickLink()
{
    QString errstring;
    QString defdatadir = QDir::homePath();

#if defined( Q_OS_WIN32 )
    defdatadir += "/brickstore-cache/";
#else
    defdatadir += "/.brickstore-cache/";
#endif

    BrickLink::Core *bl = BrickLink::create(Config::inst()->value("/BrickLink/DataDir", defdatadir).toString(), &errstring);

    if (!bl)
        MessageBox::critical(0, tr("Could not initialize the BrickLink kernel:<br /><br />%1").arg(errstring));
    
    bl->setTransfer(new Transfer(10));
    bl->transfer()->setProxy(Config::inst()->proxy());

    LDraw::Core *ld = LDraw::create(QString(), &errstring);

    if (!ld)
        MessageBox::critical(0, tr("Could not initialize the LDraw kernel:<br /><br />%1").arg(errstring));

    return (bl != 0); // && (ld != 0);
}


void Application::exitBrickLink()
{
    delete BrickLink::core();
    delete LDraw::core();
}


void Application::about()
{
    static const char *layout =
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
        "</center>%6<p>%7</p>";


    QString page1_link = QString("<strong>%1</strong> | <a href=\"system\">%2</a>").arg(tr("Legal Info"), tr("System Info"));
    QString page2_link = QString("<a href=\"index\">%1</a> | <strong>%2</strong>").arg(tr("Legal Info"), tr("System Info"));

    QString copyright = tr("Copyright &copy; %1").arg(BRICKSTORE_COPYRIGHT);
    QString version   = tr("Version %1").arg(BRICKSTORE_VERSION);
    QString support   = tr("Visit %1, or send an email to %2").arg("<a href=\"http://" BRICKSTORE_URL "\">" BRICKSTORE_URL "</a>",
                                                                   "<a href=\"mailto:" BRICKSTORE_MAIL "\">" BRICKSTORE_MAIL "</a>");

    QString qt   = qVersion();

    QString translators = "<b>" + tr("Translators") + "</b><table border=\"0\">";

    foreach (const Config::Translation &trans, Config::inst()->translations()) {
        if (trans.language != QLatin1String("en")) {
            QString langname = trans.languageName.value(QLocale().name().left(2), trans.languageName[QLatin1String("en")]);
            translators += QString("<tr><td>%1</td><td width=\"2em\"></td><td>%2 &lt;<a href=\"mailto:%3\">%4</a>&gt;</td></tr>").arg(langname, trans.author, trans.authorEMail, trans.authorEMail);
        }
    }

    translators += "</table>";

    static const char *legal_src = QT_TR_NOOP(
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
                                       "</p>"
                                   );

    static const char *technical_src =
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
        "<tr><td>libqt  </td><td>%7</td></tr>"
        "</table>"
        "</p>";

    QString technical = QString(technical_src).arg(STR(__USER__), STR(__HOST__), __DATE__ " " __TIME__).arg(
#if defined(_MSC_VER)
                         "Microsoft Visual-C++ "
#  if _MSC_VER >= 1500
                         ".NET 2008"
#  elif _MSC_VER >= 1400
                         ".NET 2005"
#  elif _MSC_VER >= 1310
                         ".NET 2003"
#  elif _MSC_VER >= 1300
                         ".NET"
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
                         ).arg(sysName() + " " + sysVersion()).arg(Utility::physicalMemory()/(1024ULL*1024ULL)).arg(qt);

    QString legal = tr(legal_src);

    QString page1 = QString(layout).arg(appName(), copyright, version, support).arg(page1_link, legal, translators);
    QString page2 = QString(layout).arg(appName(), copyright, version, support).arg(page2_link, technical, QString());

    QMap<QString, QString> pages;
    pages ["index"]  = page1;
    pages ["system"] = page2;

    InformationDialog d(appName(), pages, false, FrameWork::inst());
    d.exec();
}

void Application::demoVersion()
{
    if (Config::inst()->registration() != Config::Demo)
        return;

    static const char *layout =
        "<qt><center>"
        "<table border=\"0\"><tr>"
        "<td valign=\"middle\" align=\"right\"><img src=\":/images/icon.png\" /></td>"
        "<td align=\"left\"><big>"
        "<big><strong>%1</strong></big>"
        "<br />%2<br />"
        "<strong>%3</strong>"
        "</big></td>"
        "</tr></table>"
        "<br />%4"
        "</center</qt>";

    static const char *demo_src = QT_TR_NOOP(
                                      "BrickStore is currently running in <b>Demo</b> mode.<br /><br />"
                                      "The complete functionality is accessible, but this reminder will pop up every 20 minutes."
                                      "<br /><br />"
                                      "You can change the mode of operation at anytime via <b>Help > Registration...</b>"
                                  );

    QString copyright = tr("Copyright &copy; %1").arg(BRICKSTORE_COPYRIGHT);
    QString version   = tr("Version %1").arg(BRICKSTORE_VERSION);
    QString demo      = tr(demo_src);
    QString text      = QString(layout).arg(appName(), copyright, version, demo);

    QMap<QString, QString> pages;
    pages ["index"] = text;

    InformationDialog d(appName(), pages, true, FrameWork::inst());
    d.exec();

    QTimer::singleShot(20*60*1000, this, SLOT(demoVersion()));
}

void Application::checkForUpdates()
{
    Transfer trans(1);
    trans.setProxy(Config::inst()->proxy());

    ProgressDialog d(&trans, FrameWork::inst());
    CheckForUpdates cfu(&d);
    d.exec();
}

void Application::registration()
{
    Config::Registration oldreg = Config::inst()->registration();

    RegistrationDialog d(false, FrameWork::inst());
    if ((d.exec() == QDialog::Accepted) &&
        (Config::inst()->registration() == Config::Demo) &&
        (Config::inst()->registration() != oldreg))
    {
        MessageBox::information(FrameWork::inst(), tr("The program has to be restarted to activate the Demo mode."));
        FrameWork::inst()->close();
        quit();
    }
}

