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

#if defined( Q_OS_UNIX )
#include <sys/utsname.h>
#endif

#include "cframework.h"
#if 0
#include "creport.h"
#endif


#include "dimportorder.h"
#include "cselectcolor.h"


#include "dinformation.h"
#include "dregistration.h"
#include "cprogressdialog.h"
#include "ccheckforupdates.h"
#include "cconfig.h"
#include "cmoney.h"
#include "crebuilddatabase.h"
#include "bricklink.h"
#include "csplash.h"
#include "cmessagebox.h"

#include "version.h"

#include "capplication.h"


#define XSTR(a) #a
#define STR(a) XSTR(a)

#if defined( Q_WS_WIN )
#include "dotnetstyle.h"
#endif

CApplication *cApp = 0;

CApplication::CApplication(const char *rebuild_db_only, int _argc, char **_argv)
    : QApplication(_argc, _argv, !rebuild_db_only)
{
    cApp = this;

    m_enable_emit = false;
    m_rebuild_db_only = rebuild_db_only;
    m_has_alpha = (QPixmap::defaultDepth() >= 15);

    setOrganizationName("Softforge");
    setOrganizationDomain("softforge.de");
    setApplicationName(appName());

    if (m_rebuild_db_only.isEmpty())
        CSplash::inst();

#if defined( Q_WS_WIN )
    int wv = QSysInfo::WindowsVersion;

    // don't use the native file dialogs on Windows < XP, since it
    // (a) may crash on some configurations (not yet checked with Qt4) and
    // (b) the Qt dialog is more powerful on these systems
    extern bool Q_GUI_EXPORT qt_use_native_dialogs;
    qt_use_native_dialogs = !((wv & QSysInfo::WV_DOS_based) || ((wv & QSysInfo::WV_NT_based) < QSysInfo::WV_XP));

    // setStyle(new DotNetStyle());
#endif

    // initialize config & resource
    (void) CConfig::inst()->upgrade(BRICKSTORE_MAJOR, BRICKSTORE_MINOR, BRICKSTORE_PATCH);
    (void) CMoney::inst();
// (void) CReportManager::inst ( );

    m_trans_qt = 0;
    m_trans_brickstore = 0;

    if (!initBrickLink()) {
        // we cannot call quit directly, since there is
        // no event loop to quit from...
        postEvent(this, new QEvent(QEvent::Quit));
        return;
    }
    else if (!m_rebuild_db_only.isEmpty()) {
        QTimer::singleShot(0, this, SLOT(rebuildDatabase()));
    }
    else {
        updateTranslations();
        connect(CConfig::inst(), SIGNAL(languageChanged()), this, SLOT(updateTranslations()));

        CSplash::inst()->message(qApp->translate("CSplash", "Initializing..."));

        CMessageBox::setDefaultTitle(appName());

        for (int i = 1; i < argc(); i++)
            m_files_to_open << argv()[i];

        CSplash::inst()->finish(CFrameWork::inst());
        CFrameWork::inst()->show();

        while (CConfig::inst()->registration() == CConfig::None) {
            DRegistration d(true, CFrameWork::inst());
            d.exec();
        }
        demoVersion();
        connect(CConfig::inst(), SIGNAL(registrationChanged(CConfig::Registration)), this, SLOT(demoVersion()));
    }
}

CApplication::~CApplication()
{
    exitBrickLink();

// delete CReportManager::inst ( );
    delete CMoney::inst();
    delete CConfig::inst();
}

bool CApplication::pixmapAlphaSupported() const
{
    return m_has_alpha;
}

