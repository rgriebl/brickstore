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
#include <QProcess>
#include <QDesktopServices>
#include <QLocalSocket>
#include <QLocalServer>
#include <QNetworkProxyFactory>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QStringBuilder>

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
#elif defined(Q_OS_UNIX)
#  include <sys/utsname.h>
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

#define XSTR(a) #a
#define STR(a) XSTR(a)

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

    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);

#if !defined(Q_OS_WINDOWS) // HighDPI work fine, but only without this setting
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#  if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#  endif
#endif

    new QApplication(_argc, _argv);

    setupLogging();

    qApp->installEventFilter(this);

    setIconTheme();

#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_MACOS)
    QPixmap pix(":/images/brickstore_icon.png");
    if (!pix.isNull())
        QGuiApplication::setWindowIcon(pix);
#endif
#if defined(Q_OS_MACOS)
    QGuiApplication::setAttribute(Qt::AA_DontShowIconsInMenus); // mac style guide

    // macOS style guide doesn't want shortcut keys in dialogs (Alt + underlined character)
    qApp->setStyle(new MacUnderlineStyle());
    void qt_set_sequence_auto_mnemonic(bool on);
    qt_set_sequence_auto_mnemonic(true);

#  if QT_VERSION <= QT_VERSION_CHECK(5, 15, 2)
    // the new default font San Francisco has rendering problems: QTBUG-88495
    if (QVersionNumber::fromString(QSysInfo::productVersion()).majorVersion() >= 11) {
        QFont f = QApplication::font();
        f.setFamily("Helvetica Neue");
        QApplication::setFont(f);
    }
#  endif
#endif

    m_default_fontsize = QGuiApplication::font().pointSizeF();
    qApp->setProperty("_bs_defaultFontSize", m_default_fontsize); // the settings dialog needs this

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

    checkNetwork();
    auto *netcheck = new QTimer(this);
    connect(netcheck, &QTimer::timeout,
            this, &Application::checkNetwork);
    netcheck->start(5s);

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    //TODO5: find out why we are blacklisted ... for now, fake the UA
    Transfer::setDefaultUserAgent("Br1ckstore" + QLatin1Char('/') + QCoreApplication::applicationVersion() +
                                  QLatin1String(" (") + QSysInfo::prettyProductName() +
                                  QLatin1Char(')'));

    // initialize config & resource
    (void) Config::inst()->upgrade(BRICKSTORE_MAJOR, BRICKSTORE_MINOR);
    (void) Currency::inst();
    (void) ScriptManager::inst();

    if (!initBrickLink()) {
        // we cannot call quit directly, since there is no event loop to quit from...
        QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }
    if (LDraw::create(Config::inst()->ldrawDir(), nullptr)) {
        BrickLink::core()->setLDrawDataPath(LDraw::core()->dataPath());
    }

    updateTranslations();
    connect(Config::inst(), &Config::languageChanged,
            this, &Application::updateTranslations);

    m_files_to_open << QCoreApplication::arguments().mid(1);

    MessageBox::setDefaultParent(FrameWork::inst());

    FrameWork::inst()->show();
#if defined(Q_OS_MACOS)
    FrameWork::inst()->raise();

#elif defined(Q_OS_WIN)
    RegisterApplicationRestart(nullptr, 0); // make us restart-able by installers

#endif
}

Application::~Application()
{
    exitBrickLink();

    delete ScriptManager::inst();
    delete Currency::inst();
    delete Config::inst();

    delete qApp;
}

QStringList Application::externalResourceSearchPath(const QString &subdir) const
{
    static QStringList baseSearchPath;
    const bool isDeveloperBuild =
#if defined(QT_NO_DEBUG)
            false;
#else
            true;
#endif

    if (baseSearchPath.isEmpty()) {
        QString appdir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WINDOWS)
        baseSearchPath << appdir;

        if (isDeveloperBuild)
            baseSearchPath << appdir + QLatin1String("/..");
#elif defined(Q_OS_MACOS)
        Q_UNUSED(isDeveloperBuild)
        baseSearchPath << appdir + QLatin1String("/../Resources");
#elif defined(Q_OS_UNIX)
        baseSearchPath << QLatin1String(STR(INSTALL_PREFIX) "/share/brickstore");

        if (isDeveloperBuild)
            baseSearchPath << appdir;
#endif
    }
    if (subdir.isEmpty()) {
        return baseSearchPath;
    } else {
        QStringList searchPath;
        for (const QString &bsp : qAsConst(baseSearchPath))
            searchPath << bsp + QDir::separator() + subdir;
        return searchPath;
    }
}

