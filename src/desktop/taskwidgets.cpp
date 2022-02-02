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

#include <cfloat>
#include <cmath>

#include <QDockWidget>
#include <QMainWindow>
#include <QEvent>
#include <QDesktopServices>
#include <QStatusTipEvent>
#include <QApplication>
#include <QStringBuilder>
#include <QStyledItemDelegate>
#include <QItemSelectionModel>
#include <QMenu>
#include <QDebug>

#include "bricklink/core.h"
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/documentlist.h"
#include "common/recentfiles.h"
#include "utility/utility.h"
#include "mainwindow.h"

#include "taskwidgets.h"

using namespace std::chrono_literals;


TaskPriceGuideWidget::TaskPriceGuideWidget(QWidget *parent)
    : PriceGuideWidget(parent), m_document(nullptr), m_dock(nullptr)
{
    setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout, this, [this]() {
        bool ok = (m_document && (m_selection.count() == 1));

        setPriceGuide(ok ? BrickLink::core()->priceGuide(m_selection.front()->item(),
                                                         m_selection.front()->color(), true)
                         : nullptr);
    });


    connect(MainWindow::inst(), &MainWindow::documentActivated,
            this, &TaskPriceGuideWidget::documentUpdate);
    connect(this, &PriceGuideWidget::priceDoubleClicked,
            this, &TaskPriceGuideWidget::setPrice);
    fixParentDockWindow();
}

void TaskPriceGuideWidget::documentUpdate(Document *document)
{
    if (m_document) {
        disconnect(m_document.data(), &Document::selectedLotsChanged,
                   this, &TaskPriceGuideWidget::selectionUpdate);
        disconnect(m_document->model(), &DocumentModel::currencyCodeChanged,
                   this, &TaskPriceGuideWidget::currencyUpdate);
    }
    m_document = document;
    if (m_document) {
        connect(m_document.data(), &Document::selectedLotsChanged,
                this, &TaskPriceGuideWidget::selectionUpdate);
        connect(m_document->model(), &DocumentModel::currencyCodeChanged,
                this, &TaskPriceGuideWidget::currencyUpdate);
    }

    setCurrencyCode(m_document ? m_document->model()->currencyCode() : Config::inst()->defaultCurrencyCode());
    selectionUpdate(m_document ? m_document->selectedLots() : LotList());
}

void TaskPriceGuideWidget::currencyUpdate(const QString &ccode)
{
    setCurrencyCode(ccode);
}

void TaskPriceGuideWidget::selectionUpdate(const LotList &list)
{
    m_selection = list;
    m_delayTimer.start();
}

void TaskPriceGuideWidget::setPrice(double p)
{
    if (m_document && (m_document->selectedLots().count() == 1)) {
        Lot *pos = m_document->selectedLots().front();
        Lot lot = *pos;

        auto doc = m_document->model();
        p *= Currency::inst()->rate(doc->currencyCode());
        lot.setPrice(p);

        doc->changeLot(pos, lot);
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
        disconnect(m_dock, &QDockWidget::dockLocationChanged,
                   this, &TaskPriceGuideWidget::dockLocationChanged);
        disconnect(m_dock, &QDockWidget::topLevelChanged,
                   this, &TaskPriceGuideWidget::topLevelChanged);
    }

    m_dock = nullptr;

    for (QObject *p = parent(); p; p = p->parent()) {
        if (qobject_cast<QDockWidget *>(p)) {
            m_dock = static_cast<QDockWidget *>(p);
            break;
        }
    }

    if (m_dock) {
        connect(m_dock, &QDockWidget::dockLocationChanged,
                this, &TaskPriceGuideWidget::dockLocationChanged);
        connect(m_dock, &QDockWidget::topLevelChanged,
                this, &TaskPriceGuideWidget::topLevelChanged);
    }
}

void TaskPriceGuideWidget::topLevelChanged(bool b)
{
    if (b) {
        setLayout(PriceGuideWidget::Normal);
        //setFixedSize(minimumSize());
    }
}

void TaskPriceGuideWidget::dockLocationChanged(Qt::DockWidgetArea area)
{
    bool vertical = (area ==  Qt::LeftDockWidgetArea) || (area == Qt::RightDockWidgetArea);

    setLayout(vertical ? PriceGuideWidget::Vertical : PriceGuideWidget::Horizontal);
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskInfoWidget::TaskInfoWidget(QWidget *parent)
    : QStackedWidget(parent), m_document(nullptr)
{
    setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_pic = new PictureWidget(this);
    m_text = new QLabel(this);
    m_text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_text->setIndent(8);

    m_text->setBackgroundRole(QPalette::Base);
    m_text->setAutoFillBackground(true);

    addWidget(m_pic);
    addWidget(m_text);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout,
            this, &TaskInfoWidget::delayedSelectionUpdate);

    connect(MainWindow::inst(), &MainWindow::documentActivated,
            this, &TaskInfoWidget::documentUpdate);
    connect(Config::inst(), &Config::measurementSystemChanged,
            this, &TaskInfoWidget::refresh);
}

