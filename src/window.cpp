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
#include <cmath>

#include <QLayout>
#include <QLabel>
#include <QApplication>
#include <QFileDialog>
#include <QValidator>
#include <QClipboard>
#include <QCursor>
#include <QTableView>
#include <QScrollArea>
#include <QSizeGrip>
#include <QHeaderView>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QPrinter>
#include <QPrintDialog>
#include <QProgressDialog>
#include <QStringBuilder>
#include <QScrollBar>
#include <QMenu>
#include <QPainter>
#include <QStandardPaths>
#include <QRegularExpression>

#include "messagebox.h"
#include "config.h"
#include "framework.h"
#include "utility.h"
#include "currency.h"
#include "document.h"
#include "documentio.h"
#include "window.h"
#include "window_p.h"
#include "headerview.h"
#include "documentdelegate.h"
#include "scriptmanager.h"
#include "script.h"
#include "exception.h"
#include "changecurrencydialog.h"

#include "bricklink/bricklink_setmatch.h"

#include "selectcolordialog.h"
#include "selectdocumentdialog.h"
#include "settopriceguidedialog.h"
#include "incdecpricesdialog.h"
#include "consolidateitemsdialog.h"
#include "selectprintingtemplatedialog.h"

using namespace std::chrono_literals;


// should be a function, but MSVC.Net doesn't accept default
// template arguments for template functions...

