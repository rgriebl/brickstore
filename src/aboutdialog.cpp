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
#include <QPushButton>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QLibraryInfo>
#include <QProcess>

#include "aboutdialog.h"
#include "ldraw.h"
#include "ldraw/renderwidget.h"
#include "version.h"
#include "config.h"
#include "utility.h"


AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    int iconSize = fontMetrics().height() * 7;
    w_icon->setFixedSize(iconSize, iconSize);

    QString cpu = cpuModel();

    LDraw::Part *part = LDraw::core() ? LDraw::core()->partFromId("3833") : nullptr;

    if (part) {
        auto ldraw_icon = new LDraw::RenderWidget();
        ldraw_icon->setPartAndColor(part, 321);
        ldraw_icon->startAnimation();
        auto layout = new QHBoxLayout(w_icon);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(ldraw_icon);
        w_icon->setPixmap(QPixmap());
    }

    QString ldrawInstallation = part ? LDraw::core()->dataPath()
                                     : tr("No LDraw installation was found.");
    const QString layoutTop = \
        R"(<strong style="font-size: x-large">%1</strong><br>)"
        R"(<strong style="font-size: large">%3</strong><br>)"
        R"(<span style="font-size: large">%2</strong><br>)"
        R"(<br>%4)"_l1;
    const QString layoutBottom = R"(<center><br><big>%5</big></center>%6<p>%7</p>)"_l1;

    const QString page1LinkFmt = R"(<strong>%1</strong> | <a href="system">%2</a>)"_l1;
    const QString page1_link = page1LinkFmt.arg(tr("Legal Info"), tr("System Info"));
    const QString page2LinkFmt = R"(<a href="index">%1</a> | <strong>%2</strong>)"_l1;
    const QString page2_link = page2LinkFmt.arg(tr("Legal Info"), tr("System Info"));

    const QString copyright = tr("Copyright &copy; %1").arg(QLatin1String(BRICKSTORE_COPYRIGHT));
    const QString version   = tr("Version %1 (build: %2)").arg(QLatin1String(BRICKSTORE_VERSION),
                                                               QLatin1String(BRICKSTORE_BUILD_NUMBER));
    const QString support   = tr("Visit %1").arg(R"(<a href="https://)" BRICKSTORE_URL
                                                 R"(">)" BRICKSTORE_URL R"(</a>)"_l1);

    QString qt = QLibraryInfo::version().toString();
    if (QLibraryInfo::isDebugBuild())
        qt += R"( (debug build))"_l1;

    QString translators = R"(<b>)"_l1 + tr("Translators") + R"(</b><table border="0">)"_l1;

    const QString translators_html = R"(<tr><td>%1</td><td width="2em"></td><td>%2 &lt;<a href="mailto:%3">%4</a>&gt;</td></tr>)"_l1;
    const auto translations = Config::inst()->translations();
    for (const Config::Translation &trans : translations) {
        if (trans.language != "en"_l1) {
            QString langname = trans.languageName.value(Config::inst()->language(),
                                                        trans.languageName["en"_l1]);
            translators += translators_html.arg(langname, trans.author, trans.authorEMail, trans.authorEMail);
        }
    }

    translators += R"(</table>)"_l1;

    const QString legal = tr(
        R"(<p>)"
        R"(This program is free software; it may be distributed and/or modified )"
        R"(under the terms of the GNU General Public License version 2 as published )"
        R"(by the Free Software Foundation and appearing in the file LICENSE.GPL )"
        R"(included in this software package.)"
        R"(<br>)"
        R"(This program is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE )"
        R"(WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.)"
        R"(<br>)"
        R"(See <a href="https://www.gnu.org/licenses/old-licenses/gpl-2.0.html">www.gnu.org/licenses/old-licenses/gpl-2.0.html</a> for GPL licensing information.)"
        R"(</p><p>)"
        R"(All data from <a href="https://www.bricklink.com">www.bricklink.com</a> is owned by BrickLink. )"
        R"(Both BrickLink and LEGO are trademarks of the LEGO group, which does not sponsor, )"
        R"(authorize or endorse this software. All other trademarks recognized.)"
        R"(</p><p>)"
        R"(Only made possible by <a href="https://www.danjezek.com/">Dan Jezek's</a> support.)"
        R"(</p>)");

    const QString technicalFmt =
        R"(<table>)"
        R"(<th colspan="2" align="left">Build Info</th>)"
        R"(<tr><td>Git version   </td><td>%1</td></tr>)"
        R"(<tr><td>User          </td><td>%2</td></tr>)"
        R"(<tr><td>Host          </td><td>%3</td></tr>)"
        R"(<tr><td>Date          </td><td>%4</td></tr>)"
        R"(<tr><td>Architecture  </td><td>%5</td></tr>)"
        R"(<tr><td>Compiler      </td><td>%6</td></tr>)"
        R"(</table><br>)"
        R"(<table>)"
        R"(<th colspan="2" align="left">Runtime Info</th>)"
        R"(<tr><td>OS            </td><td>%7</td></tr>)"
        R"(<tr><td>Architecture  </td><td>%8</td></tr>)"
        R"(<tr><td>CPU           </td><td>%9</td></tr>)"
        R"(<tr><td>Memory        </td><td>%L10 MB</td></tr>)"
        R"(<tr><td>Qt            </td><td>%11</td></tr>)"
        R"(<tr><td>LDraw         </td><td>%12</td></tr>)"
        R"(</table>)"_l1;

    const QString technical = technicalFmt
            .arg(QLatin1String(BRICKSTORE_GIT_VERSION), QLatin1String(BRICKSTORE_BUILD_USER),
                 QLatin1String(BRICKSTORE_BUILD_HOST), QLatin1String(__DATE__ " " __TIME__))
            .arg(QSysInfo::buildCpuArchitecture()).arg(QLatin1String(BRICKSTORE_COMPILER_VERSION))
            .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), cpu)
            .arg(Utility::physicalMemory()/(1024ULL*1024ULL)).arg(qt)
            .arg(ldrawInstallation);

    const QString header = layoutTop.arg(QCoreApplication::applicationName(), copyright, version, support);

    const QString page1 = layoutBottom.arg(page1_link, legal, translators);
    const QString page2 = layoutBottom.arg(page2_link, technical, QString());

    m_pages["index"_l1]  = page1;
    m_pages["system"_l1] = page2;

    w_header->setText(header);

    gotoPage("index"_l1);
    connect(w_browser, &QLabel::linkActivated,
            this, &AboutDialog::gotoPage);

    setFixedSize(sizeHint());
}

