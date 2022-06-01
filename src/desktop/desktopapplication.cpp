/* Copyright (C) 2004-2022 Robert Griebl. All rights reserved.
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
#include <QtCore/QThread>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <QtCore/QCommandLineParser>
#include <QtCore/QProcess>
#include <QtCore/QMessageLogContext>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QSequentialAnimationGroup>
#include <QtCore/QPauseAnimation>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtGui/QPainter>
#include <QtGui/QSurfaceFormat>
#include <QtQml/QQmlApplicationEngine>
#if defined(Q_OS_WINDOWS)
#  if defined(Q_CC_MINGW)
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x0600
#  endif
#  include <windows.h>
#  ifdef MessageBox
#    undef MessageBox
#  endif
#  if defined(Q_CC_MSVC) // needed for themed common controls (e.g. file open dialogs)
#    pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#  endif
#elif defined(Q_OS_MACOS)
#  include <QtCore/QVersionNumber>
#  include <QtGui/private/qguiapplication_p.h>
#  include <QtGui/private/qshortcutmap_p.h>
#endif

#include "bricklink/core.h"
#include "common/config.h"
#include "desktop/brickstoreproxystyle.h"
#include "desktop/desktopuihelpers.h"
#include "desktop/developerconsole.h"
#include "desktop/mainwindow.h"
#include "desktop/scriptmanager.h"
#include "desktop/smartvalidator.h"
#include "utility/utility.h"

#include "desktopapplication.h"


class ToastMessage : public QWidget
{
    Q_OBJECT
public:
    ToastMessage(const QString &message, int timeout)
        : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
        , m_message(message)
        , m_timeout((timeout < 3000) ? 3000 : timeout)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false);

        QPalette pal = palette();
        bool isDark = (pal.color(QPalette::Window).lightness() < 128);
        pal.setBrush(QPalette::WindowText, isDark ? Qt::black : Qt::white);
        pal.setBrush(QPalette::Window, QColor(isDark ? "#dddddd" : "#444444"));
        setPalette(pal);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(20, 10, 20, 10);
        auto *label = new QLabel(u"<b>" % message % u"</b>");
        label->setTextFormat(Qt::AutoText);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label, 1);
    }

    void open(const QRect &area)
    {
        auto fadeIn = new QPropertyAnimation(this, "windowOpacity");
        fadeIn->setStartValue(0.);
        fadeIn->setEndValue(1.);
        fadeIn->setDuration(m_fadeTime);
        auto fadeOut = new QPropertyAnimation(this, "windowOpacity");
        fadeOut->setEndValue(0.);
        fadeOut->setDuration(m_fadeTime);
        auto seq = new QSequentialAnimationGroup(this);
        seq->addAnimation(fadeIn);
        seq->addPause(m_timeout);
        seq->addAnimation(fadeOut);
        connect(seq, &QAnimationGroup::finished, this, &ToastMessage::closed);
        m_animation = seq;

        resize(sizeHint());
        QPoint p = area.center() - rect().center();
        p.ry() += area.height() / 4;
        move(p);
        show();
        seq->start();
    }

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        qreal r = qreal(height()) / 3.;
        p.setBrush(palette().brush(QPalette::Window));
        p.drawRoundedRect(QRectF(rect().adjusted(0, 0, -1, -1)), r, r);
    }

    void mousePressEvent(QMouseEvent *) override
    {
        if (m_animation && (m_animation->state() == QAbstractAnimation::Running)) {
            int total = m_animation->totalDuration();
            if (m_animation->currentTime() < (total - m_fadeTime)) {
                m_animation->pause();
                m_animation->setCurrentTime(total - m_fadeTime);
                m_animation->resume();
            }
        }
    }

private:
    QString m_message;
    int m_timeout = -1;
    QAbstractAnimation *m_animation = nullptr;
    static constexpr int m_fadeTime = 1500;
};


DesktopApplication::DesktopApplication(int &argc, char **argv)
    : Application(argc, argv)
{
#if defined(Q_OS_WINDOWS)
    // the Vista style scales very badly when scaled to non-integer factors
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#else
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

#if defined(Q_OS_LINUX)
    const QByteArray xdgDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    for (const auto *gtkBased : { "GNOME", "MATE", "Cinnamon" }) {
        if (xdgDesktop.contains(gtkBased))
            qputenv("QT_QPA_PLATFORMTHEME", "gtk2");
    }
#endif

    (void) new QApplication(argc, argv);

    m_clp.addHelpOption();
    m_clp.addVersionOption();
    m_clp.addOption({ "load-translation"_l1, "Load the specified translation (testing only)."_l1, "qm-file"_l1 });
    m_clp.addOption({ "new-instance"_l1, "Start a new instance."_l1 });
    m_clp.addPositionalArgument("files"_l1, "The BSX documents to open, optionally."_l1, "[files...]"_l1);
    m_clp.process(QCoreApplication::arguments());

    m_translationOverride = m_clp.value("load-translation"_l1);
    m_queuedDocuments << m_clp.positionalArguments();

    // check for an already running instance
    if (!m_clp.isSet("new-instance"_l1) && notifyOtherInstance())
        exit(0);

#if defined(Q_OS_LINUX)
    QPixmap pix(":/assets/generated-app-icons/brickstore.png"_l1);
    if (!pix.isNull())
        QGuiApplication::setWindowIcon(pix);
#endif
}

void DesktopApplication::init()
{
    DesktopUIHelpers::create();

    Application::init();

#if defined(Q_OS_MACOS)
    // the handling of emacs-style multi-key shortcuts is broken on macOS, because menu
    // shortcuts are handled directly in the QPA plugin, instead of going through the global
    // QShortcutMap. The workaround is override any shortcut while the map is in PartialMatch state.
    new EventFilter(qApp, [](QObject *, QEvent *e) -> bool {
        if (e->type() == QEvent::ShortcutOverride) {
            auto &scm = QGuiApplicationPrivate::instance()->shortcutMap;
            if (scm.state() == QKeySequence::PartialMatch) {
                e->accept();
                return true;
            }
        }
        return false;
    });
#endif

    new EventFilter(qApp, [this](QObject *, QEvent *e) -> bool {
        if (e->type() == QEvent::ApplicationPaletteChange) {
            // we need to delay this: otherwise macOS crashes on theme changes
            QMetaObject::invokeMethod(this, &DesktopApplication::setDesktopIconTheme,
                                      Qt::QueuedConnection);
        }
        return false;
    });
    connect(Config::inst(), &Config::uiThemeChanged, this, &DesktopApplication::setUiTheme);
    setUiTheme();
    setDesktopIconTheme();

    m_defaultFontSize = QGuiApplication::font().pointSizeF();
    QCoreApplication::instance()->setProperty("_bs_defaultFontSize", m_defaultFontSize); // the settings dialog needs this

    auto setFontSizePercentLambda = [this](int p) {
        QFont f = QApplication::font();
        f.setPointSizeF(qreal(m_defaultFontSize * qBound(50, p, 200) / 100.));
        QApplication::setFont(f);
    };
    connect(Config::inst(), &Config::fontSizePercentChanged, this, setFontSizePercentLambda);
    int fsp = Config::inst()->fontSizePercent();
    if (fsp != 100)
        setFontSizePercentLambda(fsp);

    if (!m_startupErrors.isEmpty()) {
        QCoro::waitFor(UIHelpers::critical(m_startupErrors.join("\n\n"_l1)));

        // we cannot call quit directly, since there is no event loop to quit from...
        QMetaObject::invokeMethod(this, &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }

    // tranform . or , into the local decimal separator in all QDoubleSpinBoxes and all
    // QLineEdits with a QDoubleValidator set
    DotCommaFilter::install();

    ScriptManager::inst()->initialize(m_engine);

    MainWindow::inst()->show();

#if defined(Q_OS_MACOS)
    MainWindow::inst()->raise();

#elif defined(Q_OS_WINDOWS)
    RegisterApplicationRestart(nullptr, 0); // make us restart-able by installers

#endif

#if defined(SENTRY_ENABLED)
    if (Config::inst()->sentryConsent() == Config::SentryConsent::Unknown) {
        QString text = tr("Enable anonymous crash reporting?<br><br>Please consider enabling this feature when available.<br>If you have any doubts about what information is being submitted and how it is used, please <a href='https://github.com/rgriebl/brickstore/wiki/Crash-Reporting'>see here</a>.<br><br>Crash reporting can be enabled or disabled at any time in the Settings dialog.");

        switch (QMessageBox::question(MainWindow::inst(), QCoreApplication::applicationName(), text)) {
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

DesktopApplication::~DesktopApplication()
{
    delete ScriptManager::inst();
}

void DesktopApplication::checkRestart()
{
    if (m_restart)
        QProcess::startDetached(qApp->applicationFilePath(), { });
}

DeveloperConsole *DesktopApplication::developerConsole()
{
    if (!m_devConsole) {
        m_devConsole = new DeveloperConsole();
        connect(m_devConsole, &DeveloperConsole::execute,
                this, [](const QString &command, bool *successful) {
            *successful = ScriptManager::inst()->executeString(command);
        });
        if (!m_loggingTimer.isActive())
            m_loggingTimer.start();
    }
    return m_devConsole;
}

void DesktopApplication::showToastMessage(const QString &message, int timeout)
{
    auto *tm = new ToastMessage(message, timeout);
    m_toastMessages.push_back(tm);
    processToastMessages();
}

void DesktopApplication::processToastMessages()
{
    if (m_currentToastMessage || m_toastMessages.isEmpty())
        return;

    auto *activeWindow = qApp->activeWindow();
    if ((qApp->applicationState() != Qt::ApplicationActive) || !activeWindow) {
        qDeleteAll(m_toastMessages);
        m_toastMessages.clear();
        return;
    }

    m_currentToastMessage.reset(m_toastMessages.takeFirst());
    m_currentToastMessage->open(activeWindow->frameGeometry());
    connect(m_currentToastMessage.get(), &ToastMessage::closed,
            this, [this]() {
        m_currentToastMessage.release();
        processToastMessages();
    }, Qt::QueuedConnection);
}

void DesktopApplication::setupLogging()
{
    Application::setupLogging();

    setUILoggingHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
        auto *that = inst();
        if (!that)
            return;

        try {
            // we may not be in the main thread here, but even if we are, we could be recursing

            auto ctxCopy = new QMessageLogContext(qstrdup(ctx.file), ctx.line,
                                                  qstrdup(ctx.function), qstrdup(ctx.category));
            QMutexLocker locker(&that->m_loggingMutex);
            that->m_loggingMessages.append({ type, ctxCopy, msg });
            locker.unlock();

            if (that->m_devConsole) {
                // can't start a timer from another thread
                QMetaObject::invokeMethod(that, []() {
                    if (!inst()->m_loggingTimer.isActive())
                        inst()->m_loggingTimer.start();
                }, Qt::QueuedConnection);
            }
        } catch (const std::bad_alloc &) {
            // swallow bad-allocs and hope for sentry to log something useful
        }
    });

    m_loggingTimer.setInterval(100);
    m_loggingTimer.setSingleShot(true);

    connect(&m_loggingTimer, &QTimer::timeout, this, [this]() {
        if (!m_devConsole)
            return;

        QMutexLocker locker(&m_loggingMutex);
        if (m_loggingMessages.isEmpty())
            return;
        auto messages = m_loggingMessages.mid(0, 100); // don't overload the UI
        m_loggingMessages.remove(0, messages.size());
        bool restartTimer = !m_loggingMessages.isEmpty();
        locker.unlock();

        for (const auto &t : messages) {
            auto *ctx = std::get<1>(t);

            m_devConsole->messageHandler(std::get<0>(t), *ctx, std::get<2>(t));

            delete [] ctx->category;
            delete [] ctx->file;
            delete [] ctx->function;
            delete ctx;
        }
        if (restartTimer)
            m_loggingTimer.start();
    });
}

QCoro::Task<bool> DesktopApplication::closeAllViews()
{
    return MainWindow::inst()->closeAllViews();
}

bool DesktopApplication::notifyOtherInstance()
{
    // We need a long timeout here. If multiple docs are opened at the same time from the Windows
    // Explorer, it will launch multiple BrickStore processes in parallel (one for each document).
    // With a short timeout, the clients may not get back the confirmation ('X') in time.
    const int timeout = 10000;
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
                    this, [this, server]() {
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
                        client->waitForReadyRead(timeout);
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
                client->close();

                for (const auto &f : qAsConst(files))
                    QCoreApplication::postEvent(qApp, new QFileOpenEvent(f), Qt::LowEventPriority);

                raise();
            });
            state = Server;
        }
    }
    if (state != Server) {
        QLocalSocket client(this);

        for (int i = 0; i < 2; ++i) { // try twice
            client.connectToServer(socketName);
            if (client.waitForConnected(timeout) || i)
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
            res = res && client.waitForBytesWritten(timeout);
//            res = res && client.waitForReadyRead(timeout);
//            res = res && (client.read(1) == QByteArray(1, 'X'));

            if (res) {
                delete server;
                return true;
            }
        }
    }
    return false;
}

void DesktopApplication::setUiTheme()
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
            QMessageBox::information(MainWindow::inst(), QCoreApplication::applicationName(),
                                     tr("Your macOS version is too old to support theme changes."));
    } else if (!startup || (theme != Config::UiTheme::SystemDefault)) {
        if (startup)
            setCurrentMacTheme(theme);
        else
            QMetaObject::invokeMethod(qApp, [=]() { setCurrentMacTheme(theme); }, Qt::QueuedConnection);
    }
    if (startup)
        QApplication::setStyle(new BrickStoreProxyStyle());

#else
    // on Windows and Linux, we have to instantiate a Fusion style with a matching palette for
    // the Light and Dark styles. Switching on the fly is hard because of possible QProxyStyle
    // instances, so we better restart the app.

    if (!startup) {
        QMessageBox mb(QMessageBox::Question, QCoreApplication::applicationName(),
                       tr("The theme change will take effect after a restart."),
                       QMessageBox::NoButton, MainWindow::inst());
        mb.setDefaultButton(mb.addButton(tr("Later"), QMessageBox::NoRole));
        auto restart = mb.addButton(tr("Restart now"), QMessageBox::YesRole);
        mb.setTextFormat(Qt::RichText);
        mb.exec();

        if (mb.clickedButton() == restart) {
            m_restart = true;
            QCoreApplication::quit();
        }
        return;
    }

    if (theme != Config::UiTheme::SystemDefault) {
        QApplication::setStyle(new BrickStoreProxyStyle(QStyleFactory::create("fusion"_l1)));
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
    } else {
        QApplication::setStyle(new BrickStoreProxyStyle());
    }
#endif // !Q_OS_MACOS
}

void DesktopApplication::setDesktopIconTheme()
{
    const auto pal = QGuiApplication::palette();
    auto winColor = pal.color(QPalette::Active, QPalette::Window);
    bool dark = ((winColor.lightnessF() * winColor.alphaF()) < 0.5f);

    setIconTheme(dark ? DarkTheme : LightTheme);
}

#include "moc_desktopapplication.cpp"
#include "desktopapplication.moc"