void TaskInfoWidget::documentUpdate(Document *document)
{
    if (m_document) {
        disconnect(m_document.data(), &Document::selectedLotsChanged,
                   this, &TaskInfoWidget::selectionUpdate);
        disconnect(m_document->model(), &DocumentModel::statisticsChanged,
                   this, &TaskInfoWidget::statisticsUpdate);
        disconnect(m_document->model(), &DocumentModel::currencyCodeChanged,
                   this, &TaskInfoWidget::currencyUpdate);
    }
    m_document = document;
    if (m_document) {
        connect(m_document.data(), &Document::selectedLotsChanged,
                this, &TaskInfoWidget::selectionUpdate);
        connect(m_document->model(), &DocumentModel::statisticsChanged,
                this, &TaskInfoWidget::statisticsUpdate);
        connect(m_document->model(), &DocumentModel::currencyCodeChanged,
                this, &TaskInfoWidget::currencyUpdate);
    }

    selectionUpdate(m_document ? m_document->selectedLots() : LotList());
}

void TaskInfoWidget::currencyUpdate()
{
    selectionUpdate(m_document ? m_document->selectedLots() : LotList());
}

void TaskInfoWidget::statisticsUpdate()
{
    // only relevant, if we are showing the current document info
    if (m_selection.isEmpty())
        selectionUpdate(m_selection);
}

void TaskInfoWidget::selectionUpdate(const LotList &list)
{
    m_selection = list;
    m_delayTimer.start();
}

void TaskInfoWidget::delayedSelectionUpdate()
{
    if (!m_document) {
        m_pic->setItemAndColor(nullptr);
        setCurrentWidget(m_pic);
    } else if (m_selection.count() == 1) {
        m_pic->setItemAndColor(m_selection.front()->item(), m_selection.front()->color());
        setCurrentWidget(m_pic);
    } else {
        auto stat = m_document->model()->statistics(m_selection.isEmpty() ? m_document->model()->lots()
                                                                        : m_selection,
                                                  false /* ignoreExcluded */);
        QLocale loc;
        QString ccode = m_document->model()->currencyCode();
        QString wgtstr;
        QString minvalstr;
        QString valstr = Currency::toDisplayString(stat.value());
        bool hasMinValue = !qFuzzyCompare(stat.value(), stat.minValue());

        if (hasMinValue)
            minvalstr = Currency::toDisplayString(stat.minValue());

        QString coststr = Currency::toDisplayString(stat.cost());
        QString profitstr;
        if (!qFuzzyIsNull(stat.cost())) {
            int percent = int(std::round(stat.value() / stat.cost() * 100. - 100.));
            profitstr = (percent > 0 ? u"(+" : u"(") % loc.toString(percent) % u" %)";
        }


        if (qFuzzyCompare(stat.weight(), -std::numeric_limits<double>::min())) {
            wgtstr = "-"_l1;
        } else {
            double weight = stat.weight();

            if (weight < 0) {
                weight = -weight;
                wgtstr = tr("min.") % u' ';
            }

            wgtstr += Utility::weightToString(weight, Config::inst()->measurementSystem(), true, true);
        }

        static const char *fmt =
                "<h3>%1</h3><table cellspacing=6>"
                "<tr><td>&nbsp;&nbsp;%2 </td><td colspan=2 align=right>&nbsp;&nbsp;%3</td></tr>"
                "<tr><td>&nbsp;&nbsp;%4 </td><td colspan=2 align=right>&nbsp;&nbsp;%5</td></tr>"
                "<tr></tr>"
                "<tr><td>&nbsp;&nbsp;%6 </td><td>&nbsp;&nbsp;%7</td><td align=right>%8</td></tr>"
                "<tr><td>&nbsp;&nbsp;%9 </td><td>&nbsp;&nbsp;%10</td><td align=right>%11</td></tr>"
                "<tr><td>&nbsp;&nbsp;%12 </td><td>&nbsp;&nbsp;%13</td><td align=right>%14</td><td>&nbsp;&nbsp;%15</td></tr>"
                "<tr></tr>"
                "<tr><td>&nbsp;&nbsp;%16 </td><td colspan=2 align=right>&nbsp;&nbsp;%17</td></tr>"
                "</table>";

        QString s = QString::fromLatin1(fmt).arg(
                m_selection.isEmpty() ? tr("Document statistics") : tr("Multiple lots selected"),
                tr("Lots:"), loc.toString(stat.lots()),
                tr("Items:"), loc.toString(stat.items()),
                tr("Value:"), ccode, valstr).arg(
                hasMinValue ? tr("Value (min.):") : QString(), hasMinValue ? ccode : QString(), minvalstr,
                tr("Cost:"), ccode, coststr, profitstr,
                tr("Weight:"), wgtstr);

        m_pic->setItemAndColor(nullptr);
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
    if (m_document)
        selectionUpdate(m_document->selectedLots());
}

void TaskInfoWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    QStackedWidget::changeEvent(e);
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

TaskAppearsInWidget::TaskAppearsInWidget(QWidget *parent)
    : AppearsInWidget(true /*allowPartOut*/, parent), m_document(nullptr)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    connect(MainWindow::inst(), &MainWindow::documentActivated, this, &TaskAppearsInWidget::documentUpdate);

    m_delayTimer.setSingleShot(true);
    m_delayTimer.setInterval(120ms);

    connect(&m_delayTimer, &QTimer::timeout, this, [this]() {
        if (!m_document || m_selection.isEmpty())
            setItem(nullptr, nullptr);
        else if (m_selection.count() == 1)
            setItem(m_selection.constFirst()->item(), m_selection.constFirst()->color());
        else
            setItems(m_selection);
    });
}