template <typename TG, typename TS = TG> class setOrToggle {
public:
    static void toggle(Window *w, const char *actiontext, TG(Document::Item::* getter)() const,
                       void (Document::Item::* setter)(TS t), TS val1, TS val2)
    {
        set_or_toggle(w, true, actiontext, getter, setter, val1, val2);
    }

    static void set(Window *w, const char *actiontext, TG(Document::Item::* getter)() const,
                    void (Document::Item::* setter)(TS t), TS val)
    {
        set_or_toggle(w, false, actiontext, getter, setter, val, TG());
    }

private:
    static void set_or_toggle(Window *w, bool toggle, const char *actiontext,
                              TG(Document::Item::* getter)() const,
                              void (Document::Item::* setter)(TS t), TS val1, TS val2 = TG())
    {
        Document *doc = w->document();

        if (w->selection().isEmpty())
            return;

        //DisableUpdates wp ( w_list );
        doc->beginMacro();
        int count = 0;

        foreach (Document::Item *pos, w->selection()) {
            if (toggle) {
                TG val = (pos->* getter)();

                if (val == val1)
                    val = val2;
                else if (val == val2)
                    val = val1;

                if (val != (pos->* getter)()) {
                    Document::Item item = *pos;

                    (item.* setter)(val);
                    doc->changeItem(pos, item);

                    count++;
                }
            }
            else {
                if ((pos->* getter)() != val1) {
                    Document::Item item = *pos;

                    (item.* setter)(val1);
                    doc->changeItem(pos, item);

                    count++;
                }
            }
        }


        doc->endMacro(Window::tr(actiontext, nullptr, count));
    }
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ColumnChangeWatcher::ColumnChangeWatcher(Window *window, HeaderView *header)
    : m_window(window)
    , m_header(header)
{
    // redirect the signals, so that we can block the them while we are applying new values
    connect(header, &HeaderView::sectionMoved,
            this, &ColumnChangeWatcher::internalColumnMoved);
    connect(this, &ColumnChangeWatcher::internalColumnMoved,
            this, [this](int logical, int oldVisual, int newVisual) {
        if (!m_header->isSectionAvailable(logical))
            return;
        m_window->document()->undoStack()->push(new ColumnCmd(this, true,
                                                              ColumnCmd::Type::Move,
                                                              logical, oldVisual, newVisual));
    });
    connect(header, &HeaderView::sectionResized,
            this, &ColumnChangeWatcher::internalColumnResized);
    connect(this, &ColumnChangeWatcher::internalColumnResized,
            this, [this](int logical, int oldSize, int newSize) {
        if (!m_header->isSectionAvailable(logical))
            return;
        m_window->document()->undoStack()->push(new ColumnCmd(this, true,
                                                              ColumnCmd::Type::Resize,
                                                              logical, oldSize, newSize));
    });
}

void ColumnChangeWatcher::moveColumn(int logical, int oldVisual, int newVisual)
{
    Q_UNUSED(logical)
    disable();
    m_header->moveSection(oldVisual, newVisual);
    enable();
}

void ColumnChangeWatcher::resizeColumn(int logical, int oldSize, int newSize)
{
    Q_UNUSED(oldSize)
    disable();
    m_header->resizeSection(logical, newSize);
    enable();
}

void ColumnChangeWatcher::enable()
{
    blockSignals(false);
}

void ColumnChangeWatcher::disable()
{
    blockSignals(true);
}

QString ColumnChangeWatcher::columnTitle(int logical) const
{
    return m_header->model()->headerData(logical, Qt::Horizontal).toString();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ColumnCmd::ColumnCmd(ColumnChangeWatcher *ccw, bool alreadyDone, ColumnCmd::Type type, int logical,
                     int oldValue, int newValue)
    : QUndoCommand()
    , m_ccw(ccw)
    , m_skipFirstRedo(alreadyDone)
    , m_type(type)
    , m_logical(logical)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
    if (type == Type::Move)
        setText(qApp->translate("ColumnCmd", "Moved column %1").arg(ccw->columnTitle(logical)));
    else
        setText(qApp->translate("ColumnCmd", "Resized column %1").arg(ccw->columnTitle(logical)));
}

int ColumnCmd::id() const
{
    return CID_Column;
}

bool ColumnCmd::mergeWith(const QUndoCommand *other)
{
    auto otherColumn = static_cast<const ColumnCmd *>(other);
    if ((otherColumn->m_type == m_type) && (otherColumn->m_logical == m_logical)) {
        m_oldValue = otherColumn->m_oldValue; // actually the new size, but we're swapped already
        return true;
    }
    return false;
}

void ColumnCmd::redo()
{
    if (m_skipFirstRedo) {
        m_skipFirstRedo = false;
    } else {
        if (m_type == Type::Move)
            m_ccw->moveColumn(m_logical, m_oldValue, m_newValue);
        else
            m_ccw->resizeColumn(m_logical, m_oldValue, m_newValue);
    }
    std::swap(m_oldValue, m_newValue);
}

void ColumnCmd::undo()
{
    redo();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


WindowProgressHelper::WindowProgressHelper(QProgressDialog *pd)
    : QObject(pd)
{
    pd->installEventFilter(this);
}

bool WindowProgressHelper::eventFilter(QObject *watched, QEvent *event)
{
    // eat the Escape key
    if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(event)->matches(QKeySequence::Cancel))
            return true;
    }
    return QObject::eventFilter(watched, event);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


WindowProgress::WindowProgress(QWidget *w, const QString &title, int total)
    : m_w(w), m_reenabled(false)
{
    auto *sa = qobject_cast<QAbstractScrollArea *>(w);
    m_w = sa ? sa->viewport() : w;
    m_upd_enabled = m_w->updatesEnabled();
    m_w->setUpdatesEnabled(false);
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    if (total) {
        m_progress = new QProgressDialog(title, QString(), 0, total, w->window(),
                                         Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
        m_progress->setWindowModality(Qt::WindowModal);
        m_progress->setMinimumDuration(1500);
        m_progress->setValue(0);
        new WindowProgressHelper(m_progress);
    } else {
        m_progress = nullptr;
    }
}

WindowProgress::~WindowProgress()
{
    stop();
}

void WindowProgress::stop()
{
    if (!m_reenabled) {
        delete m_progress;
        m_progress = nullptr;
        QApplication::restoreOverrideCursor();
        m_w->setUpdatesEnabled(m_upd_enabled);
        m_reenabled = true;
    }
}

void WindowProgress::step(int d)
{
    m_counter += d;

    if (m_progress) {
        int total = m_progress->maximum();
        if ((m_counter == total) || ((m_counter - m_lastTick) >= (total / 100))) {
            m_progress->setValue(m_counter);
            m_lastTick = m_counter;
        }
    }
}

void WindowProgress::setOverrideCursor()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

void WindowProgress::restoreOverrideCursor()
{
    QApplication::restoreOverrideCursor();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////



TableView::TableView(QWidget *parent)
    : QTableView(parent)
{
    setCornerWidget(new QSizeGrip(this));
}

void TableView::keyPressEvent(QKeyEvent *e)
{
    QTableView::keyPressEvent(e);
#if !defined(Q_OS_MACOS)
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (state() != EditingState) {
            if (edit(currentIndex(), EditKeyPressed, e))
                e->accept();
        }
    }
#endif
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


CheckColorTabBar::CheckColorTabBar()
{
    addTab(" ");
}

QColor CheckColorTabBar::color() const
{
    QImage image(20, 20, QImage::Format_ARGB32);
    image.fill(palette().color(QPalette::Window));
    QPainter p(&image);
    QStyleOptionTab opt;
    opt.initFrom(this);
    initStyleOption(&opt, 0);
    opt.rect = image.rect();
    style()->drawControl(QStyle::CE_TabBarTab, &opt, &p, this);
    p.end();
    return image.pixelColor(image.rect().center());
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


StatusBar::StatusBar(Window *window)
    : QFrame(window)
    , m_window(window)
    , m_doc(window->document())
{
    setAutoFillBackground(true);
    setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Sunken));

    // hide the top and bottom frame
    setContentsMargins(contentsMargins() + QMargins(0, -frameWidth(), 0, -frameWidth()));

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(11, 4, 11, 4);

    auto addSeparator = [this, &layout]() -> QWidget * {
            auto sep = new QFrame();
            sep->setFrameShape(QFrame::VLine);
            sep->setFixedWidth(sep->frameWidth());
            sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
            layout->addWidget(sep);
            return sep;
    };

    m_differenceMode = new QToolButton();
    m_differenceMode->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_differenceMode->setIcon(QIcon::fromTheme("mode2"));
    m_differenceMode->setCheckable(true);
    connect(m_doc, &Document::differenceModeChanged,
            this, &StatusBar::updateDifferenceMode);
    connect(m_doc, &Document::itemCountChanged,
            this, [this](int count) { m_differenceMode->setEnabled(count > 0); });
    m_differenceMode->setEnabled(m_doc->items().count() > 0);
    connect(m_differenceMode, &QToolButton::toggled,
            this, [this](bool b) {
        if (b == m_doc->isDifferenceModeActive())
            return;
        if (b) {
            m_doc->setDifferenceModeActive(true);
        } else {
            if (m_doc->lastCommandWasActivateDifferenceMode()
                    || (MessageBox::question(this, tr("End difference mode"),
                                             tr("Ending difference mode resets all base values used for calculating the actual differences to the current values.<br>This operation is not undoable.<br>Do you still want to continue?"))
                        == QMessageBox::Yes)) {
                m_doc->setDifferenceModeActive(false);
            } else {
                m_differenceMode->setChecked(true);
            }
        }
    });
    layout->addWidget(m_differenceMode);

    if (m_doc->order()) {
        addSeparator();
        m_order = new QToolButton();
        m_order->setIcon(QIcon::fromTheme("help-about"));
        m_order->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_order->setAutoRaise(true);
        layout->addWidget(m_order);

        connect(m_order, &QToolButton::clicked,
                this, [this]() {
            auto order = m_doc->order();
            MessageBox::information(m_window, tr("Order information"), order->address());
        });
    } else {
        m_order = nullptr;
    }
    layout->addStretch(1);

    m_errorsSeparator = addSeparator();
    m_errors = new QToolButton();
    m_errors->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_errors->setIcon(QIcon::fromTheme("edit-find"));
    m_errors->setShortcut(tr("F6"));
    m_errors->setAutoRaise(true);
    connect(m_errors, &QToolButton::clicked,
            m_window, &Window::gotoNextError);
    layout->addWidget(m_errors);

    addSeparator();
    m_count = new QLabel();
    layout->addWidget(m_count);

    addSeparator();
    m_weight = new QLabel();
    layout->addWidget(m_weight);

    addSeparator();

    QHBoxLayout *currencyLayout = new QHBoxLayout();
    currencyLayout->setSpacing(0);
    m_value = new QLabel();
    currencyLayout->addWidget(m_value);
    m_currency = new QToolButton();
    m_currency->setPopupMode(QToolButton::InstantPopup);
    m_currency->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_currency->setAutoRaise(true);
    currencyLayout->addWidget(m_currency);
    layout->addLayout(currencyLayout);
    m_profit = new QLabel();
    layout->addWidget(m_profit);

    connect(m_currency, &QToolButton::triggered,
            this, &StatusBar::changeDocumentCurrency);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &StatusBar::updateCurrencyRates);
    connect(m_doc, &Document::currencyCodeChanged,
            this, &StatusBar::documentCurrencyChanged);
    connect(m_doc, &Document::statisticsChanged,
            this, &StatusBar::updateStatistics);

    updateCurrencyRates();
    documentCurrencyChanged(m_doc->currencyCode());

    paletteChange();
    languageChange();
}

void StatusBar::updateCurrencyRates()
{
    delete m_currency->menu();
    auto m = new QMenu();

    QString defCurrency = Config::inst()->defaultCurrencyCode();

    if (!defCurrency.isEmpty()) {
        auto a = new QAction(tr("Default currency (%1)").arg(defCurrency));
        a->setObjectName(defCurrency);
        m->addAction(a);
        m->addSeparator();
    }

    foreach (const QString &c, Currency::inst()->currencyCodes()) {
        auto a = new QAction(c);
        a->setObjectName(c);
        if (c == m_doc->currencyCode())
            a->setEnabled(false);
        m->addAction(a);
    }
    m_currency->setMenu(m);
}

void StatusBar::documentCurrencyChanged(const QString &ccode)
{
    m_currency->setText(ccode + QLatin1String("  "));
    // the menu might still be open right now, so we need to delay deleting the actions
    QMetaObject::invokeMethod(this, &StatusBar::updateCurrencyRates, Qt::QueuedConnection);
}

void StatusBar::changeDocumentCurrency(QAction *a)
{
    QString ccode = a->objectName();

    if (ccode != m_doc->currencyCode()) {
        ChangeCurrencyDialog d(m_doc->currencyCode(), ccode, m_doc->legacyCurrencyCode(), this);
        if (d.exec() == QDialog::Accepted) {
            double rate = d.exchangeRate();

            if (rate > 0)
                m_doc->setCurrencyCode(ccode, rate);
        }
    }
}

void StatusBar::updateDifferenceMode()
{
    bool b = m_doc->isDifferenceModeActive();
    m_differenceMode->setText(m_differenceMode->property(b ? "bsTextUpdate" : "bsTextNormal").toString());
    m_differenceMode->setChecked(b);
}

void StatusBar::updateStatistics()
{
    static QLocale loc;
    auto stat = m_doc->statistics(m_doc->items(), true /* ignoreExcluded */);

    bool b = (stat.errors() > 0);
    if (b && Config::inst()->showInputErrors()) {
        auto oldShortcut = m_errors->shortcut();
        m_errors->setText(tr("%n Error(s)", nullptr, stat.errors()));
        m_errors->setShortcut(oldShortcut);
    }
    m_errors->setVisible(b);
    m_errorsSeparator->setVisible(b);

    QString cntstr = tr("Items") % u": " % loc.toString(stat.items())
            % u" (" % loc.toString(stat.lots()) % u")";
    m_count->setText(cntstr);

    QString wgtstr;
    if (qFuzzyCompare(stat.weight(), -std::numeric_limits<double>::min())) {
        wgtstr = QStringLiteral("-");
    } else {
        wgtstr = Utility::weightToString(std::abs(stat.weight()),
                                         Config::inst()->measurementSystem(),
                                         true /*optimize*/, true /*add unit*/);
        if (stat.weight() < 0)
            wgtstr.prepend(QStringLiteral(u"\u2265 "));
    }
    m_weight->setText(wgtstr);

    QString valstr = loc.toString(stat.value(), 'f', 3);
    if (stat.minValue() < stat.value())
        valstr.prepend(QStringLiteral(u"\u2264 "));
    m_value->setText(valstr);

    b = !qFuzzyIsNull(stat.cost());
    if (b) {
        int percent = int(std::round(stat.value() / stat.cost() * 100. - 100.));
        QString profitstr = (percent > 0 ? u"(+" : u"(") % loc.toString(percent) % u" %)";
        m_profit->setText(profitstr);
    }
    m_profit->setVisible(b);
}

void StatusBar::languageChange()
{
    m_differenceMode->setProperty("bsTextUpdate", tr("Disable difference mode"));
    m_differenceMode->setProperty("bsTextNormal", tr("Enable difference mode"));
    updateDifferenceMode();

    m_errors->setToolTip(Utility::toolTipLabel(tr("Go to the next error"), m_errors->shortcut()));

    if (m_order)
        m_order->setText(tr("Order information..."));
    m_value->setText(tr("Currency:"));
    updateStatistics();
}

void StatusBar::paletteChange()
{
    auto pal = palette();
    pal.setColor(QPalette::Window, CheckColorTabBar().color());
    setPalette(pal);

    updateStatistics();

    auto coloredToolButton = [this](QToolButton *tb, const QColor &baseColor, bool checkable) {
        auto winbg = palette().color(QPalette::Window);

        auto downbg = Utility::gradientColor(baseColor, winbg, 0.5);
        bool darkText = (tb->palette().color(QPalette::ButtonText).lightnessF() < 0.5);
        QColor border;
        QColor hoverbg;
        if (darkText) {
            border = downbg.darker(150);
            hoverbg = downbg.lighter();
        } else {
            border = downbg.lighter();
            hoverbg = downbg.darker(150);
        }
        auto bgName = checkable ? QString::fromLatin1("transparent")
                                : hoverbg.name();

        tb->setStyleSheet(QString::fromLatin1(
                              "QToolButton         { background-color: %1; border: 1px solid transparent; padding: 1px; }"
                "QToolButton:hover   { background-color: %3; border: 1px solid %4 } "
                "QToolButton:checked { background-color: %2; border: 1px solid %4; } "
                "QToolButton:checked:hover { } "
                "QToolButton:pressed { background-color: %2; border: 1px solid %4 } "
            ).arg(bgName, downbg.name(), hoverbg.name(), border.name()));
    };

    coloredToolButton(m_differenceMode, Qt::green, true);
    coloredToolButton(m_errors, Qt::red, false);
}

void StatusBar::changeEvent(QEvent *e)
{
    QFrame::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    if (e->type() == QEvent::PaletteChange)
        paletteChange();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


Window::Window(Document *doc, QWidget *parent)
    : QWidget(parent)
    , m_uuid(QUuid::createUuid())
{
    setAttribute(Qt::WA_DeleteOnClose);

    m_doc = doc;
    m_doc->setParent(this);

    m_latest_row = -1;
    m_latest_timer = new QTimer(this);
    m_latest_timer->setSingleShot(true);
    m_latest_timer->setInterval(100ms);

    m_settopg_failcnt = 0;
    m_settopg_list = nullptr;
    m_settopg_time = BrickLink::Time::PastSix;
    m_settopg_price = BrickLink::Price::Average;
    m_simple_mode = false;
    m_diff_mode = false;

    w_statusbar = new StatusBar(this);

    w_list = new TableView();
    w_header = new HeaderView(Qt::Horizontal, w_list);
    w_header->setSectionsClickable(true);
    w_header->setSectionsMovable(true);
    w_header->setHighlightSections(false);
    w_list->setHorizontalHeader(w_header);
    w_header->setSortIndicatorShown(true);
    w_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    w_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    w_list->setAlternatingRowColors(true);
    w_list->setAutoFillBackground(false);
    w_list->setTabKeyNavigation(true);
    w_list->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    w_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    w_list->setContextMenuPolicy(Qt::CustomContextMenu);
    w_list->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    w_list->verticalHeader()->hide();
    w_list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    setFocusProxy(w_list);

    w_list->setModel(m_doc);
    m_selection_model = new QItemSelectionModel(m_doc, this);
    w_list->setSelectionModel(m_selection_model);

    connect(w_header, &QHeaderView::sectionClicked,
            this, [this](int section) {
        if (!document()->isSorted() && (section == document()->sortColumn())) {
            document()->reSort();
            w_header->setSortIndicatorShown(true);
            w_header->setSortIndicator(document()->sortColumn(), document()->sortOrder());
        } else {
            w_list->sortByColumn(section, w_header->sortIndicatorOrder());
        }
        w_list->scrollTo(m_selection_model->currentIndex());
    });
    connect(doc, &Document::sortColumnChanged,
            w_header, [this](int column) {
        w_header->setSortIndicator(column, m_doc->sortOrder());
    });
    connect(doc, &Document::sortOrderChanged,
            w_header, [this](Qt::SortOrder order) {
        w_header->setSortIndicator(m_doc->sortColumn(), order);
    });
    connect(doc, &Document::isSortedChanged,
            w_header, [this](bool b) {
        w_header->setSortIndicatorShown(b);
    });

    // This shouldn't be needed, but we are abusing layoutChanged a bit for adding and removing
    // items. The docs are a bit undecided if you should really do that, but it really helps
    // performance wise. Just the selection is not updated, when the items in it are deleted.
    connect(m_doc, &Document::layoutChanged,
            m_selection_model, [this]() {
        updateSelection();
    });

    auto *dd = new DocumentDelegate(doc, w_list);
    w_list->setItemDelegate(dd);
    w_list->verticalHeader()->setDefaultSectionSize(dd->defaultItemHeight(w_list));

    QBoxLayout *toplay = new QVBoxLayout(this);
    toplay->setSpacing(0);
    toplay->setContentsMargins(0, 0, 0, 0);
    toplay->addWidget(w_statusbar, 0);
    toplay->addWidget(w_list, 10);

    connect(m_latest_timer, &QTimer::timeout,
            this, &Window::ensureLatestVisible);
    connect(w_list, &QWidget::customContextMenuRequested,
            this, &Window::contextMenu);

    connect(m_selection_model, &QItemSelectionModel::selectionChanged,
            this, &Window::updateSelection);

    connect(BrickLink::core(), &BrickLink::Core::priceGuideUpdated,
            this, &Window::priceGuideUpdated);

    connect(Config::inst(), &Config::simpleModeChanged,
            this, &Window::setSimpleMode);
    connect(Config::inst(), &Config::showInputErrorsChanged,
            this, &Window::updateErrorMask);
    connect(Config::inst(), &Config::measurementSystemChanged,
            w_list->viewport(), QOverload<>::of(&QWidget::update));

    //connect(Config::inst(), SIGNAL(localCurrencyChanged()), w_list->viewport(), SLOT(update()));

    connect(m_doc, &Document::titleChanged,
            this, &Window::updateCaption);
    connect(m_doc, &Document::fileNameChanged,
            this, &Window::updateCaption);
    connect(m_doc, &Document::modificationChanged,
            this, &Window::updateCaption);
    connect(m_doc, &Document::differenceModeChanged,
            this, &Window::updateDifferenceMode);
    connect(m_doc, &Document::dataChanged,
            this, &Window::documentItemsChanged);

    // don't save the hidden status of these
    w_header->setSectionInternal(Document::PriceOrig, true);
    w_header->setSectionInternal(Document::PriceDiff, true);
    w_header->setSectionInternal(Document::QuantityOrig, true);
    w_header->setSectionInternal(Document::QuantityDiff, true);

    bool columnsSet = false;
    bool sortFilterSet = false;

    if (m_doc->hasGuiState()) {
        applyGuiStateXML(m_doc->guiState(), columnsSet, sortFilterSet);
        m_doc->clearGuiState();
    }

    if (!columnsSet) {
        auto layout = Config::inst()->columnLayout("user-default");
        if (!w_header->restoreLayout(layout)) {
            resizeColumnsToDefault();
            w_header->setSortIndicator(0, Qt::AscendingOrder);
        }
        columnsSet = true;
    }
    if (columnsSet && !sortFilterSet) {
        // ugly hack for older files without sort order to prevent an unclean undo stack
        m_doc->nextSortFilterIsDirect();
        w_list->sortByColumn(w_header->sortIndicatorSection(), w_header->sortIndicatorOrder());
    }

    updateDifferenceMode();
    setSimpleMode(Config::inst()->simpleMode());

    m_ccw = new ColumnChangeWatcher(this, w_header);

    updateErrorMask();
    updateCaption();

    setFocusProxy(w_list);

    connect(&m_autosave_timer, &QTimer::timeout,
            this, &Window::autosave);
    m_autosave_timer.start(1min); // every minute
}

Window::~Window()
{
    m_autosave_timer.stop();
    deleteAutosave();
}

void Window::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        updateCaption();
    QWidget::changeEvent(e);
}

void Window::updateCaption()
{
    QString cap = m_doc->fileNameOrTitle();
    if (cap.isEmpty())
        cap = tr("Untitled");

    cap += QLatin1String("[*]");

    setWindowTitle(cap);
    setWindowModified(m_doc->isModified());
}

bool Window::isSimpleMode() const
{
    return m_simple_mode;
}

QByteArray Window::currentColumnLayout() const
{
    return w_header->saveLayout();
}

void Window::setSimpleMode(bool b)
{
    m_simple_mode = b;

    for (const auto col : {
         Document::Bulk,
         Document::Sale,
         Document::TierQ1,
         Document::TierQ2,
         Document::TierQ3,
         Document::TierP1,
         Document::TierP2,
         Document::TierP3,
         Document::Reserved,
         Document::Stockroom,
         Document::Retain,
         Document::LotId,
         Document::Comments }) {
        w_header->setSectionAvailable(col, !b);
    }

    updateErrorMask();
}

void Window::ensureLatestVisible()
{
    if (m_latest_row >= 0) {
        int xOffset = w_list->horizontalScrollBar()->value();
        w_list->scrollTo(m_doc->index(m_latest_row, w_header->logicalIndexAt(-xOffset)));
        m_latest_row = -1;
    }
}

int Window::addItems(const BrickLink::InvItemList &items, AddItemMode addItemMode)
{
    // we own the items now

    if (items.isEmpty())
        return 0;

    bool startedMacro = false;

    const auto &documentItems = document()->items();
    bool wasEmpty = documentItems.isEmpty();
    Document::Item *lastAdded = nullptr;
    int addCount = 0;
    int consolidateCount = 0;
    Consolidate conMode = Consolidate::IntoExisting;
    bool repeatForRemaining = false;
    bool costQtyAvg = true;

    for (int i = 0; i < items.count(); ++i) {
        Document::Item *item = items.at(i);
        bool justAdd = true;

        if (addItemMode != AddItemMode::AddAsNew) {
            Document::Item *mergeItem = nullptr;

            for (int j = documentItems.count() - 1; j >= 0; --j) {
                Document::Item *otherItem = documentItems.at(j);
                if ((!item->isIncomplete() && !otherItem->isIncomplete())
                        && (item->item() == otherItem->item())
                        && (item->color() == otherItem->color())
                        && (item->condition() == otherItem->condition())
                        && ((item->status() == BrickLink::Status::Exclude) ==
                            (otherItem->status() == BrickLink::Status::Exclude))) {
                    mergeItem = otherItem;
                    break;
                }
            }

            if (mergeItem) {
                int mergeIndex = -1;

                if ((addItemMode == AddItemMode::ConsolidateInteractive) && !repeatForRemaining) {
                    BrickLink::InvItemList list { mergeItem, item };

                    ConsolidateItemsDialog dlg(this, list,
                                               conMode == Consolidate::IntoExisting ? 0 : 1,
                                               conMode, i + 1, items.count(), this);
                    bool yesClicked = (dlg.exec() == QDialog::Accepted);
                    repeatForRemaining = dlg.repeatForAll();
                    costQtyAvg = dlg.costQuantityAverage();


                    if (yesClicked) {
                        mergeIndex = dlg.consolidateToIndex();

                        if (repeatForRemaining)
                            conMode = dlg.consolidateRemaining();
                    } else {
                        if (repeatForRemaining)
                            addItemMode = AddItemMode::AddAsNew;
                    }
                } else {
                    mergeIndex = (conMode == Consolidate::IntoExisting) ? 0 : 1;
                }

                if (mergeIndex >= 0) {
                    justAdd = false;

                    if (!startedMacro) {
                        m_doc->beginMacro();
                        startedMacro = true;
                    }

                    if (mergeIndex == 0) {
                        // merge new into existing
                        Document::Item changedItem = *mergeItem;
                        changedItem.mergeFrom(*item, costQtyAvg);
                        m_doc->changeItem(mergeItem, changedItem);
                        delete item; // we own it, but we don't need it anymore
                    } else {
                        // merge existing into new, add new, remove existing
                        item->mergeFrom(*mergeItem, costQtyAvg);
                        m_doc->appendItem(item); // pass on ownership
                        m_doc->removeItem(mergeItem);
                    }

                    ++consolidateCount;
                }
            }
        }

        if (justAdd) {
            if (!startedMacro) {
                m_doc->beginMacro();
                startedMacro = true;
            }

            m_doc->appendItem(item);  // pass on ownership to the doc
            ++addCount;
            lastAdded = item;
        }
    }

    if (startedMacro)
        m_doc->endMacro(tr("Added %1, consolidated %2 items").arg(addCount).arg(consolidateCount));

    if (wasEmpty)
        w_list->selectRow(0);

    if (lastAdded) {
        m_latest_row = m_doc->index(lastAdded).row();
        m_latest_timer->start();
    }

    return items.count();
}


void Window::consolidateItems(const Document::ItemList &items)
{
    if (items.count() < 2)
        return;

    QVector<Document::ItemList> mergeList;
    Document::ItemList sourceItems = items;

    for (int i = 0; i < sourceItems.count(); ++i) {
        Document::Item *item = sourceItems.at(i);
        Document::ItemList mergeItems;

        for (int j = i + 1; j < sourceItems.count(); ++j) {
            Document::Item *otherItem = sourceItems.at(j);
            if ((!item->isIncomplete() && !otherItem->isIncomplete())
                    && (item->item() == otherItem->item())
                    && (item->color() == otherItem->color())
                    && (item->condition() == otherItem->condition())
                    && ((item->status() == BrickLink::Status::Exclude) ==
                        (otherItem->status() == BrickLink::Status::Exclude))) {
                mergeItems << sourceItems.takeAt(j--);
            }
        }
        if (mergeItems.isEmpty())
            continue;

        mergeItems.prepend(sourceItems.at(i));
        mergeList << mergeItems;
    }

    if (mergeList.isEmpty())
        return;

    bool startedMacro = false;

    auto conMode = Consolidate::IntoLowestIndex;
    bool repeatForRemaining = false;
    bool costQtyAvg = true;
    int consolidateCount = 0;

    for (int mi = 0; mi < mergeList.count(); ++mi) {
        const Document::ItemList &mergeItems = mergeList.at(mi);
        int mergeIndex = -1;

        if (!repeatForRemaining) {
            ConsolidateItemsDialog dlg(this, mergeItems, consolidateItemsHelper(mergeItems, conMode),
                                       conMode, mi + 1, mergeList.count(), this);
            bool yesClicked = (dlg.exec() == QDialog::Accepted);
            repeatForRemaining = dlg.repeatForAll();
            costQtyAvg = dlg.costQuantityAverage();

            if (yesClicked) {
                mergeIndex = dlg.consolidateToIndex();

                if (repeatForRemaining)
                    conMode = dlg.consolidateRemaining();
            } else {
                if (repeatForRemaining)
                    break;
                else
                    continue; // TODO: we may end up with an empty macro
            }
        } else {
            mergeIndex = consolidateItemsHelper(mergeItems, conMode);
        }

        if (!startedMacro) {
            m_doc->beginMacro();
            startedMacro = true;
        }

        Document::Item newitem = *mergeItems.at(mergeIndex);
        for (int i = 0; i < mergeItems.count(); ++i) {
            if (i != mergeIndex) {
                newitem.mergeFrom(*mergeItems.at(i), costQtyAvg);
                m_doc->removeItem(mergeItems.at(i));
            }    
        }
        m_doc->changeItem(mergeItems.at(mergeIndex), newitem);

        ++consolidateCount;
    }
    if (startedMacro)
        m_doc->endMacro(tr("Consolidated %n item(s)", nullptr, consolidateCount));
}

int Window::consolidateItemsHelper(const Document::ItemList &items, Consolidate conMode) const
{
    switch (conMode) {
    case Consolidate::IntoTopSorted:
        return 0;
    case Consolidate::IntoBottomSorted:
        return items.count() - 1;
    case Consolidate::IntoLowestIndex: {
        const auto di = document()->items();
        auto it = std::min_element(items.cbegin(), items.cend(), [di](const auto &a, const auto &b) {
            return di.indexOf(a) < di.indexOf(b);
        });
        return int(std::distance(items.cbegin(), it));
    }
    case Consolidate::IntoHighestIndex: {
        const auto di = document()->items();
        auto it = std::max_element(items.cbegin(), items.cend(), [di](const auto &a, const auto &b) {
            return di.indexOf(a) < di.indexOf(b);
        });
        return int(std::distance(items.cbegin(), it));
    }
    default:
        break;
    }
    return -1;
}

QDomElement Window::createGuiStateXML()
{
    QDomDocument doc; // dummy
    int version = 2;

    QDomElement root = doc.createElement("GuiState");
    root.setAttribute("Application", "BrickStore");
    root.setAttribute("Version", version);

    auto cl = doc.createElement("ColumnLayout");
    cl.setAttribute("Compressed", 1);
    cl.appendChild(doc.createCDATASection(qCompress(w_header->saveLayout()).toBase64()));
    root.appendChild(cl);

    auto sf = doc.createElement("SortFilterState");
    sf.setAttribute("Compressed", 1);
    sf.appendChild(doc.createCDATASection(qCompress(m_doc->saveSortFilterState()).toBase64()));
    root.appendChild(sf);

    return root.cloneNode(true).toElement();
}

void Window::applyGuiStateXML(const QDomElement &root, bool &changedColumns, bool &changedSortFilter)
{
    changedColumns = changedSortFilter = false;

    if ((root.nodeName() == "GuiState") &&
        (root.attribute("Application") == "BrickStore") &&
        (root.attribute("Version").toInt() == 2)) {

        auto cl = root.firstChildElement("ColumnLayout");
        if (!cl.isNull()) {
            auto ba = QByteArray::fromBase64(cl.text().toLatin1());
            if (cl.attribute("Compressed").toInt() == 1)
                ba = qUncompress(ba);
            changedColumns = w_header->restoreLayout(ba);
        }

        auto sf = root.firstChildElement("SortFilterState");
        if (!sf.isNull()) {
            auto ba = QByteArray::fromBase64(sf.text().toLatin1());
            if (sf.attribute("Compressed").toInt() == 1)
                ba = qUncompress(ba);
            changedSortFilter = m_doc->restoreSortFilterState(ba);
        }
    }
}

void Window::on_edit_cut_triggered()
{
    on_edit_copy_triggered();
    on_edit_delete_triggered();
}

void Window::on_edit_copy_triggered()
{
    if (!selection().isEmpty())
        QApplication::clipboard()->setMimeData(new BrickLink::InvItemMimeData(selection()));
}

void Window::on_edit_paste_triggered()
{
    BrickLink::InvItemList bllist = BrickLink::InvItemMimeData::items(QApplication::clipboard()->mimeData());

    if (!bllist.isEmpty()) {
        if (!selection().isEmpty()) {
            if (MessageBox::question(this, { }, tr("Overwrite the currently selected items?"),
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
                                     ) == QMessageBox::Yes) {
                m_doc->removeItems(selection());
            }
        }
        addItems(bllist, AddItemMode::ConsolidateInteractive);
    }
}

void Window::on_edit_delete_triggered()
{
    if (!selection().isEmpty())
        m_doc->removeItems(selection());
}

void Window::on_edit_select_all_triggered()
{
    w_list->selectAll();
}

void Window::on_edit_select_none_triggered()
{
    w_list->clearSelection();
}

void Window::on_edit_filter_from_selection_triggered()
{
    if (selection().count() == 1) {
        auto idx = m_selection_model->currentIndex();
        if (idx.isValid() && idx.column() >= 0) {
            FrameWork::inst()->setFilter(m_doc->headerData(idx.column(), Qt::Horizontal).toString()
                                         + QLatin1String(" == ")
                                         + m_doc->data(idx, Document::FilterRole).toString());
        }
    }
}

void Window::on_edit_status_include_triggered()
{
    setOrToggle<BrickLink::Status>::set(this, QT_TR_N_NOOP("Set 'include' status on %n item(s)"),
                                        &Document::Item::status, &Document::Item::setStatus,
                                        BrickLink::Status::Include);
}

void Window::on_edit_status_exclude_triggered()
{
    setOrToggle<BrickLink::Status>::set(this, QT_TR_N_NOOP("Set 'exclude' status on %n item(s)"),
                                        &Document::Item::status, &Document::Item::setStatus,
                                        BrickLink::Status::Exclude);
}

void Window::on_edit_status_extra_triggered()
{
    setOrToggle<BrickLink::Status>::set(this, QT_TR_N_NOOP("Set 'extra' status on %n item(s)"),
                                        &Document::Item::status, &Document::Item::setStatus,
                                        BrickLink::Status::Extra);
}

void Window::on_edit_status_toggle_triggered()
{
    setOrToggle<BrickLink::Status>::toggle(this, QT_TR_N_NOOP("Toggled status on %n item(s)"),
                                           &Document::Item::status, &Document::Item::setStatus,
                                           BrickLink::Status::Include, BrickLink::Status::Exclude);
}

void Window::on_edit_cond_new_triggered()
{
    setOrToggle<BrickLink::Condition>::set(this, QT_TR_N_NOOP("Set 'new' condition on %n item(s)"),
                                           &Document::Item::condition, &Document::Item::setCondition,
                                           BrickLink::Condition::New);
}

void Window::on_edit_cond_used_triggered()
{
    setOrToggle<BrickLink::Condition>::set(this, QT_TR_N_NOOP("Set 'used' condition on %n item(s)"),
                                           &Document::Item::condition, &Document::Item::setCondition,
                                           BrickLink::Condition::Used);
}

void Window::on_edit_cond_toggle_triggered()
{
    setOrToggle<BrickLink::Condition>::toggle(this, QT_TR_N_NOOP("Toggled condition on %n item(s)"),
                                              &Document::Item::condition, &Document::Item::setCondition,
                                              BrickLink::Condition::New, BrickLink::Condition::Used);
}

void Window::on_edit_subcond_none_triggered()
{
    setOrToggle<BrickLink::SubCondition>::set(this, QT_TR_N_NOOP("Set 'none' sub-condition on %n item(s)"),
                                              &Document::Item::subCondition, &Document::Item::setSubCondition,
                                              BrickLink::SubCondition::None);
}

void Window::on_edit_subcond_sealed_triggered()
{
    setOrToggle<BrickLink::SubCondition>::set(this, QT_TR_N_NOOP("Set 'sealed' sub-condition on %n item(s)"),
                                              &Document::Item::subCondition, &Document::Item::setSubCondition,
                                              BrickLink::SubCondition::Sealed);
}

void Window::on_edit_subcond_complete_triggered()
{
    setOrToggle<BrickLink::SubCondition>::set(this, QT_TR_N_NOOP("Set 'complete' sub-condition on %n item(s)"),
                                              &Document::Item::subCondition, &Document::Item::setSubCondition,
                                              BrickLink::SubCondition::Complete);
}

void Window::on_edit_subcond_incomplete_triggered()
{
    setOrToggle<BrickLink::SubCondition>::set(this, QT_TR_N_NOOP("Set 'incomplete' sub-condition on %n item(s)"),
                                              &Document::Item::subCondition, &Document::Item::setSubCondition,
                                              BrickLink::SubCondition::Incomplete);
}

void Window::on_edit_retain_yes_triggered()
{
    setOrToggle<bool>::set(this, QT_TR_N_NOOP("Set 'retain' flag on %n item(s)"),
                           &Document::Item::retain, &Document::Item::setRetain, true);
}

void Window::on_edit_retain_no_triggered()
{
    setOrToggle<bool>::set(this, QT_TR_N_NOOP("Cleared 'retain' flag on %n item(s)"),
                           &Document::Item::retain, &Document::Item::setRetain, false);
}

void Window::on_edit_retain_toggle_triggered()
{
    setOrToggle<bool>::toggle(this, QT_TR_N_NOOP("Toggled 'retain' flag on %n item(s)"),
                              &Document::Item::retain, &Document::Item::setRetain, true, false);
}

void Window::on_edit_stockroom_no_triggered()
{
    setOrToggle<BrickLink::Stockroom>::set(this, QT_TR_N_NOOP("Cleared 'stockroom' flag on %n item(s)"),
                                           &Document::Item::stockroom, &Document::Item::setStockroom,
                                           BrickLink::Stockroom::None);
}

void Window::on_edit_stockroom_a_triggered()
{
    setOrToggle<BrickLink::Stockroom>::set(this, QT_TR_N_NOOP("Set stockroom to 'A' on %n item(s)"),
                                           &Document::Item::stockroom, &Document::Item::setStockroom,
                                           BrickLink::Stockroom::A);
}

void Window::on_edit_stockroom_b_triggered()
{
    setOrToggle<BrickLink::Stockroom>::set(this, QT_TR_N_NOOP("Set stockroom to 'B' on %n item(s)"),
                                           &Document::Item::stockroom, &Document::Item::setStockroom,
                                           BrickLink::Stockroom::B);
}

void Window::on_edit_stockroom_c_triggered()
{
    setOrToggle<BrickLink::Stockroom>::set(this, QT_TR_N_NOOP("Set stockroom to 'C' on %n item(s)"),
                                           &Document::Item::stockroom, &Document::Item::setStockroom,
                                           BrickLink::Stockroom::C);
}


void Window::on_edit_price_set_triggered()
{
    if (selection().isEmpty())
        return;

    double price = selection().front()->price();

    if (MessageBox::getDouble(this, { }, tr("Enter the new price for all selected items:"),
                              m_doc->currencyCode(), price, 0, FrameWork::maxPrice, 3)) {
        setOrToggle<double>::set(this, QT_TR_N_NOOP("Set price on %n item(s)"),
                                 &Document::Item::price, &Document::Item::setPrice, price);
    }
}

void Window::on_edit_price_round_triggered()
{
    if (selection().isEmpty())
        return;

    int roundcount = 0;
    m_doc->beginMacro();

    WindowProgress wp(w_list);

    foreach(Document::Item *pos, selection()) {
        double p = int(pos->price() * 100 + .5) / 100.;

        if (!qFuzzyCompare(p, pos->price())) {
            Document::Item item = *pos;

            item.setPrice(p);
            m_doc->changeItem(pos, item);

            roundcount++;
        }
    }
    m_doc->endMacro(tr("Price change on %n item(s)", nullptr, roundcount));
}


void Window::on_edit_price_to_priceguide_triggered()
{
    if (selection().isEmpty())
        return;

    if (m_settopg_list || m_settopg_todocnt) {
        MessageBox::information(this, { }, tr("Prices are currently updated to Price Guide values.<br /><br />Please wait until this operation has finished."));
        return;
    }

    SetToPriceGuideDialog dlg(this);

    if (dlg.exec() == QDialog::Accepted) {
        const auto sel = selection();

        WindowProgress wp(w_list, tr("Verifying Price Guide data"), sel.count());

        m_settopg_list    = new QMultiHash<BrickLink::PriceGuide *, Document::Item *>();
        m_settopg_failcnt = 0;
        m_settopg_todocnt = sel.count();
        m_settopg_time    = dlg.time();
        m_settopg_price   = dlg.price();
        bool force_update = dlg.forceUpdate();

        for (Document::Item *item : sel) {
            BrickLink::PriceGuide *pg = BrickLink::core()->priceGuide(item->item(), item->color());

            if (force_update && pg && (pg->updateStatus() != BrickLink::UpdateStatus::Updating))
                pg->update();

            if (pg && (pg->updateStatus() == BrickLink::UpdateStatus::Updating)) {
                m_settopg_list->insert(pg, item);
                pg->addRef();
            }
            else if (pg && pg->valid()) {
                double p = pg->price(m_settopg_time, item->condition(), m_settopg_price);

                p *= Currency::inst()->rate(m_doc->currencyCode());

                if (!qFuzzyCompare(p, item->price())) {
                    Document::Item newitem = *item;
                    newitem.setPrice(p);
                    m_doc->changeItem(item, newitem);
                }
                --m_settopg_todocnt;
            }
            else {
                Document::Item newitem = *item;
                newitem.setPrice(0);
                m_doc->changeItem(item, newitem);

                ++m_settopg_failcnt;
                --m_settopg_todocnt;
            }
            wp.step();
        }
        wp.stop();

        if (m_settopg_list && m_settopg_list->isEmpty())
            priceGuideUpdated(nullptr);
    }
}

void Window::priceGuideUpdated(BrickLink::PriceGuide *pg)
{
    if (m_settopg_list && pg) {
        double crate = Currency::inst()->rate(m_doc->currencyCode());

        const auto items = m_settopg_list->values(pg);
        for (auto item : items) {
            double p = pg->valid() ? pg->price(m_settopg_time, item->condition(), m_settopg_price)
                                   : 0;
            p *= crate;
            if (!qFuzzyCompare(p, item->price())) {
                Document::Item newitem = *item;
                newitem.setPrice(p);
                m_doc->changeItem(item, newitem);
            }
            pg->release();
            --m_settopg_todocnt;
        }
        m_settopg_list->remove(pg);
    }

    if (m_settopg_list && m_settopg_list->isEmpty() && !m_settopg_todocnt) {
        int fails = m_settopg_failcnt;

        m_settopg_failcnt = 0;
        delete m_settopg_list;
        m_settopg_list = nullptr;

        QString s = tr("Prices of the selected items have been updated to Price Guide values.");
        if (fails) {
            s += "<br /><br />" + tr("%1 have been skipped, because of missing Price Guide records and/or network errors.")
                    .arg(CMB_BOLD(QString::number(fails)));
        }

        MessageBox::information(this, { }, s);

    }
}


void Window::on_edit_price_inc_dec_triggered()
{
    if (selection().isEmpty())
        return;

    IncDecPricesDialog dlg(!isSimpleMode(), m_doc->currencyCode(), this);

    if (dlg.exec() == QDialog::Accepted) {
        double fixed     = dlg.fixed();
        double percent   = dlg.percent();
        double factor    = (1.+ percent / 100.);
        bool tiers       = dlg.applyToTiers();
        int incdeccount = 0;

        m_doc->beginMacro();

        WindowProgress wp(w_list);

        foreach(Document::Item *pos, selection()) {
            Document::Item item = *pos;

            double p = item.price();

            if (!qFuzzyIsNull(percent))
                p *= factor;
            else if (!qFuzzyIsNull(fixed))
                p += fixed;

            item.setPrice(p);

            if (tiers) {
                for (int i = 0; i < 3; i++) {
                    p = item.tierPrice(i);

                    if (!qFuzzyIsNull(percent))
                        p *= factor;
                    else if (!qFuzzyIsNull(fixed))
                        p += fixed;

                    item.setTierPrice(i, p);
                }
            }
            m_doc->changeItem(pos, item);
            incdeccount++;
        }

        m_doc->endMacro(tr("Price change on %n item(s)", nullptr, incdeccount));
    }
}

void Window::on_edit_cost_set_triggered()
{
    if (selection().isEmpty())
        return;

    double cost = selection().front()->cost();

    if (MessageBox::getDouble(this, { }, tr("Enter the new cost for all selected items:"),
                              m_doc->currencyCode(), cost, 0, FrameWork::maxPrice, 3)) {
        setOrToggle<double>::set(this, QT_TR_N_NOOP("Set cost on %n item(s)"),
                                 &Document::Item::cost, &Document::Item::setCost, cost);
    }
}

void Window::on_edit_cost_round_triggered()
{
    if (selection().isEmpty())
        return;

    int roundcount = 0;
    m_doc->beginMacro();

    WindowProgress wp(w_list);

    foreach(Document::Item *pos, selection()) {
        double c = int(pos->cost() * 100 + .5) / 100.;

        if (!qFuzzyCompare(c, pos->cost())) {
            Document::Item item = *pos;

            item.setCost(c);
            m_doc->changeItem(pos, item);

            roundcount++;
        }
    }
    m_doc->endMacro(tr("Cost change on %n item(s)", nullptr, roundcount));
}

void Window::on_edit_cost_inc_dec_triggered()
{
    if (selection().isEmpty())
        return;

    IncDecPricesDialog dlg(false, m_doc->currencyCode(), this);

    if (dlg.exec() == QDialog::Accepted) {
        double fixed     = dlg.fixed();
        double percent   = dlg.percent();
        double factor    = (1.+ percent / 100.);
        int incdeccount = 0;

        m_doc->beginMacro();

        WindowProgress wp(w_list);

        foreach(Document::Item *pos, selection()) {
            Document::Item item = *pos;

            double c = item.cost();

            if (!qFuzzyIsNull(percent))
                c *= factor;
            else if (!qFuzzyIsNull(fixed))
                c += fixed;

            item.setCost(c);

            m_doc->changeItem(pos, item);
            incdeccount++;
        }

        m_doc->endMacro(tr("Cost change on %n item(s)", nullptr, incdeccount));
    }
}

void Window::on_edit_cost_spread_triggered()
{
    if (selection().size() < 2)
        return;

    double spreadAmount = 0;

    if (MessageBox::getDouble(this, { }, tr("Enter the cost amount to spread over all the selected items:"),
                              m_doc->currencyCode(), spreadAmount, 0, FrameWork::maxPrice, 3)) {
        int spreadcount = 0;
        double priceTotal = 0;

        foreach (Document::Item *item, selection())
            priceTotal += (item->price() * item->quantity());

        if (qFuzzyIsNull(priceTotal))
            return;
        double f = spreadAmount / priceTotal;

        m_doc->beginMacro();

        foreach (Document::Item *item, selection()) {
            Document::Item newItem(*item);
            newItem.setCost(item->price() * f);
            m_doc->changeItem(item, newItem);

            spreadcount++;
        }

        m_doc->endMacro(tr("Cost spreaded over %n item(s)", nullptr, spreadcount));
    }
}

void Window::on_edit_qty_divide_triggered()
{
    if (selection().isEmpty())
        return;

    int divisor = 1;

    if (MessageBox::getInteger(this, { },
                               tr("Divide the quantities of all selected items by this number.<br /><br />(A check is made if all quantites are exactly divisible without reminder, before this operation is performed.)"),
                               QString(), divisor, 1, 1000)) {
        if (divisor > 1) {
            int lots_with_errors = 0;

            foreach(Document::Item *item, selection()) {
                if (qAbs(item->quantity()) % divisor)
                    lots_with_errors++;
            }

            if (lots_with_errors) {
                MessageBox::information(this, { },
                                        tr("The quantities of %n lot(s) are not divisible without remainder by %1.<br /><br />Nothing has been modified.",
                                           nullptr, lots_with_errors).arg(divisor));
            }
            else {
                int divcount = 0;
                m_doc->beginMacro();

                WindowProgress wp(w_list);

                foreach(Document::Item *pos, selection()) {
                    Document::Item item = *pos;

                    item.setQuantity(item.quantity() / divisor);
                    m_doc->changeItem(pos, item);

                    divcount++;
                }
                m_doc->endMacro(tr("Quantity divide by %1 on %n item(s)", nullptr, divcount).arg(divisor));
            }
        }
    }
}

void Window::on_edit_qty_multiply_triggered()
{
    if (selection().isEmpty())
        return;

    int factor = 1;

    if (MessageBox::getInteger(this, { }, tr("Multiply the quantities of all selected items with this factor."),
                               tr("x"), factor, -1000, 1000)) {
        if ((factor <= -1) || (factor > 1)) {
            int lots_with_errors = 0;
            int maxQty = FrameWork::maxQuantity / qAbs(factor);

            foreach(Document::Item *item, selection()) {
                if (qAbs(item->quantity()) > maxQty)
                    lots_with_errors++;
            }

            if (lots_with_errors) {
                MessageBox::information(this, { },
                                        tr("The quantities of %n lot(s) will exceed the maximum allowed value (%2) when multiplied by %1.<br /><br />Nothing has been modified.",
                                           nullptr, lots_with_errors).arg(factor).arg(FrameWork::maxQuantity));
            } else {
                int mulcount = 0;
                m_doc->beginMacro();

                WindowProgress wp(w_list);

                foreach(Document::Item *pos, selection()) {
                    Document::Item item = *pos;

                    item.setQuantity(item.quantity() * factor);
                    m_doc->changeItem(pos, item);

                    mulcount++;
                }
                m_doc->endMacro(tr("Quantity multiply by %1 on %n item(s)", nullptr, mulcount).arg(factor));
            }
        }
    }
}

void Window::on_edit_sale_triggered()
{
    if (selection().isEmpty())
        return;

    int sale = selection().front()->sale();

    if (MessageBox::getInteger(this, { }, tr("Set sale in percent for the selected items (this will <u>not</u> change any prices).<br />Negative values are also allowed."),
                               tr("%"), sale, -1000, 99)) {
        setOrToggle<int>::set(this, QT_TR_N_NOOP("Set sale on %n item(s)"),
                              &Document::Item::sale, &Document::Item::setSale, sale);
    }
}

void Window::on_edit_bulk_triggered()
{
    if (selection().isEmpty())
        return;

    int bulk = selection().front()->bulkQuantity();

    if (MessageBox::getInteger(this, { }, tr("Set bulk quantity for the selected items:"),
                               QString(), bulk, 1, 99999)) {
        setOrToggle<int>::set(this, QT_TR_N_NOOP("Set bulk quantity on %n item(s)"),
                              &Document::Item::bulkQuantity, &Document::Item::setBulkQuantity, bulk);
    }
}

void Window::on_edit_color_triggered()
{
    if (selection().isEmpty())
        return;

    SelectColorDialog d(this);
    d.setWindowTitle(tr("Modify Color"));
    d.setColor(selection().front()->color());

    if (d.exec() == QDialog::Accepted) {
        setOrToggle<const BrickLink::Color *>::set(this, QT_TR_N_NOOP( "Set color on %n item(s)" ),
                                                   &Document::Item::color, &Document::Item::setColor,
                                                   d.color());
    }
}

void Window::on_edit_qty_set_triggered()
{
    if (selection().isEmpty())
        return;

    int quantity = selection().front()->quantity();

    if (MessageBox::getInteger(this, { }, tr("Enter the new quantities for all selected items:"),
                               QString(), quantity, -FrameWork::maxQuantity, FrameWork::maxQuantity)) {
        setOrToggle<int>::set(this, QT_TR_N_NOOP("Set quantity on %n item(s)"),
                              &Document::Item::quantity, &Document::Item::setQuantity, quantity);
    }
}

void Window::on_edit_remark_set_triggered()
{
    if (selection().isEmpty())
        return;

    QString remarks = selection().front()->remarks();

    if (MessageBox::getString(this, { }, tr("Enter the new remark for all selected items:"),
                              remarks)) {
        setOrToggle<QString, const QString &>::set(this, QT_TR_N_NOOP("Set remark on %n item(s)"),
                                                   &Document::Item::remarks, &Document::Item::setRemarks,
                                                   remarks);
    }
}

void Window::on_edit_remark_clear_triggered()
{
    if (selection().isEmpty())
        return;

    setOrToggle<QString, const QString &>::set(this, QT_TR_N_NOOP("Clear remark on %n item(s)"),
                                               &Document::Item::remarks, &Document::Item::setRemarks,
                                               QString { });
}

void Window::on_edit_remark_add_triggered()
{
    if (selection().isEmpty())
        return;

    QString addremarks;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be added to the remarks of all selected items:"),
                              addremarks)) {
        int remarkcount = 0;
        m_doc->beginMacro();

        QRegularExpression regexp("\\b" + QRegularExpression::escape(addremarks) + "\\b");

        WindowProgress wp(w_list, tr("Changing remarks"), selection().count());

        foreach(Document::Item *pos, selection()) {
            QString str = pos->remarks();

            if (str.isEmpty())
                str = addremarks;
            else if (str.indexOf(regexp) != -1)
                ;
            else if (addremarks.indexOf(QRegularExpression("\\b" + QRegularExpression::escape(str) + "\\b")) != -1)
                str = addremarks;
            else
                str = str + " " + addremarks;

            if (str != pos->remarks()) {
                Document::Item item = *pos;

                item.setRemarks(str);
                m_doc->changeItem(pos, item);

                remarkcount++;
            }
            wp.step();
        }
        m_doc->endMacro(tr("Modified remark on %n item(s)", nullptr, remarkcount));
    }
}