QPlainTextEdit *Application::logWidget() const
{
    return m_logWidget;
}

void Application::updateTranslations()
{
    QString locale = Config::inst()->language();
    if (locale.isEmpty())
        locale = QLocale::system().name();
    QLocale::setDefault(QLocale(locale));

    if (m_trans_qt)
        QCoreApplication::removeTranslator(m_trans_qt.data());
    if (m_trans_brickstore)
        QCoreApplication::removeTranslator(m_trans_brickstore.data());
    m_trans_qt.reset(new QTranslator());
    m_trans_brickstore.reset(new QTranslator());
    if (!m_trans_brickstore_en)
        m_trans_brickstore_en.reset(new QTranslator());

    QString i18n = ":/i18n";

    static bool once = false; // always load the english plural forms
    if (!once) {
        if (m_trans_brickstore_en->load("brickstore_en", i18n))
            QCoreApplication::installTranslator(m_trans_brickstore_en.data());
        once = true;
    }

    if (locale != "en") {
        if (m_trans_qt->load(QLatin1String("qtbase_") + locale, i18n))
            QCoreApplication::installTranslator(m_trans_qt.data());
        else
            m_trans_qt.reset();

        if (m_trans_brickstore->load(QLatin1String("brickstore_") + locale, i18n))
            QCoreApplication::installTranslator(m_trans_brickstore.data());
        else
            m_trans_brickstore.reset();
    }
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
    while (m_enable_emit && !m_files_to_open.isEmpty())
        emit openDocument(m_files_to_open.takeFirst());
}

bool Application::eventFilter(QObject *o, QEvent *e)
{
    if ((o != qApp) || !e)
        return false;

    switch (e->type()) {
    case QEvent::FileOpen:
        m_files_to_open.append(static_cast<QFileOpenEvent *>(e)->file());
        doEmitOpenDocument();
        return true;
    case QEvent::PaletteChange:
        setIconTheme();
        break;
    }
    return QObject::eventFilter(o, e);
}

bool Application::isClient(int timeout)
{
    enum { Undecided, Server, Client } state = Undecided;
    QString socketName = QLatin1String("BrickStore");
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

static const char16_t msgTypeNames[][5] = { u"DBG ", u"WARN", u"CRIT", u"FATL", u"INFO" };

class LogHighlighter : public QSyntaxHighlighter
{
public:
    explicit LogHighlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent)
    {
        m_lvlFmt[QtDebugMsg].setForeground(Qt::black);
        m_lvlFmt[QtDebugMsg].setBackground(Qt::green);
        m_lvlFmt[QtWarningMsg].setForeground(Qt::black);
        m_lvlFmt[QtWarningMsg].setBackground(Qt::yellow);
        m_lvlFmt[QtCriticalMsg].setForeground(Qt::red);
        m_lvlFmt[QtCriticalMsg].setBackground(Qt::black);
        m_lvlFmt[QtFatalMsg].setForeground(Qt::white);
        m_lvlFmt[QtFatalMsg].setBackground(Qt::red);
        m_lvlFmt[QtInfoMsg].setForeground(Qt::black);
        m_lvlFmt[QtInfoMsg].setBackground(Qt::blue);
        for (int i = 0; i < 6; ++i) {
            QColor c = QColor::fromHsl(60 *i, 210, 128);
            m_catFmt[i].setForeground(c);
            m_catFmt[i].clearBackground();
            m_catFmt[i].setFontWeight(QFont::Bold);
        }
        m_atFmt.setForeground(Qt::darkMagenta);
        m_atFmt.setFontWeight(QFont::Bold);
        m_lineFmt.setForeground(Qt::magenta);
        m_lineFmt.setFontWeight(QFont::Bold);
    }

    void highlightBlock(const QString &text) override;
private:
    QTextCharFormat m_lvlFmt[QtInfoMsg + 1];
    QTextCharFormat m_catFmt[6];
    QTextCharFormat m_atFmt;
    QTextCharFormat m_lineFmt;
};

