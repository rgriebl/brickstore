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

#include <cfloat>

#include <QDockWidget>
#include <QMainWindow>
#include <QEvent>
#include <QDesktopServices>
#include <QStatusTipEvent>
#include <QApplication>

#include "bricklink.h"
#include "utility.h"
#include "config.h"
#include "framework.h"

#include "taskwidgets.h"


TaskLinksWidget::TaskLinksWidget(QWidget *parent)
        : QLabel(parent), m_win(0)
{
    connect(FrameWork::inst(), SIGNAL(windowActivated(Window *)), this, SLOT(windowUpdate(Window *)));

    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setIndent(8);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    setText("<b>ABCDEFGHIJKLM</b><br />1<br />2<br />3<br />4<br /><br /><b>X</b><br />1<br />");
    setMinimumSize(minimumSizeHint());
    setText(QString());

    connect(this, SIGNAL(linkActivated(const QString &)), this, SLOT(linkActivate(const QString &)));
    connect(this, SIGNAL(linkHovered(const QString &)), this, SLOT(linkHover(const QString &)));
}

void TaskLinksWidget::linkActivate(const QString &url)
{
    QDesktopServices::openUrl(url);
}

void TaskLinksWidget::linkHover(const QString &url)
{
    if (parent()) {
        QStatusTipEvent tip(url);
        QApplication::sendEvent(parent(), &tip);
    }
//    setStatusTip(url);
  //  if (QMainWindow *tl = qobject_cast<QMainWindow *>(topLevelWidget())) {
  //      tl->
  //  }
}

void TaskLinksWidget::windowUpdate(Window *win)
{
    if (m_win)
        disconnect(m_win, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));
    m_win = win;
    if (m_win)
        connect(m_win, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));

    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskLinksWidget::selectionUpdate(const Document::ItemList &list)
{
    QString str;

    if (m_win && (list.count() == 1)) {
        const BrickLink::Item *item   = list.front()->item();
        const BrickLink::Color *color = list.front()->color();

        if (item && color) {
            QString fmt1 = "&nbsp;&nbsp;<b>%1</b><br />";
            QString fmt2 = "&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"%2\">%1...</a><br />";

            str += fmt1.arg(tr("BrickLink"));
            if (list.front()->lotId()) {
                uint lotid = list.front()->lotId();
                str += fmt2.arg(tr("My Inventory")).arg(BrickLink::core()->url(BrickLink::URL_StoreItemDetail, &lotid).toString());
            }

            str += fmt2.arg(tr("Catalog")).       arg(BrickLink::core()->url(BrickLink::URL_CatalogInfo,    item, color).toString());
            str += fmt2.arg(tr("Price Guide")).   arg(BrickLink::core()->url(BrickLink::URL_PriceGuideInfo, item, color).toString());
            str += fmt2.arg(tr("Lots for Sale")). arg(BrickLink::core()->url(BrickLink::URL_LotsForSale,    item, color).toString());

            str += "<br />";

            str += fmt1.arg(tr("Peeron"));
            str += fmt2.arg(tr("Information")).arg(BrickLink::core()->url(BrickLink::URL_PeeronInfo, item, color).toString());
        }
    }
    setText(str);
}

void TaskLinksWidget::languageChange()
{
    if (m_win)
        selectionUpdate(m_win->selection());
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskPriceGuideWidget::TaskPriceGuideWidget(QWidget *parent)
        : PriceGuideWidget(parent), m_win(0), m_dock(0)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    connect(FrameWork::inst(), SIGNAL(windowActivated(Window *)), this, SLOT(windowUpdate(Window *)));
    connect(this, SIGNAL(priceDoubleClicked(double)), this, SLOT(setPrice(double)));
    fixParentDockWindow();
}

void TaskPriceGuideWidget::windowUpdate(Window *win)
{
    if (m_win) {
        disconnect(m_win, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));
        disconnect(m_win->document(), SIGNAL(currencyCodeChanged(QString)), this, SLOT(currencyUpdate(const QString &)));
    }
    m_win = win;
    if (m_win) {
        connect(m_win, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));
        connect(m_win->document(), SIGNAL(currencyCodeChanged(QString)), this, SLOT(currencyUpdate(const QString &)));
    }

    setCurrencyCode(m_win ? m_win->document()->currencyCode() : Config::inst()->defaultCurrencyCode());
    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskPriceGuideWidget::currencyUpdate(const QString &ccode)
{
    setCurrencyCode(ccode);
}

void TaskPriceGuideWidget::selectionUpdate(const Document::ItemList &list)
{
    bool ok = (m_win && (list.count() == 1));

    setPriceGuide(ok ? BrickLink::core()->priceGuide(list.front()->item(), list.front()->color(), true) : 0);
}

void TaskPriceGuideWidget::setPrice(double p)
{
    if (m_win && (m_win->selection().count() == 1)) {
        Document::Item *pos = m_win->selection().front();
        Document::Item item = *pos;

        item.setPrice(p);
        m_win->document()->changeItem(pos, item);
    }
}

bool TaskPriceGuideWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ParentChange)
        fixParentDockWindow();

    return PriceGuideWidget::event(e);
}

void TaskPriceGuideWidget::fixParentDockWindow()
{
    if (m_dock) {
        disconnect(m_dock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(dockLocationChanged(Qt::DockWidgetArea)));
        disconnect(m_dock, SIGNAL(topLevelChanged(bool)), this, SLOT(topLevelChanged(bool)));
    }

    m_dock = 0;

    for (QObject *p = parent(); p; p = p->parent()) {
        if (qobject_cast<QDockWidget *>(p)) {
            m_dock = static_cast<QDockWidget *>(p);
            break;
        }
    }

    if (m_dock) {
        connect(m_dock, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(dockLocationChanged(Qt::DockWidgetArea)));
        connect(m_dock, SIGNAL(topLevelChanged(bool)), this, SLOT(topLevelChanged(bool)));
    }
}