void Window::on_edit_remark_rem_triggered()
{
    if (selection().isEmpty())
        return;

    QString remremarks;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be removed from the remarks of all selected items:"),
                              remremarks)) {
        int remarkcount = 0;
        m_doc->beginMacro();

        QRegularExpression regexp("\\b" + QRegularExpression::escape(remremarks) + "\\b");

        WindowProgress wp(w_list, tr("Changing remarks"), selection().count());

        foreach(Document::Item *pos, selection()) {
            QString str = pos->remarks();
            str.remove(regexp);
            str = str.simplified();

            if (str != pos->remarks()) {
                Document::Item item = *pos;

                item.setRemarks(str);
                m_doc->changeItem(pos, item);

                remarkcount++;
            }
            wp.step();
        }
        m_doc->endMacro(tr("Modified remark on %n item(s)", nullptr, remarkcount));
    }
}


void Window::on_edit_comment_set_triggered()
{
    if (selection().isEmpty())
        return;

    QString comments = selection().front()->comments();

    if (MessageBox::getString(this, { }, tr("Enter the new comment for all selected items:"), comments))
        setOrToggle<QString, const QString &>::set(this, QT_TR_N_NOOP("Set comment on %n item(s)"),
                                                   &Document::Item::comments, &Document::Item::setComments,
                                                   comments);
}

