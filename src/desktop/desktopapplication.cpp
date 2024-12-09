// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <cstdio>
#include <QtCore/QThread>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QCommandLineParser>
#include <QtCore/QProcess>
#include <QtCore/QMessageLogContext>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
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
#    pragma warning(push)
#    pragma warning(disable: 4458)
#    pragma warning(disable: 4201)
#  endif
#  include <QtGui/private/qguiapplication_p.h>
#  if defined(Q_CC_MSVC)
#    pragma warning(pop)
#  endif
#  include <QStyleHints>
#elif defined(Q_OS_MACOS)
#  include <QtCore/QVersionNumber>
#  include <QtCore/QSysInfo>
#  include <QtGui/private/qguiapplication_p.h>
#  include <QtGui/private/qshortcutmap_p.h>
#  include "common/systeminfo.h"
#endif

#include "qtsingleapplication/qtlocalpeer.h"
#include "common/config.h"
#include "common/scriptmanager.h"
#include "desktop/brickstoreproxystyle.h"
#include "desktop/desktopuihelpers.h"
#include "desktop/developerconsole.h"
#include "desktop/mainwindow.h"
#include "desktop/smartvalidator.h"

#include "desktopapplication.h"


DesktopApplication::DesktopApplication(int &argc, char **argv)
    : Application(argc, argv)
{
#if defined(Q_OS_LINUX)
    qputenv("QT_WAYLAND_DECORATION", "adwaita");
#endif
#if defined(Q_OS_WINDOWS)
    // the Vista style scales very badly when scaled to non-integer factors
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif

    m_app = new QApplication(argc, argv);

    m_clp.addHelpOption();
    m_clp.addOption({ { u"v"_qs, u"version"_qs }, u"Display version information."_qs });
    m_clp.addOption({ u"load-translation"_qs, u"Load the specified translation (testing only)."_qs, u"qm-file"_qs });
    m_clp.addOption({ u"new-instance"_qs, u"Start a new instance."_qs });
    m_clp.addPositionalArgument(u"files"_qs, u"The BSX documents to open, optionally."_qs, u"[files...]"_qs);
    m_clp.process(QCoreApplication::arguments());

    m_translationOverride = m_clp.value(u"load-translation"_qs);
    const auto documents = m_clp.positionalArguments();
    for (const auto &document : documents)
        m_queuedDocuments << QUrl::fromLocalFile(document);

    if (m_clp.isSet(u"version"_qs)) {
        QString s = QCoreApplication::applicationName() + u' '  +
                QCoreApplication::applicationVersion() + u" (build: " + buildNumber() + u')';
        puts(s.toLocal8Bit());
        exit(0);
    }

    // check for an already running instance
    if (!m_clp.isSet(u"new-instance"_qs) && notifyOtherInstance())
        exit(0);

#if defined(Q_OS_LINUX)
    QPixmap pix(u":/assets/generated-app-icons/brickstore.png"_qs);
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
    new EventFilter(qApp, { QEvent::ShortcutOverride }, [](QObject *, QEvent *e) {
        auto &scm = QGuiApplicationPrivate::instance()->shortcutMap;
        if (scm.state() == QKeySequence::PartialMatch) {
            e->accept();
            return EventFilter::StopEventProcessing;
        }
        return EventFilter::ContinueEventProcessing;
    });
#endif

    new EventFilter(qApp, { QEvent::ApplicationPaletteChange }, [this](QObject *, QEvent *) {
        // we need to delay this: otherwise macOS crashes on theme changes
        QMetaObject::invokeMethod(this, &DesktopApplication::setDesktopIconTheme,
                                  Qt::QueuedConnection);
        return EventFilter::ContinueEventProcessing;
    });
    connect(Config::inst(), &Config::uiThemeChanged, this, &DesktopApplication::setUITheme);
    setUITheme();
    setDesktopIconTheme();

    m_defaultFontSize = QGuiApplication::font().pointSizeF();
    QCoreApplication::instance()->setProperty("_bs_defaultFontSize", m_defaultFontSize); // the settings dialog needs this

    auto setFontSizePercentLambda = [this](int p) {
        QFont f = QApplication::font();
        f.setPointSizeF(qreal(m_defaultFontSize * std::clamp(p, 50, 200) / 100.));
        QApplication::setFont(f);
    };
    connect(Config::inst(), &Config::fontSizePercentChanged, this, setFontSizePercentLambda);
    int fsp = Config::inst()->fontSizePercent();
    if (fsp != 100)
        setFontSizePercentLambda(fsp);

    // transform . or , into the local decimal separator in all QDoubleSpinBoxes and all
    // QLineEdits with a QDoubleValidator set
    DotCommaFilter::install();

    new EventFilter(qApp, { QEvent::MouseButtonPress, QEvent::MouseButtonRelease },
                    [](QObject *o, QEvent *e) -> EventFilter::Result {
        const auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::ForwardButton || me->button() == Qt::BackButton) {
            QKeyEvent ke(me->type() == QEvent::MouseButtonPress ? QEvent::KeyPress : QEvent::KeyRelease,
                         me->button() == Qt::ForwardButton ? Qt::Key_Forward : Qt::Key_Back,
                         Qt::NoModifier);
            QCoreApplication::sendEvent(o, &ke);
            return EventFilter::StopEventProcessing;
        }
        return EventFilter::ContinueEventProcessing;
    });

    ScriptManager::create(m_engine);

    MainWindow::inst()->show();

    connect(MainWindow::inst(), &MainWindow::shown,
            this, [this]() {
        // we need to delay this, because Quick doesn't like changes to transientParents
        setMainWindow(MainWindow::inst()->windowHandle());
        ScriptManager::inst()->reload();
    });