void TaskAppearsInWidget::documentUpdate(Document *document)
{
    if (m_document) {
        disconnect(m_document.data(), &Document::selectedLotsChanged,
                   this, &TaskAppearsInWidget::selectionUpdate);
    }
    m_document = document;
    if (m_document) {
        connect(m_document.data(), &Document::selectedLotsChanged,
                this, &TaskAppearsInWidget::selectionUpdate);
    }

    selectionUpdate(m_document ? m_document->selectedLots() : LotList());
}

void TaskAppearsInWidget::selectionUpdate(const LotList &list)
{
    m_selection = list;
    m_delayTimer.start();
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


TaskOpenDocumentsWidget::TaskOpenDocumentsWidget(QWidget *parent)
    : QTreeView(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAlternatingRowColors(true);
    setHeaderHidden(true);
    setAllColumnsShowFocus(true);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setTextElideMode(Qt::ElideMiddle);
    setWordWrap(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setModel(DocumentList::inst());
    int is = style()->pixelMetric(QStyle::PM_ListViewIconSize, nullptr, this);
    setIconSize({ int(is * 2), int(is * 2) });

    m_contextMenu = new QMenu(this);

    m_closeDocument = new QAction(this);
    m_closeDocument->setIcon(ActionManager::inst()->qAction("document_close")->icon());

    connect(m_closeDocument, &QAction::triggered, this, [this]() {
        auto idx = m_contextMenu->property("contextIndex").value<QModelIndex>();
        if (idx.isValid()) {
            if (auto *doc = idx.data(Qt::UserRole).value<Document *>())
                doc->requestClose();
        }
    });

    m_contextMenu->addAction(m_closeDocument);

    connect(MainWindow::inst(), &MainWindow::documentActivated,
            this, [this](Document *doc) {
        int row = DocumentList::inst()->documents().indexOf(doc);
        selectionModel()->select(DocumentList::inst()->index(row, 0),
                                 QItemSelectionModel::ClearAndSelect);
    });

    connect(this, &QTreeView::clicked, this, [](const QModelIndex &idx) {
        if (idx.isValid()) {
            if (auto *doc = idx.data(Qt::UserRole).value<Document *>())
                emit doc->requestActivation();
        }
    });

    connect(this, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        auto idx = indexAt(pos);
        if (idx.isValid()) {
            m_contextMenu->setProperty("contextIndex", idx);
            m_contextMenu->popup(viewport()->mapToGlobal(pos));
        }
    });

    languageChange();
}

void TaskOpenDocumentsWidget::languageChange()
{
    m_closeDocument->setText(ActionManager::inst()->qAction("document_close")->text());
}

void TaskOpenDocumentsWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    return QTreeView::changeEvent(e);
}


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


TaskRecentDocumentsWidget::TaskRecentDocumentsWidget(QWidget *parent)
    : QTreeView(parent)
{
    //setItemDelegate(new QStyledItemDelegate());
    setAlternatingRowColors(true);
    setHeaderHidden(true);
    setAllColumnsShowFocus(true);
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setTextElideMode(Qt::ElideMiddle);
    setSelectionMode(QAbstractItemView::NoSelection);
    setModel(RecentFiles::inst());

    connect(this, &QTreeView::clicked, this, [](const QModelIndex &idx) {
        if (idx.isValid()) {
            QString fp = idx.data(RecentFiles::FilePathRole).toString();
            if (!fp.isEmpty())
                Document::load(fp);
        }
    });
}

#include "moc_taskwidgets.cpp"