void Window::on_edit_comment_clear_triggered()
{
    if (selection().isEmpty())
        return;

    setOrToggle<QString, const QString &>::set(this, QT_TR_N_NOOP("Clear comment on %n item(s)"),
                                               &Document::Item::comments, &Document::Item::setComments,
                                               QString { });
}

void Window::on_edit_comment_add_triggered()
{
    if (selection().isEmpty())
        return;

    QString addcomments;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be added to the comments of all selected items:"),
                              addcomments)) {
        int commentcount = 0;
        m_doc->beginMacro();

        QRegularExpression regexp("\\b" + QRegularExpression::escape(addcomments) + "\\b");

        WindowProgress wp(w_list, tr("Changing comments"), selection().count());

        foreach(Document::Item *pos, selection()) {
            QString str = pos->comments();

            if (str.isEmpty())
                str = addcomments;
            else if (str.indexOf(regexp) != -1)
                ;
            else if (addcomments.indexOf(QRegularExpression("\\b" + QRegularExpression::escape(str) + "\\b")) != -1)
                str = addcomments;
            else
                str = str + " " + addcomments;

            if (str != pos->comments()) {
                Document::Item item = *pos;

                item.setComments(str);
                m_doc->changeItem(pos, item);

                commentcount++;
            }
            wp.step();
        }
        m_doc->endMacro(tr("Modified comment on %n item(s)", nullptr, commentcount));
    }
}