void TaskPriceGuideWidget::topLevelChanged(bool b)
{
    if (b)
        setLayout(PriceGuideWidget::Normal);
}

void TaskPriceGuideWidget::dockLocationChanged(Qt::DockWidgetArea area)
{
    bool horiz = (area ==  Qt::LeftDockWidgetArea) || (area == Qt::RightDockWidgetArea);
    setLayout(horiz ? PriceGuideWidget::Vertical : PriceGuideWidget::Horizontal);
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskInfoWidget::TaskInfoWidget(QWidget *parent)
        : QStackedWidget(parent), m_win(0)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_pic = new PictureWidget(this);
    m_text = new QLabel(this);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_text->setIndent(8);
    m_text->setBackgroundRole(QPalette::Base);
    m_text->setAutoFillBackground(true);

    addWidget(m_pic);
    addWidget(m_text);

    connect(FrameWork::inst(), SIGNAL(windowActivated(Window *)), this, SLOT(windowUpdate(Window *)));
    connect(Config::inst(), SIGNAL(measurementSystemChanged(QLocale::MeasurementSystem)), this, SLOT(refresh()));
}

void TaskInfoWidget::windowUpdate(Window *win)
{
    if (m_win) {
        disconnect(m_win, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));
        disconnect(m_win->document(), SIGNAL(currencyCodeChanged(QString)), this, SLOT(currencyUpdate()));
    }
    m_win = win;
    if (m_win) {
        connect(m_win, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));
        connect(m_win->document(), SIGNAL(currencyCodeChanged(QString)), this, SLOT(currencyUpdate()));
    }

    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskInfoWidget::currencyUpdate()
{
    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskInfoWidget::selectionUpdate(const Document::ItemList &list)
{
    if (!m_win || (list.count() == 0)) {
        m_pic->setPicture(0);
        setCurrentWidget(m_pic);
    }
    else if (list.count() == 1) {
        m_pic->setPicture(BrickLink::core()->picture(list.front()->item(), list.front()->color(), true));
        setCurrentWidget(m_pic);
    }
    else {
        Document::Statistics stat = m_win->document()->statistics(list);

        QString s;
        QString valstr, wgtstr;
        QString ccode = m_win->document()->currencyCode();

        if (stat.value() != stat.minValue()) {
            valstr = QString("%1 (%2 %3)").
                     arg(Currency::toString(stat.value(), ccode, Currency::LocalSymbol)).
                     arg(tr("min.")).
                     arg(Currency::toString(stat.minValue(), ccode, Currency::LocalSymbol));
        }
        else
            valstr = Currency::toString(stat.value(), ccode, Currency::LocalSymbol);

        if (stat.weight() == -DBL_MIN) {
            wgtstr = "-";
        }
        else {
            double weight = stat.weight();

            if (weight < 0) {
                weight = -weight;
                wgtstr = tr("min.") + " ";
            }

            wgtstr += Utility::weightToString(weight, Config::inst()->measurementSystem(), true, true);
        }

        s = QString("<h3>%1</h3>&nbsp;&nbsp;%2: %3<br />&nbsp;&nbsp;%4: %5<br /><br />&nbsp;&nbsp;%6: %7<br /><br />&nbsp;&nbsp;%8: %9").
            arg(tr("Multiple lots selected")).
            arg(tr("Lots")).arg(stat.lots()).
            arg(tr("Items")).arg(stat.items()).
            arg(tr("Value")).arg(valstr).
            arg(tr("Weight")).arg(wgtstr);

//  if (( stat.errors ( ) > 0 ) && Config::inst ( )->showInputErrors ( ))
//   s += QString ( "<br /><br />&nbsp;&nbsp;%1: %2" ).arg ( tr( "Errors" )).arg ( stat.errors ( ));

        m_pic->setPicture(0);
        m_text->setText(s);
        setCurrentWidget(m_text);
    }
}

void TaskInfoWidget::languageChange()
{
    refresh();
}

void TaskInfoWidget::refresh()
{
    if (m_win)
        selectionUpdate(m_win->selection());
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskAppearsInWidget::TaskAppearsInWidget(QWidget *parent)
    : AppearsInWidget(parent), m_win(0)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    connect(FrameWork::inst(), SIGNAL(windowActivated(Window *)), this, SLOT(windowUpdate(Window *)));
}

QSize TaskAppearsInWidget::minimumSizeHint() const
{
    const QFontMetrics &fm = fontMetrics();

    return QSize(fm.width('m') * 20, fm.height() * 10);
}

QSize TaskAppearsInWidget::sizeHint() const
{
    return minimumSizeHint();
}

void TaskAppearsInWidget::windowUpdate(Window *win)
{
    if (m_win)
        disconnect(m_win, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));
    m_win = win;
    if (m_win)
        connect(m_win, SIGNAL(selectionChanged(const Document::ItemList &)), this, SLOT(selectionUpdate(const Document::ItemList &)));

    selectionUpdate(m_win ? m_win->selection() : Document::ItemList());
}

void TaskAppearsInWidget::selectionUpdate(const Document::ItemList &list)
{
    if (!m_win || list.isEmpty())
        setItem(0, 0);
    else if (list.count() == 1)
        setItem(list.first()->item(), list.first()->color());
    else
        setItems(list);
}