#if defined(Q_OS_WINDOWS)
    RegisterApplicationRestart(nullptr, 0); // make us restart-able by installers

#endif

#if defined(SENTRY_ENABLED)
    if (Config::inst()->sentryConsent() == Config::SentryConsent::Unknown) {
        QString text = tr("Enable anonymous crash reporting?<br><br>Please consider enabling this feature when available.<br>If you have any doubts about what information is being submitted and how it is used, please <a href='https://www.brickstore.dev/crash-reporting'>see here</a>.<br><br>Crash reporting can be enabled or disabled at any time in the Settings dialog.");

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

#if defined(Q_OS_MACOS)
    if (SystemInfo::inst()->value(u"build.qt.version"_qs).toString() == u"6.4.3") {
        if (!QSysInfo::productVersion().startsWith(u"10.")) {
            QString text = tr("You are using the legacy version of BrickStore for old macOS 10 "
                              "machines, but you are running macOS %1.")
                               .arg(QSysInfo::productVersion())
                           + u"<br><br>"
                           + tr("Please download <a href='https://%1/releases/latest'>the normal "
                                "macOS version</a> and replace the current installation to avoid problems.")
                                 .arg(gitHubUrl());
            QMessageBox::critical(MainWindow::inst(), QCoreApplication::applicationName(), text);
        }
    }
#endif
}

DesktopApplication::~DesktopApplication()
{
    delete ScriptManager::inst();
    delete MainWindow::inst();
}

void DesktopApplication::checkRestart()
{
#if QT_CONFIG(process)
    if (m_restart)
        QProcess::startDetached(qApp->applicationFilePath(), { });
#endif
}

DeveloperConsole *DesktopApplication::developerConsole()
{
    if (!m_devConsole) {
        m_devConsole = new DeveloperConsole(u"\u2771\u2771\u2771 "_qs, [](QString command) {
            return ScriptManager::inst()->executeString(command);
        });
        if (!m_loggingTimer.isActive())
            m_loggingTimer.start();
    }
    return m_devConsole;
}