void Window::on_edit_comment_rem_triggered()
{
    if (selection().isEmpty())
        return;

    QString remcomments;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be removed from the comments of all selected items:"),
                              remcomments)) {
        int commentcount = 0;
        m_doc->beginMacro();

        QRegularExpression regexp("\\b" + QRegularExpression::escape(remcomments) + "\\b");

        WindowProgress wp(w_list, tr("Changing comments"), selection().count());

        foreach(Document::Item *pos, selection()) {
            QString str = pos->comments();
            str.remove(regexp);
            str = str.simplified();

            if (str != pos->comments()) {
                Document::Item item = *pos;

                item.setComments(str);
                m_doc->changeItem(pos, item);

                commentcount++;
            }
            wp.step();
        }
        m_doc->endMacro(tr("Modified comment on %n item(s)", nullptr, commentcount));
    }
}


void Window::on_edit_reserved_triggered()
{
    if (selection().isEmpty())
        return;

    QString reserved = selection().front()->reserved();

    if (MessageBox::getString(this, { },
                              tr("Reserve all selected items for this specific buyer (BrickLink username):"),
                              reserved)) {
        setOrToggle<QString, const QString &>::set(this, QT_TR_N_NOOP("Set reservation on %n item(s)"),
                                                   &Document::Item::reserved, &Document::Item::setReserved,
                                                   reserved);
    }
}