void AboutDialog::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi(this);
    QDialog::changeEvent(e);
}

void AboutDialog::enableOk()
{
    w_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void AboutDialog::reject()
{
    if (w_buttons->button(QDialogButtonBox::Ok)->isEnabled())
        QDialog::reject();
}

void AboutDialog::closeEvent(QCloseEvent *ce)
{
    ce->setAccepted(w_buttons->button(QDialogButtonBox::Ok)->isEnabled());
}

void AboutDialog::gotoPage(const QString &url)
{
    if (m_pages.contains(url)) {
        w_browser->setText(m_pages [url]);
        layout()->activate();
    }
    else {
        QDesktopServices::openUrl(url);
    }
}

QString AboutDialog::cpuModel()
{
#if defined(Q_OS_LINUX)
    QProcess p;
    p.start("sh"_l1, { "-c"_l1, R"(grep -m 1 '^model name' /proc/cpuinfo | sed -e 's/^.*: //g')"_l1 },
            QIODevice::ReadOnly);
    p.waitForFinished(1000);
    return QString::fromUtf8(p.readAllStandardOutput()).simplified();

#elif defined(Q_OS_WIN)
    QProcess p;
    p.start("wmic"_l1, { "/locale:ms_409"_l1, "cpu"_l1, "get"_l1, "name"_l1, "/value"_l1 },
            QIODevice::ReadOnly);
    p.waitForFinished(1000);
    return QString::fromUtf8(p.readAllStandardOutput()).simplified().mid(5);

#elif defined(Q_OS_MACOS)
    QProcess p;
    p.start("sysctl"_l1, { "-n"_l1, "machdep.cpu.brand_string"_l1 }, QIODevice::ReadOnly);
    p.waitForFinished(1000);
    return QString::fromUtf8(p.readAllStandardOutput()).simplified();

#else
    return "?"_l1;
#endif
}

#include "moc_aboutdialog.cpp"