void DesktopApplication::setupLogging()
{
    Application::setupLogging();

    setUILoggingHandler([](const UILogMessage &lm) {
        if (auto *that = inst()) {
            if (auto devConsole = that->developerConsole())
                devConsole->appendLogMessage(std::get<0>(lm), std::get<1>(lm), std::get<2>(lm),
                                             std::get<3>(lm), std::get<4>(lm));
        }
    });
}

QCoro::Task<bool> DesktopApplication::closeAllDocuments()
{
    auto result = co_await Application::closeAllDocuments();

    MainWindow::inst()->closeAllDialogs();
    co_return result;
}

bool DesktopApplication::notifyOtherInstance()
{
    auto peer = new QtLocalPeer(this, QCoreApplication::applicationName());
    connect(peer, &QtLocalPeer::messageReceived, this,
            [this](const QString &message) {
        const auto files = message.split(QChar::Null);
        for (const auto &file : std::as_const(files))
            QCoreApplication::postEvent(qApp, new QFileOpenEvent(file), Qt::LowEventPriority);

        raise();
    });

    if (peer->isClient()) {
        const QStringList args = QCoreApplication::arguments().mid(1);
        QStringList files;
        files.reserve(args.size());
        for (const auto &arg : args) {
            QFileInfo fi(arg);
            if (fi.isFile())
                files << fi.absoluteFilePath();
        }
        return peer->sendMessage(files.join(QChar::Null), 5000);
    }
    return false;
}

void DesktopApplication::setUITheme()
{
    static bool once = false;
    bool startup = !once;
    if (!once)
        once = true;
    auto theme = Config::inst()->uiTheme();

#if defined(Q_OS_MACOS)
    extern bool hasMacThemes();
    extern void setCurrentMacTheme(Config::UITheme);

    // on macOS, we are using the native theme switch
    if (!hasMacThemes()) {
        if (!startup)
            QMessageBox::information(MainWindow::inst(), QCoreApplication::applicationName(),
                                     tr("Your macOS version is too old to support theme changes."));
    } else if (!startup || (theme != Config::UITheme::SystemDefault)) {
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

#if defined(Q_OS_WINDOWS)
    auto qwa = qApp->nativeInterface<QNativeInterface::Private::QWindowsApplication>();
#  if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    bool isDarkMode = qwa && qwa->isDarkMode();
#  else
    bool isDarkMode = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#  endif

    if (qwa && isDarkMode) {
        // let Qt handle the window frames, but we handle the style ourselves
        qwa->setDarkModeHandling(QNativeInterface::Private::QWindowsApplication::DarkModeWindowFrames);
    }

    if ((theme == Config::UITheme::SystemDefault) && isDarkMode) {
        // Qt's Windows Vista style only supports light mode, so we have to fake dark mode
        theme = Config::UITheme::Dark;
    } else if (theme == Config::UITheme::Light) {
        // no need to use Fusion tricks here, just use the default Vista style
        theme = Config::UITheme::SystemDefault;
    }
    if (theme == Config::UITheme::Dark) {
        // prevent Win11 palette updates when the system theme or screen settings change
        QGuiApplication::setDesktopSettingsAware(false);
    }
#endif

    if (theme != Config::UITheme::SystemDefault) {
        QApplication::setStyle(new BrickStoreProxyStyle(QStyleFactory::create(u"fusion"_qs)));
        auto palette = QApplication::style()->standardPalette();

        if (theme == Config::UITheme::Dark) {
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
            palette.setColor(QPalette::PlaceholderText, QColor(127, 127, 127));
            palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
        }

        QApplication::setPalette(palette);
        QToolTip::setPalette(palette);
    } else if (QSysInfo::productVersion().toInt() >= 11) {
        // The new Windows 11 style still has problems as of 6.8.1
        QGuiApplication::setDesktopSettingsAware(false);
        QApplication::setStyle(new BrickStoreProxyStyle(QStyleFactory::create(u"windowsvista"_qs)));
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