void Window::updateErrorMask()
{
    quint64 em = 0;

    if (Config::inst()->showInputErrors())
        em = (1ULL << Document::FieldCount) - 1;

    m_doc->setErrorMask(em);
}

void Window::on_edit_copyremarks_triggered()
{
    SelectDocumentDialog dlg(document(), tr("Please choose the document that should serve as a source to fill in the remarks fields of the current document:"));

    if (dlg.exec() == QDialog::Accepted ) {
        BrickLink::InvItemList list = dlg.items();

        if (!list.isEmpty())
            copyRemarks(list);

        qDeleteAll(list);
    }
}

void Window::copyRemarks(const BrickLink::InvItemList &items)
{
    if (items.isEmpty())
        return;

    m_doc->beginMacro();

    WindowProgress wp(w_list);

    int copy_count = 0;

    foreach(Document::Item *pos, m_doc->items()) {
        BrickLink::InvItem *match = nullptr;

        foreach(BrickLink::InvItem *ii, items) {
            const BrickLink::Item *item   = ii->item();
            const BrickLink::Color *color = ii->color();
            BrickLink::Condition cond     = ii->condition();
            int qty                       = ii->quantity();

            if (!item || !color || !qty)
                continue;

            if ((pos->item() == item) && (pos->condition() == cond) && (pos->color() == color))
                match = ii;
        }

        if (match && !match->remarks().isEmpty()) {
            Document::Item newitem = *pos;
            newitem.setRemarks(match->remarks());
            m_doc->changeItem(pos, newitem);

            copy_count++;
        }
    }
    m_doc->endMacro(tr("Copied Remarks for %n item(s)", nullptr, copy_count));
}

void Window::on_edit_subtractitems_triggered()
{
    SelectDocumentDialog dlg(document(), tr("Which items should be subtracted from the current document:"));

    if (dlg.exec() == QDialog::Accepted ) {
        BrickLink::InvItemList list = dlg.items();

        if (!list.isEmpty())
            subtractItems(list);

        qDeleteAll(list);
    }
}

