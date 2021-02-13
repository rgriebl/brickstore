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

    LDraw::Part *part = nullptr;

    if (LDraw::core())
        part = LDraw::core()->partFromId("3833");

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
    QString layoutTop = QLatin1String(
        "<strong style=\"font-size: x-large\">%1</strong><br>"
        "<strong style=\"font-size: large\">%3</strong><br>"
        "<span style=\"font-size: large\">%2</strong><br>"
        "<br>%4</td>");
    QString layoutBottom = QLatin1String(
        "<center>"
        "<br><big>%5</big>"
        "</center>%6<p>%7</p>");


    QString page1_link = QLatin1String("<strong>%1</strong> | <a href=\"system\">%2</a>");
    page1_link = page1_link.arg(tr("Legal Info"), tr("System Info"));
    QString page2_link = QLatin1String("<a href=\"index\">%1</a> | <strong>%2</strong>");
    page2_link = page2_link.arg(tr("Legal Info"), tr("System Info"));

    QString copyright = tr("Copyright &copy; %1").arg(BRICKSTORE_COPYRIGHT);
    QString version   = tr("Version %1 (build: %2)").arg(BRICKSTORE_VERSION).arg(BRICKSTORE_BUILD_NUMBER);
    QString support   = tr("Visit %1").arg("<a href=\"https://" BRICKSTORE_URL "\">" BRICKSTORE_URL "</a>");

    QString qt = QLibraryInfo::version().toString();
    if (QLibraryInfo::isDebugBuild())
        qt += QLatin1String(" (debug build)");

    QString translators = QLatin1String("<b>") + tr("Translators") + QLatin1String("</b><table border=\"0\">");

    QString translators_html = QLatin1String(R"(<tr><td>%1</td><td width="2em"></td><td>%2 &lt;<a href="mailto:%3">%4</a>&gt;</td></tr>)");
    const auto translations = Config::inst()->translations();
    for (const Config::Translation &trans : translations) {
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
        "<table>"
        "<th colspan=\"2\" align=\"left\">Build Info</th>"
        "<tr><td>Git version   </td><td>%1</td></tr>"
        "<tr><td>User          </td><td>%2</td></tr>"
        "<tr><td>Host          </td><td>%3</td></tr>"
        "<tr><td>Date          </td><td>%4</td></tr>"
        "<tr><td>Architecture  </td><td>%5</td></tr>"
        "<tr><td>Compiler      </td><td>%6</td></tr>"
        "</table><br>"
        "<table>"
        "<th colspan=\"2\" align=\"left\">Runtime Info</th>"
        "<tr><td>OS            </td><td>%7</td></tr>"
        "<tr><td>Architecture  </td><td>%8</td></tr>"
        "<tr><td>Memory        </td><td>%L9 MB</td></tr>"
        "<tr><td>Qt            </td><td>%10</td></tr>"
        "<tr><td>LDraw         </td><td>%11</td></tr>"
        "</table>");

    technical = technical.arg(BRICKSTORE_GIT_VERSION, BRICKSTORE_BUILD_USER, BRICKSTORE_BUILD_HOST, __DATE__ " " __TIME__)
            .arg(QSysInfo::buildCpuArchitecture()).arg(BRICKSTORE_COMPILER_VERSION)
            .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture())
            .arg(Utility::physicalMemory()/(1024ULL*1024ULL)).arg(qt)
            .arg(ldrawInstallation);

    QString header = layoutTop.arg(QCoreApplication::applicationName(), copyright, version, support);

    QString page1 = layoutBottom.arg(page1_link, legal, translators);
    QString page2 = layoutBottom.arg(page2_link, technical, QString());

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
        // w_browser->document()->setTextWidth(500);
        //w_browser->document()->adjustSize();
//  w_browser->setFixedSize(w_browser->document()->size().toSize());
        layout()->activate();
    }
    else {
        QDesktopServices::openUrl(url);
    }
}

#include "moc_aboutdialog.cpp"