void CApplication::updateTranslations()
{
    QString locale = CConfig::inst()->language();
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

void CApplication::rebuildDatabase()
{
    CRebuildDatabase rdb(m_rebuild_db_only);
    exit(rdb.exec());
}


QString CApplication::appName() const
{
    return "BSE";
}

QString CApplication::appVersion() const
{
    return BRICKSTORE_VERSION;
}

QString CApplication::appURL() const
{
    return BRICKSTORE_URL;
}

QString CApplication::sysName() const
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

QString CApplication::sysVersion() const
{
    QString sys_version = "(unknown)";

#if defined( Q_OS_MACX )
    switch (QSysInfo::MacintoshVersion) {
    case QSysInfo::MV_10_0: sys_version = "10.0 (Cheetah)"; break;
    case QSysInfo::MV_10_1: sys_version = "10.1 (Puma)";    break;
    case QSysInfo::MV_10_2: sys_version = "10.2 (Jaguar)";  break;
    case QSysInfo::MV_10_3: sys_version = "10.3 (Panther)"; break;
    case QSysInfo::MV_10_4: sys_version = "10.4 (Tiger)";   break;
    case QSysInfo::MV_10_5: sys_version = "10.5 (Leopard)"; break;
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
    if (::uname(&utsinfo) >= 0)
        sys_version = QString("%2 (%3)").arg(utsinfo.release, utsinfo.machine);
#endif

    return sys_version;
}

void CApplication::enableEmitOpenDocument(bool b)
{
    if (b != m_enable_emit) {
        m_enable_emit = b;

        if (b && !m_files_to_open.isEmpty())
            QTimer::singleShot(0, this, SLOT(doEmitOpenDocument()));
    }
}

void CApplication::doEmitOpenDocument()
{
    while (m_enable_emit && !m_files_to_open.isEmpty()) {
        QString file = m_files_to_open.front();
        m_files_to_open.pop_front();

        emit openDocument(file);
    }
}

bool CApplication::event(QEvent *e)
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

bool CApplication::initBrickLink()
{
    QString errstring;
    QString defdatadir = QDir::homePath();

#if defined( Q_OS_WIN32 )
    defdatadir += "/brickstore-cache/";
#else
    defdatadir += "/.brickstore-cache/";
#endif

    BrickLink::Core *bl = BrickLink::create(CConfig::inst()->value("/BrickLink/DataDir", defdatadir).toString(), &errstring);

    if (!bl)
        CMessageBox::critical(0, tr("Could not initialize the BrickLink kernel:<br /><br />%1").arg(errstring));

    return (bl != 0);
}


void CApplication::exitBrickLink()
{
    delete BrickLink::core();
}


void CApplication::about()
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
        "</center>%6";


    QString page1_link = QString("<strong>%1</strong> | <a href=\"system\">%2</a>").arg(tr("Legal Info"), tr("System Info"));
    QString page2_link = QString("<a href=\"index\">%1</a> | <strong>%2</strong>").arg(tr("Legal Info"), tr("System Info"));

    QString copyright = tr("Copyright &copy; %1").arg(BRICKSTORE_COPYRIGHT);
    QString version   = tr("Version %1").arg(BRICKSTORE_VERSION);
    QString support   = tr("Visit %1, or send an email to %2").arg("<a href=\"http://" BRICKSTORE_URL "\">" BRICKSTORE_URL "</a>", "<a href=\"mailto:" BRICKSTORE_MAIL "\">" BRICKSTORE_MAIL "</a>");

    QString qt   = qVersion();

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
                                       "</p><p>"
                                       "French translation by Sylvain Perez (<a href=\"mailto:bricklink@1001bricks.com\">bricklink@1001bricks.com</a>)"
                                       "<br />"
                                       "Dutch translation by Eric van Horssen (<a href=\"mailto:horzel@hacktic.nl\">horzel@hacktic.nl</a>)"
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
        "<tr><td>libqt  </td><td>%6</td></tr>"
        "</table>"
        "</p>";

    QString technical = QString(technical_src).arg(STR(__USER__), STR(__HOST__), __DATE__ " " __TIME__).arg(
#if defined( _MSC_VER )
                         "Microsoft Visual-C++ " + QString(_MSC_VER >= 1500 ? ".NET 2008" :
                                                          (_MSC_VER >= 1400 ? ".NET 2005" :
                                                          (_MSC_VER >= 1310 ? ".NET 2003" :
                                                          (_MSC_VER >= 1300 ? ".NET" :
                                                          (_MSC_VER >= 1200 ? "6.0" : "???")))))
#elif defined( __GNUC__ )
                         "GCC " __VERSION__
#else
                         "???"
#endif
                        ).arg(sysName() + " " + sysVersion()).arg(qt);

    QString legal = tr(legal_src);

    QString page1 = QString(layout).arg(appName(), copyright, version, support).arg(page1_link, legal);
    QString page2 = QString(layout).arg(appName(), copyright, version, support).arg(page2_link, technical);

    QMap<QString, QString> pages;
    pages ["index"]  = page1;
    pages ["system"] = page2;

    DInformation d(appName(), pages, false, CFrameWork::inst());
    d.exec();
}

void CApplication::demoVersion()
{
    if (CConfig::inst()->registration() != CConfig::Demo)
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

    DInformation d(appName(), pages, true, CFrameWork::inst());
    d.exec();

    QTimer::singleShot(20*60*1000, this, SLOT(demoVersion()));
}

void CApplication::checkForUpdates()
{
    CProgressDialog d(CFrameWork::inst());
    CCheckForUpdates cfu(&d);
    d.exec();
}

void CApplication::registration()
{
    CConfig::Registration oldreg = CConfig::inst()->registration();

    DRegistration d(false, CFrameWork::inst());
    if ((d.exec() == QDialog::Accepted) &&
        (CConfig::inst()->registration() == CConfig::Demo) &&
        (CConfig::inst()->registration() != oldreg))
    {
        CMessageBox::information(CFrameWork::inst(), tr("The program has to be restarted to activate the Demo mode."));
        CFrameWork::inst()->close();
        quit();
    }
}