void Window::subtractItems(const BrickLink::InvItemList &items)
{
    if (items.isEmpty())
        return;

    m_doc->beginMacro();

    WindowProgress wp(w_list, tr("Subtracting items"), items.count() * m_doc->items().count());

    foreach(BrickLink::InvItem *ii, items) {
        const BrickLink::Item *item   = ii->item();
        const BrickLink::Color *color = ii->color();
        BrickLink::Condition cond     = ii->condition();
        int qty                       = ii->quantity();

        if (!item || !color || !qty) {
            wp.step(m_doc->items().count());
            continue;
        }

        Document::Item *last_match = nullptr;

        foreach(Document::Item *pos, m_doc->items()) {
            if ((pos->item() == item) && (pos->color() == color) && (pos->condition() == cond)) {
                Document::Item newitem = *pos;

                if (pos->quantity() >= qty) {
                    newitem.setQuantity(pos->quantity() - qty);
                    qty = 0;
                }
                else {
                    qty -= pos->quantity();
                    newitem.setQuantity(0);
                }
                m_doc->changeItem(pos, newitem);
                last_match = pos;
            }
            wp.step();
        }

        if (qty) {   // still a qty left
            if (last_match) {
                Document::Item newitem = *last_match;
                newitem.setQuantity(last_match->quantity() - qty);
                m_doc->changeItem(last_match, newitem);
            }
            else {
                auto *newitem = new Document::Item();
                newitem->setItem(item);
                newitem->setColor(color);
                newitem->setCondition(cond);
                ///////newitem->setOrigQuantity(0);
                newitem->setQuantity(-qty);

                m_doc->appendItem(newitem);
            }
        }
    }
    m_doc->endMacro(tr("Subtracted %n item(s)", nullptr, items.count()));
}

void Window::on_edit_mergeitems_triggered()
{
    if (!selection().isEmpty())
        consolidateItems(selection());
    else
        consolidateItems(m_doc->items());
}

void Window::on_edit_partoutitems_triggered()
{
    if (selection().count() >= 1) {
        bool inplace = false;

        switch (MessageBox::question(this, { },
                                     tr("Should the selected items be parted out into the current document, replacing the selected items?"),
                                     QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                     QMessageBox::Yes)) {
        case QMessageBox::Cancel:
            return;
        case QMessageBox::Yes:
            inplace = true;
            break;
        default:
            break;
        }

        if (inplace)
            m_doc->beginMacro();

        int partcount = 0;

        foreach(Document::Item *item, selection()) {
            if (inplace) {
                if (item->item()->hasInventory() && item->quantity()) {
                    BrickLink::InvItemList items = item->item()->consistsOf();
                    if (!items.isEmpty()) {
                        int multiply = item->quantity();

                        for (int i = 0; i < items.size(); ++i) {
                            if (multiply != 1)
                                items[i]->setQuantity(items[i]->quantity() * multiply);
                            items[i]->setCondition(item->condition());
                        }
                        m_doc->insertItemsAfter(item, Document::ItemList(items));
                        m_doc->removeItem(item);
                        partcount++;
                    }
                }
            } else {
                FrameWork::inst()->fileImportBrickLinkInventory(item->item(), item->quantity(), item->condition());
            }
        }
        if (inplace)
            m_doc->endMacro(tr("Parted out %n item(s)", nullptr, partcount));
    }
    else
        QApplication::beep();
}

void Window::setMatchProgress(int /*pmax*/, int /*pcur*/)
{
    fputc('.', stderr);
}

void Window::setMatchFinished(QVector<const BrickLink::Item *> result)
{
    foreach (const BrickLink::Item *item, result)
        qWarning() << "SetMatch:" << item->name() << item->id();
}

void Window::on_edit_setmatch_triggered()
{
    if (!m_doc->items().isEmpty()) {
        BrickLink::SetMatch *sm = new BrickLink::SetMatch(m_doc);

        sm->setGreedyPreference(BrickLink::SetMatch::PreferLargerSets);

        sm->setItemTypeConstraint(QVector<const BrickLink::ItemType *>() << BrickLink::core()->itemType('S'));
        sm->setPartCountConstraint(100, -1);
        sm->setYearReleasedConstraint(1995, -1);

        connect(sm, &BrickLink::SetMatch::finished,
                this, &Window::setMatchFinished);
        connect(sm, &BrickLink::SetMatch::progress,
                this, &Window::setMatchProgress);

        if (!sm->startMaximumPossibleSetMatch(selection().isEmpty() ? m_doc->items()
                                              : selection(), BrickLink::SetMatch::Greedy))
            qWarning("SetMatch (Recursive): error.");
    } else {
        QApplication::beep();
    }
}

void Window::setPrice(double d)
{
    if (selection().count() == 1) {
        Document::Item *pos = selection().front();
        Document::Item item = *pos;

        item.setPrice(d);
        m_doc->changeItem(pos, item);
    }
    else
        QApplication::beep();
}

void Window::gotoNextError()
{
    auto startIdx = m_selection_model->currentIndex();
    if (!startIdx.isValid())
        startIdx = m_doc->index(0, 0);
    bool skipFirst = (startIdx.row() != 0) || (startIdx.column() != 0);

    bool wrapped = false;
    int startCol = startIdx.column();

    for (int row = startIdx.row(); row < m_doc->rowCount(); ) {
        if (wrapped && (row > startIdx.row()))
            return;

        auto error = m_doc->itemFlags(m_doc->item(m_doc->index(row, 0))).first;
        if (error) {
            for (int col = startCol; col < m_doc->columnCount(); ++col) {
                if (wrapped && (row == startIdx.row()) && (col > startIdx.column()))
                    return;

                if (error & (1ULL << col)) {
                    if (skipFirst) {
                        skipFirst = false;
                    } else {
                        auto gotoIdx = m_doc->index(row, col);
                        m_selection_model->setCurrentIndex(gotoIdx, QItemSelectionModel::SelectCurrent
                                                           | QItemSelectionModel::Rows);
                        w_list->scrollTo(gotoIdx, QAbstractItemView::PositionAtCenter);
                        return;
                    }
                }
            }
        }
        startCol = 0;

        ++row;
        if (row >= m_doc->rowCount()) { // wrap around
            row = 0;
            wrapped = true;
        }
    }
}

void Window::contextMenu(const QPoint &p)
{
    FrameWork::inst()->showContextMenu(true, w_list->viewport()->mapToGlobal(p));
}

void Window::on_document_close_triggered()
{
    close();
}

void Window::closeEvent(QCloseEvent *e)
{
    bool close_empty = (m_doc->items().isEmpty() && Config::inst()->closeEmptyDocuments());

    if (m_doc->isModified() && !close_empty) {
        FrameWork::inst()->setActiveWindow(this);

        QMessageBox msgBox(this);
        msgBox.setText(tr("The document %1 has been modified.").arg(CMB_BOLD(windowTitle().replace(QLatin1String("[*]"), QString()))));
        msgBox.setInformativeText(tr("Do you want to save your changes?"));
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        msgBox.setWindowModality(Qt::WindowModal);

        switch (msgBox.exec()) {
        case MessageBox::Save:
            on_document_save_triggered();

            if (!m_doc->isModified())
                e->accept();
            else
                e->ignore();
            break;

        case MessageBox::Discard:
            e->accept();
            break;

        default:
            e->ignore();
            break;
        }
    }
    else {
        e->accept();
    }

}

void Window::on_bricklink_catalog_triggered()
{
    if (!selection().isEmpty())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_CatalogInfo, (*selection().front()).item(), (*selection().front()).color()));
}

void Window::on_bricklink_priceguide_triggered()
{
    if (!selection().isEmpty())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_PriceGuideInfo, (*selection().front()).item(), (*selection().front()).color()));
}

void Window::on_bricklink_lotsforsale_triggered()
{
    if (!selection().isEmpty())
        QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_LotsForSale, (*selection().front()).item(), (*selection().front()).color()));
}

void Window::on_bricklink_myinventory_triggered()
{
    if (!selection().isEmpty()) {
        uint lotid = (*selection().front()).lotId();
        if (lotid)
            QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_StoreItemDetail, &lotid));
        else
            QDesktopServices::openUrl(BrickLink::core()->url(BrickLink::URL_StoreItemSearch, (*selection().front()).item(), (*selection().front()).color()));
    }
}

void Window::on_view_column_layout_save_triggered()
{
    QString name;

    if (MessageBox::getString(this, { },
                              tr("Enter an unique name for this column layout. Leave empty to change the user default layout."),
                              name)) {
        QString layoutId;
        if (name.isEmpty()) {
            layoutId = "user-default";
        } else {
            const auto allIds = Config::inst()->columnLayoutIds();
            for (const auto &id : allIds) {
                if (Config::inst()->columnLayoutName(id) == name) {
                    layoutId = id;
                    break;
                }
            }
        }

        QString newId = Config::inst()->setColumnLayout(layoutId, w_header->saveLayout());
        if (layoutId.isEmpty() && !newId.isEmpty())
            Config::inst()->renameColumnLayout(newId, name);
    }
}

void Window::on_view_column_layout_list_load(const QString &layoutId)
{
    QString cmdName;
    if (layoutId == "default")
        cmdName = tr("Set column layout to BrickStore defaults");
    else if (layoutId == "user-default")
        cmdName = tr("Set column layout to user defaults");
    else if (layoutId == "auto-resize")
        cmdName = tr("Auto resized all columns");
    else
        cmdName = tr("Loaded column layout %1").arg(Config::inst()->columnLayoutName(layoutId));

    document()->undoStack()->beginMacro(cmdName);

    if (layoutId == "default") {
        resizeColumnsToDefault();
        w_list->sortByColumn(0, Qt::AscendingOrder);
    } else if (layoutId == "auto-resize") {
        w_list->resizeColumnsToContents();
    } else {
        auto layout = Config::inst()->columnLayout(layoutId);
        if (!layout.isEmpty()) {
            w_header->restoreLayout(layout);
            w_list->sortByColumn(w_header->sortIndicatorSection(), w_header->sortIndicatorOrder());
        } else if (layoutId == "user-default") {
            on_view_column_layout_list_load(QLatin1String("default"));
        }
    }
    document()->undoStack()->endMacro();
}

void Window::on_document_print_triggered()
{
    print(false);
}

void Window::on_document_print_pdf_triggered()
{
    print(true);
}