void LogHighlighter::highlightBlock(const QString &text)
{
    ++Application::s_inst->m_logGuiLock;

    int colonPos = text.indexOf(':', 1);
    if (colonPos > 0) {
        int atPos = text.lastIndexOf(" at ");
        int linePos = text.lastIndexOf(", line ");

        int atLen = 0;
        int lineLen = 0;
        bool hasLocation = (atPos > 0 && linePos > atPos);

        if (hasLocation) {
            atPos += 4;
            atLen = linePos - atPos;
            linePos += 7;
            lineLen = text.length() - linePos;
        }

        static const int lvlPos = 0;
        static const int lvlLen = 4;
        static const int catPos = lvlPos + lvlLen + 1;

        int catLen = colonPos - catPos;

        int msgType = -1;
        for (int i = 0; i < int(sizeof(msgTypeNames) / sizeof(*msgTypeNames)); ++i) {
            if (text.midRef(lvlPos, lvlLen) == msgTypeNames[i])
                msgType = i;
        }

        if (msgType >= 0) {
            int catType = qHash(text.midRef(catPos, catLen)) % 6;

            setFormat(lvlPos, lvlLen, m_lvlFmt[msgType]);
            setFormat(catPos, catLen, m_catFmt[catType]);
        }

        if (hasLocation) {
            setFormat(linePos, lineLen, m_lineFmt);
            setFormat(atPos, atLen, m_atFmt);
        }
    }

    --Application::s_inst->m_logGuiLock;
}


void Application::setupLogging()
{
    m_logWidget = new QPlainTextEdit();
    m_logWidget->setObjectName("LogWidget");
    m_logWidget->setReadOnly(true);
    m_logWidget->setMaximumBlockCount(1000);
    m_logWidget->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    new LogHighlighter(m_logWidget->document());

    auto msgHandler = [](QtMsgType msgType, const QMessageLogContext &msgCtx, const QString &msg) {
        if (s_inst && s_inst->m_defaultMessageHandler)
            (*s_inst->m_defaultMessageHandler)(msgType, msgCtx, msg);

        if (!s_inst || s_inst->m_logGuiLock || !s_inst->m_logWidget)
            return;

        QString filename;
        if (msgCtx.file && msgCtx.file[0] && msgCtx.line > 1) {
            filename = QString::fromLocal8Bit(msgCtx.file);
            int pos = -1;
#if defined(Q_OS_WIN)
            pos = filename.lastIndexOf('\\');
#endif
            if (pos < 0)
                pos = filename.lastIndexOf('/');
            filename = filename.mid(pos + 1);
        }

        QString str = QString() % msgTypeNames[qBound(QtDebugMsg, msgType, QtInfoMsg)]
                % u" " % QLatin1String(msgCtx.category)
                % u": " % msg;
        if (!filename.isEmpty())
            str = str % u" at " % filename % u", line " % QString::number(msgCtx.line);

        QMetaObject::invokeMethod(s_inst->m_logWidget, "appendPlainText", Qt::QueuedConnection,
                                  Q_ARG(QString, str));
    };
    m_defaultMessageHandler = qInstallMessageHandler(msgHandler);
}

void Application::setIconTheme()
{
    static bool once = false;
    if (!once) {
        QIcon::setThemeSearchPaths({ QLatin1String(":/assets/icons") });
        once = true;
    }

    const auto pal = QGuiApplication::palette();
    auto winColor = pal.color(QPalette::Active, QPalette::Window);
    bool dark = ((winColor.lightnessF() * winColor.alphaF()) < 0.5);

    QIcon::setThemeName(dark ? "brickstore-breeze-dark" : "brickstore-breeze");
}

bool Application::initBrickLink()
{
    QString errstring;

    BrickLink::Core *bl = BrickLink::create(Config::inst()->dataDir(), &errstring);
    if (!bl) {
        MessageBox::critical(nullptr, { }, tr("Could not initialize the BrickLink kernel:<br /><br />%1").arg(errstring));
    } else {
        bl->setItemImageScaleFactor(Config::inst()->itemImageSizePercent() / 100.);
        connect(Config::inst(), &Config::itemImageSizePercentChanged,
                this, [](qreal p) { BrickLink::core()->setItemImageScaleFactor(p / 100.); });
        bl->setTransfer(new Transfer);

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

#include "moc_application.cpp"
