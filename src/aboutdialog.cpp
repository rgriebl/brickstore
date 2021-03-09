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
    const QString layoutTop = qL1S(
        R"(<strong style="font-size: x-large">%1</strong><br>)"
        R"(<strong style="font-size: large">%3</strong><br>)"
        R"(<span style="font-size: large">%2</strong><br>)"
        R"(<br>%4)");
    const QString layoutBottom = qL1S(
        R"(<center><br><big>%5</big></center>%6<p>%7</p>)");

    const QString page1LinkFmt = qL1S(R"(<strong>%1</strong> | <a href="system">%2</a>)");
    const QString page1_link = page1LinkFmt.arg(tr("Legal Info"), tr("System Info"));
    const QString page2LinkFmt = qL1S(R"(<a href="index">%1</a> | <strong>%2</strong>)");
    const QString page2_link = page2LinkFmt.arg(tr("Legal Info"), tr("System Info"));

    const QString copyright = tr("Copyright &copy; %1").arg(BRICKSTORE_COPYRIGHT);
    const QString version   = tr("Version %1 (build: %2)").arg(BRICKSTORE_VERSION).arg(BRICKSTORE_BUILD_NUMBER);
    const QString support   = tr("Visit %1").arg(R"(<a href="https://)" BRICKSTORE_URL R"(">)" BRICKSTORE_URL R"(</a>)");

    QString qt = QLibraryInfo::version().toString();
    if (QLibraryInfo::isDebugBuild())
        qt += qL1S(R"( (debug build))");

    QString translators = qL1S(R"(<b>)") + tr("Translators") + qL1S(R"(</b><table border="0">)");

    const QString translators_html = qL1S(R"(<tr><td>%1</td><td width="2em"></td><td>%2 &lt;<a href="mailto:%3">%4</a>&gt;</td></tr>)");
    const auto translations = Config::inst()->translations();
    for (const Config::Translation &trans : translations) {
        if (trans.language != qL1S("en")) {
            QString langname = trans.languageName.value(Config::inst()->language(),
                                                        trans.languageName[qL1S("en")]);
            translators += translators_html.arg(langname, trans.author, trans.authorEMail, trans.authorEMail);
        }
    }

    translators += qL1S(R"(</table>)");

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

    const QString technicalFmt = qL1S(
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
        R"(</table>)");

    const QString technical = technicalFmt
            .arg(BRICKSTORE_GIT_VERSION, BRICKSTORE_BUILD_USER, BRICKSTORE_BUILD_HOST, __DATE__ " " __TIME__)
            .arg(QSysInfo::buildCpuArchitecture()).arg(BRICKSTORE_COMPILER_VERSION)
            .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), cpu)
            .arg(Utility::physicalMemory()/(1024ULL*1024ULL)).arg(qt)
            .arg(ldrawInstallation);

    const QString header = layoutTop.arg(QCoreApplication::applicationName(), copyright, version, support);

    const QString page1 = layoutBottom.arg(page1_link, legal, translators);
    const QString page2 = layoutBottom.arg(page2_link, technical, QString());

    m_pages ["index"]  = page1;
    m_pages ["system"] = page2;

    w_header->setText(header);

    gotoPage("index");
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
    p.start(R"(sh)", { R"(-c)", R"(grep -m 1 '^model name' /proc/cpuinfo | sed -e 's/^.*: //g')" },
            QIODevice::ReadOnly);
    p.waitForFinished(1000);
    return QString::fromUtf8(p.readAllStandardOutput()).simplified();

#elif defined(Q_OS_WIN)
    QProcess p;
    p.start(R"(wmic)", { R"(/locale:ms_409)", R"(cpu)", R"(get)", R"(name)", R"(/value)" },
            QIODevice::ReadOnly);
    p.waitForFinished(1000);
    return QString::fromUtf8(p.readAllStandardOutput()).simplified().mid(5);

#elif defined(Q_OS_MACOS)
    QProcess p;
    p.start(R"(sysctl)", { R"(-n)", R"(machdep.cpu.brand_string)" }, QIODevice::ReadOnly);
    p.waitForFinished(1000);
    return QString::fromUtf8(p.readAllStandardOutput()).simplified();

#else
    return qL1S("?");
#endif
}

#include "moc_aboutdialog.cpp"