void Window::print(bool as_pdf)
{
    if (m_doc->items().isEmpty())
        return;

    if (ScriptManager::inst()->printingScripts().isEmpty()) {
        ScriptManager::inst()->reload();

        if (ScriptManager::inst()->printingScripts().isEmpty()) {
            MessageBox::warning(this, { }, tr("Couldn't find any print scripts."));
            return;
        }
    }

    QString doctitle = m_doc->fileNameOrTitle();

    QPrinter prt(QPrinter::HighResolution);

    if (as_pdf) {
        QString pdfname = doctitle + QLatin1String(".pdf");

        QDir d(Config::inst()->documentDir());
        if (d.exists())
            pdfname = d.filePath(pdfname);

        pdfname = QFileDialog::getSaveFileName(this, tr("Save PDF as"), pdfname, tr("PDF Documents (*.pdf)"));

        if (pdfname.isEmpty())
            return;

        // check if the file is actually writable - otherwise QPainter::begin() will fail
        if (QFile::exists(pdfname) && !QFile(pdfname).open(QFile::ReadWrite)) {
            MessageBox::warning(this, { }, tr("The PDF document already exists and is not writable."));
            return;
        }

        prt.setOutputFileName(pdfname);
        prt.setOutputFormat(QPrinter::PdfFormat);
    }
    else {
        QPrintDialog pd(&prt, FrameWork::inst());

        pd.setOption(QAbstractPrintDialog::PrintToFile);
        pd.setOption(QAbstractPrintDialog::PrintCollateCopies);
        pd.setOption(QAbstractPrintDialog::PrintShowPageSize);

        if (!selection().isEmpty())
            pd.setOption(QAbstractPrintDialog::PrintSelection);

        pd.setPrintRange(selection().isEmpty() ? QAbstractPrintDialog::AllPages
                                               : QAbstractPrintDialog::Selection);

        if (pd.exec() != QDialog::Accepted)
            return;
    }

    prt.setDocName(doctitle);
    prt.setFullPage(true);

    SelectPrintingTemplateDialog d(this);

    if (d.exec() != QDialog::Accepted)
        return;

    PrintingScriptTemplate *prtTemplate = d.script();

    if (!prtTemplate)
        return;

    try {
        prtTemplate->executePrint(&prt, this, prt.printRange() == QPrinter::Selection);
    } catch (const Exception &e) {
        QString msg = e.error();
        if (msg.isEmpty())
            msg = tr("Printing failed.");
        else
            msg.replace(QChar('\n'), QLatin1String("<br>"));
        MessageBox::warning(nullptr, { }, msg);
    }
}

void Window::on_document_save_triggered()
{
    m_doc->setGuiState(createGuiStateXML());
    DocumentIO::save(m_doc);
    m_doc->clearGuiState();
    deleteAutosave();
}

void Window::on_document_save_as_triggered()
{
    m_doc->setGuiState(createGuiStateXML());
    DocumentIO::saveAs(m_doc);
    m_doc->clearGuiState();
    deleteAutosave();
}

void Window::on_document_export_bl_xml_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        DocumentIO::exportBrickLinkXML(items);
}

void Window::on_document_export_bl_xml_clip_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        DocumentIO::exportBrickLinkXMLClipboard(items);
}

void Window::on_document_export_bl_update_clip_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        DocumentIO::exportBrickLinkUpdateClipboard(m_doc, items);
}

void Window::on_document_export_bl_invreq_clip_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        DocumentIO::exportBrickLinkInvReqClipboard(items);
}

void Window::on_document_export_bl_wantedlist_clip_triggered()
{
    Document::ItemList items = exportCheck();

    if (!items.isEmpty())
        DocumentIO::exportBrickLinkWantedListClipboard(items);
}

Document::ItemList Window::exportCheck() const
{
    Document::ItemList items = m_doc->items();

    if (!selection().isEmpty() && (selection().count() != m_doc->items().count())) {
        if (MessageBox::question(nullptr, { },
                                 tr("There are %n item(s) selected.<br /><br />Do you want to export only these items?",
                                    nullptr, selection().count()),
                                 MessageBox::Yes | MessageBox::No, MessageBox::Yes
                                 ) == MessageBox::Yes) {
            items = selection();
        }
    }

    if (m_doc->statistics(items).errors()) {
        if (MessageBox::warning(nullptr, { },
                                tr("This list contains items with errors.<br /><br />Do you really want to export this list?"),
                                MessageBox::Yes | MessageBox::No, MessageBox::No
                                ) != MessageBox::Yes) {
            items.clear();
        }
    }

    return m_doc->sortItemList(items);
}

void Window::resizeColumnsToDefault()
{
    int em = w_list->fontMetrics().averageCharWidth();
    for (int i = 0; i < w_list->model()->columnCount(); i++) {
        if (w_header->visualIndex(i) != i)
            w_header->moveSection(w_header->visualIndex(i), i);

        if (w_header->isSectionAvailable(i))
            w_header->setSectionHidden(i, false);

        int mw = w_list->model()->headerData(i, Qt::Horizontal, Qt::UserRole).toInt();
        int width = qMax((mw < 0 ? -mw : mw * em) + 8, w_header->sectionSizeHint(i));
        if (width)
            w_header->resizeSection(i, width);
    }
}

void Window::updateSelection()
{
    if (!m_delayedSelectionUpdate) {
        m_delayedSelectionUpdate = new QTimer(this);
        m_delayedSelectionUpdate->setSingleShot(true);
        m_delayedSelectionUpdate->setInterval(0);

        connect(m_delayedSelectionUpdate, &QTimer::timeout, this, [this]() {
            m_selection.clear();

            foreach (const QModelIndex &idx, m_selection_model->selectedRows())
                m_selection.append(m_doc->item(idx));

            emit selectionChanged(m_selection);

            if (m_selection_model->currentIndex().isValid())
                w_list->scrollTo(m_selection_model->currentIndex());
        });
    }
    m_delayedSelectionUpdate->start();
}

void Window::updateDifferenceMode()
{
    bool b = document()->isDifferenceModeActive();
    setWindowIcon(b ? QIcon::fromTheme("mode2") : QIcon());

    if (m_ccw)
        m_ccw->disable();
    w_header->setSectionAvailable(Document::PriceOrig, b);
    w_header->setSectionAvailable(Document::PriceDiff, b);
    w_header->setSectionAvailable(Document::QuantityOrig, b);
    w_header->setSectionAvailable(Document::QuantityDiff, b);
    if (m_ccw)
        m_ccw->enable();
}

void Window::setSelection(const Document::ItemList &lst)
{
    QItemSelection idxs;

    foreach (const Document::Item *item, lst) {
        QModelIndex idx(m_doc->index(item));
        idxs.select(idx, idx);
    }
    m_selection_model->select(idxs, QItemSelectionModel::Clear | QItemSelectionModel::Select
                              | QItemSelectionModel::Current | QItemSelectionModel::Rows);
}

void Window::documentItemsChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    for (int r = topLeft.row(); r <= bottomRight.row(); ++r) {
        auto item = m_doc->item(topLeft.siblingAtRow(r));
        if (m_selection.contains(item)) {
            emit selectionChanged(m_selection);
            break;
        }
    }
}


static const char *autosaveMagic = "||BRICKSTORE AUTOSAVE MAGIC||";
static const QString autosaveTemplate = QLatin1String("brickstore_%1.autosave");

void Window::deleteAutosave()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    QString filename = autosaveTemplate.arg(m_uuid.toString());
    temp.remove(filename);
}

void Window::moveColumnDirect(int /*logical*/, int oldVisual, int newVisual)
{
    w_header->moveSection(oldVisual, newVisual);
}

void Window::resizeColumnDirect(int logical, int /*oldSize*/, int newSize)
{
    w_header->resizeSection(logical, newSize);
}

class AutosaveJob : public QRunnable
{
public:
    explicit AutosaveJob(const QUuid &uuid, const QByteArray &contents)
        : QRunnable()
        , m_uuid(uuid)
        , m_contents(contents)
    { }

    void run() override;
private:
    const QUuid m_uuid;
    const QByteArray m_contents;
};

void AutosaveJob::run()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    QString fileName = autosaveTemplate.arg(m_uuid.toString());
    QString newFileName = fileName % u".new";

    { // reading is cheaper than writing, so check first
        QFile f(temp.filePath(fileName));
        if (f.open(QIODevice::ReadOnly)) {
            if (f.readAll() == m_contents)
                return;
        }
    }

    QFile f(temp.filePath(newFileName));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(m_contents);
        f.flush();
        f.close();

        temp.remove(fileName);
        if (!temp.rename(newFileName, fileName))
            qWarning() << "Autosave rename from" << newFileName << "to" << fileName << "failed";
    }
}


void Window::autosave() const
{
    auto doc = document();
    auto items = document()->items();

    if (m_uuid.isNull() || !doc->isModified() || items.isEmpty())
        return;

    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << QByteArray(autosaveMagic)
       << qint32(4) // version
       << doc->title()
       << doc->fileName()
       << doc->currencyCode()
       << currentColumnLayout()
       << doc->saveSortFilterState()
       << qint32(doc->items().count());

    for (auto item : items)
        item->save(ds);

    ds << QByteArray(autosaveMagic);

    QThreadPool::globalInstance()->start(new AutosaveJob(m_uuid, ba));
}

int Window::restorableAutosaves()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    return temp.entryList({ autosaveTemplate.arg("*") }).count();
}

const QVector<Window *> Window::processAutosaves(AutosaveAction action)
{
    QVector<Window *> restored;

    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    const QStringList ondisk = temp.entryList({ autosaveTemplate.arg("*") });

    for (const QString &filename : ondisk) {
        QFile f(temp.filePath(filename));
        if ((action == AutosaveAction::Restore) && f.open(QIODevice::ReadOnly)) {
            QByteArray magic;
            qint32 version;
            QString savedTitle;
            QString savedFileName;
            QString savedCurrencyCode;
            QByteArray savedColumnLayout;
            QByteArray savedSortFilterState;
            qint32 count = 0;

            QDataStream ds(&f);
            ds >> magic >> version >> savedTitle >> savedFileName >> savedCurrencyCode
                    >> savedColumnLayout >> savedSortFilterState >> count;

            if ((count > 0) && (magic == QByteArray(autosaveMagic)) && (version == 4)) {
                BrickLink::InvItemList items;

                for (int i = 0; i < count; ++i) {
                    if (auto item = BrickLink::InvItem::restore(ds))
                        items.append(item);
                }
                ds >> magic;

                if (magic == QByteArray(autosaveMagic)) {
                    QString restoredTag = tr("RESTORED", "Tag for document restored from autosave");

                    // Document owns the items now
                    auto doc = new Document(items, savedCurrencyCode, true /*mark as modified*/);
                    items.clear();

                    if (!savedFileName.isEmpty()) {
                        QFileInfo fi(savedFileName);
                        QString newFileName = fi.dir().filePath(restoredTag % u" " % fi.fileName());
                        doc->setFileName(newFileName);
                    } else {
                        doc->setTitle(restoredTag % u" " % savedTitle);
                    }
                    auto win = new Window(doc);
                    win->w_header->restoreLayout(savedColumnLayout);
                    doc->restoreSortFilterState(savedSortFilterState);

                    if (!savedFileName.isEmpty())
                        DocumentIO::save(doc);

                    restored.append(win);
                }
                qDeleteAll(items);
            }
            f.close();
        }
        f.remove();
    }
    return restored;
}

#include "moc_window.cpp"
