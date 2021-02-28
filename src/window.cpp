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
#include <QStringBuilder>
#include <QScrollBar>
#include <QMenu>
#include <QPainter>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDebug>

#include <QProgressBar>
#include <QStackedLayout>

#include "messagebox.h"
#include "config.h"
#include "framework.h"
#include "utility.h"
#include "currency.h"
#include "document.h"
#include "documentio.h"
#include "undo.h"
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
#include "selectitemdialog.h"
#include "selectdocumentdialog.h"
#include "settopriceguidedialog.h"
#include "incdecpricesdialog.h"
#include "consolidateitemsdialog.h"
#include "selectprintingtemplatedialog.h"

using namespace std::chrono_literals;


template <typename E>
static E nextEnumValue(E current, std::initializer_list<E> values)
{
    for (size_t i = 0; i < values.size(); ++i) {
        if (current == values.begin()[i])
            return values.begin()[(i + 1) % values.size()];
    }
    return current;
}


void Window::applyTo(const Document::ItemList &items, const char *undoText,
                     std::function<bool(const Document::Item &, Document::Item &)> callback)
{
    if (items.isEmpty())
        return;

    document()->beginMacro();

    int count = items.size();
    std::vector<std::pair<Document::Item *, Document::Item>> changes;
    changes.reserve(count);

    for (Document::Item *from : items) {
        Document::Item to = *from;
        if (callback(*from, to)) {
            changes.emplace_back(from, to);
        } else {
            --count;
        }
    }
    document()->changeItems(changes);
    document()->endMacro(Window::tr(undoText, nullptr, count));
}


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
    disable();
    if (oldSize && !newSize)
        m_header->hideSection(logical);
    else if (!oldSize && newSize)
        m_header->showSection(logical);
    if (newSize)
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
    else if (!oldValue || !newValue)
        setText(qApp->translate("ColumnCmd", "Show/hide column %1").arg(ccw->columnTitle(logical)));
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

    auto page = new QWidget();
    auto layout = new QHBoxLayout(page);
    layout->setContentsMargins(11, 4, 11, 4);

    auto addSeparator = [&layout]() -> QWidget * {
            auto sep = new QFrame();
            sep->setFrameShape(QFrame::VLine);
            sep->setFixedWidth(sep->frameWidth());
            sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
            layout->addWidget(sep);
            return sep;
    };

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

    m_differencesSeparator = addSeparator();
    m_differences = new QToolButton();
    m_differences->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_differences->setIcon(QIcon::fromTheme("vcs-locally-modified-small"));
    m_differences->setShortcut(tr("F5"));
    m_differences->setAutoRaise(true);
    connect(m_differences, &QToolButton::clicked,
            m_window, [this]() { m_window->gotoNextErrorOrDifference(true); });
    layout->addWidget(m_differences);

    m_errorsSeparator = addSeparator();
    m_errors = new QToolButton();
    m_errors->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_errors->setIcon(QIcon::fromTheme("vcs-conflicting-small"));
    m_errors->setShortcut(tr("F6"));
    m_errors->setAutoRaise(true);
    connect(m_errors, &QToolButton::clicked,
            m_window, [this]() { m_window->gotoNextErrorOrDifference(false); });
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

    auto blockPage = new QWidget();
    auto blockLayout = new QHBoxLayout(blockPage);
    blockLayout->setContentsMargins(11, 4, 11, 4);
    m_blockTitle = new QLabel();
    blockLayout->addWidget(m_blockTitle);
    m_blockProgress = new QProgressBar();
    blockLayout->addWidget(m_blockProgress, 1);
    m_blockCancel = new QToolButton();
    m_blockCancel->setIcon(QIcon::fromTheme("process-stop"));
    m_blockCancel->setAutoRaise(true);
    blockLayout->addWidget(m_blockCancel);

    m_stack = new QStackedLayout(this);
    m_stack->addWidget(page);
    m_stack->addWidget(blockPage);

    connect(m_currency, &QToolButton::triggered,
            this, &StatusBar::changeDocumentCurrency);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &StatusBar::updateCurrencyRates);
    connect(m_doc, &Document::currencyCodeChanged,
            this, &StatusBar::documentCurrencyChanged);
    connect(m_doc, &Document::statisticsChanged,
            this, &StatusBar::updateStatistics);
    connect(window, &Window::blockingOperationActiveChanged,
            this, &StatusBar::updateBlockState);
    connect(window, &Window::blockingOperationTitleChanged,
            m_blockTitle, &QLabel::setText);
    connect(window, &Window::blockingOperationCancelableChanged,
            m_blockCancel, &QWidget::setEnabled);
    connect(window, &Window::blockingOperationProgress,
            this, [this](int done, int total) {
        if (m_blockProgress->maximum() != total)
            m_blockProgress->setRange(0, total);
        m_blockProgress->setValue(done);
    });
    connect(m_blockCancel, &QToolButton::clicked,
            window, &Window::cancelBlockingOperation);

    updateCurrencyRates();
    updateBlockState(false);
    documentCurrencyChanged(m_doc->currencyCode());

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

void StatusBar::updateStatistics()
{
    static QLocale loc;
    auto stat = m_doc->statistics(m_doc->items(), true /* ignoreExcluded */);

    bool b = (stat.differences() > 0);
    if (b && Config::inst()->showDifferenceIndicators()) {
        auto oldShortcut = m_differences->shortcut();
        m_differences->setText(u"  " % tr("%n Differences(s)", nullptr, stat.differences()));
        m_differences->setShortcut(oldShortcut);
    }
    m_differences->setVisible(b);
    m_differencesSeparator->setVisible(b);

    b = (stat.errors() > 0);
    if (b && Config::inst()->showInputErrors()) {
        auto oldShortcut = m_errors->shortcut();
        m_errors->setText(u"  " % tr("%n Error(s)", nullptr, stat.errors()));
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

    QString valstr = Currency::toDisplayString(stat.value());
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

void StatusBar::updateBlockState(bool blocked)
{
    m_stack->setCurrentIndex(blocked ? 1 : 0);
    if (blocked) {
        m_blockTitle->setText(m_window->blockingOperationTitle());
        m_blockProgress->setRange(0, 0);
    } else {
        m_blockTitle->clear();
        m_blockProgress->reset();
    }
}

void StatusBar::languageChange()
{
    m_differences->setToolTip(Utility::toolTipLabel(tr("Go to the next difference"),
                                                    m_differences->shortcut()));
    m_errors->setToolTip(Utility::toolTipLabel(tr("Go to the next error"), m_errors->shortcut()));

    m_blockCancel->setToolTip(tr("Cancel the currently running blocking operation"));
    if (m_order)
        m_order->setText(tr("Order information..."));
    m_value->setText(tr("Currency:"));
    updateStatistics();
}

void StatusBar::changeEvent(QEvent *e)
{
    QFrame::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        languageChange();
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

    auto *dd = new DocumentDelegate(w_list);
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

    connect(Config::inst(), &Config::showInputErrorsChanged,
            this, &Window::updateItemFlagsMask);
    connect(Config::inst(), &Config::showDifferenceIndicatorsChanged,
            this, &Window::updateItemFlagsMask);
    connect(Config::inst(), &Config::measurementSystemChanged,
            w_list->viewport(), QOverload<>::of(&QWidget::update));

    connect(m_doc, &Document::titleChanged,
            this, &Window::updateCaption);
    connect(m_doc, &Document::fileNameChanged,
            this, &Window::updateCaption);
    connect(m_doc, &Document::modificationChanged,
            this, &Window::updateCaption);
    connect(m_doc, &Document::dataChanged,
            this, &Window::documentItemsChanged);

    connect(this, &Window::blockingOperationActiveChanged,
            w_list, &QWidget::setDisabled);

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

    m_ccw = new ColumnChangeWatcher(this, w_header);

    updateItemFlagsMask();
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

QByteArray Window::currentColumnLayout() const
{
    return w_header->saveLayout();
}

bool Window::isBlockingOperationActive() const
{
    return m_blocked;
}

QString Window::blockingOperationTitle() const
{
    return m_blockTitle;
}

bool Window::isBlockingOperationCancelable() const
{
    return bool(m_blockCancelCallback);
}

void Window::setBlockingOperationCancelCallback(std::function<void ()> cancelCallback)
{
    const bool wasCancelable = isBlockingOperationCancelable();
    m_blockCancelCallback = cancelCallback;
    if (wasCancelable != isBlockingOperationCancelable())
        emit blockingOperationCancelableChanged(!wasCancelable);
}

void Window::setBlockingOperationTitle(const QString &title)
{
    if (title != m_blockTitle) {
        m_blockTitle = title;
        emit blockingOperationTitleChanged(title);
    }
}

void Window::startBlockingOperation(const QString &title, std::function<void ()> cancelCallback)
{
    if (!m_blocked) {
        m_blocked = true;
        setBlockingOperationCancelCallback(cancelCallback);
        setBlockingOperationTitle(title);

        emit blockingOperationActiveChanged(m_blocked);
    }
}

void Window::endBlockingOperation()
{
    if (m_blocked) {
        m_blocked = false;
        setBlockingOperationCancelCallback({ });
        setBlockingOperationTitle({ });

        emit blockingOperationActiveChanged(m_blocked);
    }
}

void Window::cancelBlockingOperation()
{
    if (m_blocked) {
        if (m_blockCancelCallback)
            m_blockCancelCallback();
    }
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

static const struct {
    Window::ColumnLayoutCommand cmd;
    const char *name;
    const char *id;
} columnLayoutCommandList[] = {
{ Window::ColumnLayoutCommand::BrickStoreDefault,
    QT_TRANSLATE_NOOP("Window", "BrickStore default"), "brickstore-default" },
{ Window::ColumnLayoutCommand::BrickStoreSimpleDefault,
    QT_TRANSLATE_NOOP("Window", "BrickStore buyer/collector default"), "brickstore-simple-default" },
{ Window::ColumnLayoutCommand::AutoResize,
    QT_TRANSLATE_NOOP("Window", "Auto-resize once"), "auto-resize" },
{ Window::ColumnLayoutCommand::UserDefault,
    QT_TRANSLATE_NOOP("Window", "User default"), "user-default" },
};

std::vector<Window::ColumnLayoutCommand> Window::columnLayoutCommands()
{
    return { ColumnLayoutCommand::BrickStoreDefault, ColumnLayoutCommand::BrickStoreSimpleDefault,
                ColumnLayoutCommand::AutoResize, ColumnLayoutCommand::UserDefault };
}

QString Window::columnLayoutCommandName(ColumnLayoutCommand clc)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (cmd.cmd == clc)
            return tr(cmd.name);
    }
    return { };
}

QString Window::columnLayoutCommandId(Window::ColumnLayoutCommand clc)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (cmd.cmd == clc)
            return QString::fromLatin1(cmd.id);
    }
    return { };
}

Window::ColumnLayoutCommand Window::columnLayoutCommandFromId(const QString &id)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (id == QLatin1String(cmd.id))
            return cmd.cmd;
    }
    return ColumnLayoutCommand::User;

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
            QVariant v = idx.data(Document::FilterRole);
            QString s;
            static QLocale loc;

            switch (v.userType()) {
            case QMetaType::Double : s = loc.toString(v.toDouble(), 'f', 3); break;
            case QMetaType::Int    : s = loc.toString(v.toInt()); break;
            default:
            case QMetaType::QString: s = v.toString(); break;
            }
            if (idx.column() == Document::Weight) {
                s = Utility::weightToString(v.toDouble(), Config::inst()->measurementSystem());
            }

            FrameWork::inst()->setFilter(m_doc->headerData(idx.column(), Qt::Horizontal).toString()
                                         % QLatin1String(" == ") % s);
        }
    }
}

void Window::on_view_reset_diff_mode_triggered()
{
    m_doc->resetDifferenceMode();
}

void Window::on_edit_status_include_triggered()
{
    setStatus(QT_TR_N_NOOP("Set 'include' status on %n item(s)"), BrickLink::Status::Include);
}

void Window::on_edit_status_exclude_triggered()
{
    setStatus(QT_TR_N_NOOP("Set 'exclude' status on %n item(s)"), BrickLink::Status::Exclude);
}

void Window::on_edit_status_extra_triggered()
{
    setStatus(QT_TR_N_NOOP("Set 'extra' status on %n item(s)"), BrickLink::Status::Extra);
}

void Window::on_edit_status_toggle_triggered()
{
    applyTo(selection(), QT_TR_N_NOOP("Toggled status on %n item(s)"), [](const auto &from, auto &to) {
        (to = from).setStatus(nextEnumValue(from.status(), { BrickLink::Status::Include,
                                                             BrickLink::Status::Exclude }));
        return true;
    });
}

void Window::setStatus(const char *undoText, BrickLink::Status status)
{
    applyTo(selection(), undoText, [status](const auto &from, auto &to) {
        (to = from).setStatus(status); return true;
    });
}


void Window::on_edit_cond_new_triggered()
{
    setCondition(QT_TR_N_NOOP("Set 'new' condition on %n item(s)"), BrickLink::Condition::New);
}

void Window::on_edit_cond_used_triggered()
{
    setCondition(QT_TR_N_NOOP("Set 'used' condition on %n item(s)"), BrickLink::Condition::Used);
}

void Window::setCondition(const char *undoText, BrickLink::Condition condition)
{
    applyTo(selection(), undoText, [condition](const auto &from, auto &to) {
        (to = from).setCondition(condition);
        return true;
    });
}


void Window::on_edit_subcond_none_triggered()
{
    setSubCondition(QT_TR_N_NOOP("Set 'none' sub-condition on %n item(s)"),
                    BrickLink::SubCondition::None);
}

void Window::on_edit_subcond_sealed_triggered()
{
    setSubCondition(QT_TR_N_NOOP("Set 'sealed' sub-condition on %n item(s)"),
                    BrickLink::SubCondition::Sealed);
}

void Window::on_edit_subcond_complete_triggered()
{
    setSubCondition(QT_TR_N_NOOP("Set 'complete' sub-condition on %n item(s)"),
                    BrickLink::SubCondition::Complete);
}

void Window::on_edit_subcond_incomplete_triggered()
{
    setSubCondition(QT_TR_N_NOOP("Set 'incomplete' sub-condition on %n item(s)"),
                    BrickLink::SubCondition::Incomplete);
}

void Window::setSubCondition(const char *undoText, BrickLink::SubCondition subCondition)
{
    applyTo(selection(), undoText, [subCondition](const auto &from, auto &to) {
        (to = from).setSubCondition(subCondition); return true;
    });
}


void Window::on_edit_retain_yes_triggered()
{
    setRetain(QT_TR_N_NOOP("Set 'retain' flag on %n item(s)"), true);
}

void Window::on_edit_retain_no_triggered()
{
    setRetain(QT_TR_N_NOOP("Cleared 'retain' flag on %n item(s)"), false);
}

void Window::on_edit_retain_toggle_triggered()
{
    applyTo(selection(), QT_TR_N_NOOP("Toggled 'retain' flag on %n item(s)"), [](const auto &from, auto &to) {
        (to = from).setRetain(!from.retain()); return true;
    });
}

void Window::setRetain(const char *undoText, bool retain)
{
    applyTo(selection(), undoText, [retain](const auto &from, auto &to) {
        (to = from).setRetain(retain); return true;
    });
}


void Window::on_edit_stockroom_no_triggered()
{
    setStockroom(QT_TR_N_NOOP("Cleared 'stockroom' flag on %n item(s)"), BrickLink::Stockroom::None);
}

void Window::on_edit_stockroom_a_triggered()
{
    setStockroom(QT_TR_N_NOOP("Set stockroom to 'A' on %n item(s)"), BrickLink::Stockroom::A);
}

void Window::on_edit_stockroom_b_triggered()
{
    setStockroom(QT_TR_N_NOOP("Set stockroom to 'B' on %n item(s)"), BrickLink::Stockroom::B);
}

void Window::on_edit_stockroom_c_triggered()
{
    setStockroom(QT_TR_N_NOOP("Set stockroom to 'C' on %n item(s)"), BrickLink::Stockroom::C);
}

void Window::setStockroom(const char *undoText, BrickLink::Stockroom stockroom)
{
    applyTo(selection(), undoText, [stockroom](const auto &from, auto &to) {
        (to = from).setStockroom(stockroom); return true;
    });
}


void Window::on_edit_price_set_triggered()
{
    if (selection().isEmpty())
        return;

    double price = selection().front()->price();

    if (MessageBox::getDouble(this, { }, tr("Enter the new price for all selected items:"),
                              m_doc->currencyCode(), price, 0, FrameWork::maxPrice, 3)) {
        applyTo(selection(), QT_TR_N_NOOP("Set price on %n item(s)"), [price](const auto &from, auto &to) {
            (to = from).setPrice(price); return true;
        });
    }
}

void Window::on_edit_price_round_triggered()
{
    applyTo(selection(), QT_TR_N_NOOP("Rounded price on %n item(s)"),
                     [](const auto &from, auto &to) {
        double price = int(from.price() * 100 + .5) / 100.;
        if (!qFuzzyCompare(price, from.price())) {
            (to = from).setPrice(price);
            return true;
        }
        return false;
    });
}


void Window::on_edit_price_to_priceguide_triggered()
{
    if (selection().isEmpty())
        return;

    Q_ASSERT(m_setToPG.isNull());

    SetToPriceGuideDialog dlg(this);

    if (dlg.exec() == QDialog::Accepted) {
        const auto sel = selection();

        Q_ASSERT(!isBlockingOperationActive());
        startBlockingOperation(tr("Loading price guide data from disk"));

        m_doc->beginMacro();

        m_setToPG.reset(new SetToPriceGuideData);
        m_setToPG->changes.reserve(sel.size());
        m_setToPG->totalCount = sel.count();
        m_setToPG->doneCount = 0;
        m_setToPG->time = dlg.time();
        m_setToPG->price = dlg.price();
        m_setToPG->currencyRate = Currency::inst()->rate(m_doc->currencyCode());

        bool forceUpdate = dlg.forceUpdate();

        for (Document::Item *item : sel) {
            BrickLink::PriceGuide *pg = BrickLink::core()->priceGuide(item->item(), item->color());

            if (forceUpdate && pg && (pg->updateStatus() != BrickLink::UpdateStatus::Updating))
                pg->update();

            if (pg && (pg->updateStatus() == BrickLink::UpdateStatus::Updating)) {
                m_setToPG->priceGuides.insert(pg, item);
                pg->addRef();

            } else if (pg && pg->isValid()) {
                double price = pg->price(m_setToPG->time, item->condition(), m_setToPG->price)
                        * m_setToPG->currencyRate;

                if (!qFuzzyCompare(price, item->price())) {
                    Document::Item newItem = *item;
                    newItem.setPrice(price);
                    m_setToPG->changes.emplace_back(item, newItem);
                }
                ++m_setToPG->doneCount;
                emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);

            } else {
                Document::Item newItem = *item;
                newItem.setPrice(0);
                m_setToPG->changes.emplace_back(item, newItem);

                ++m_setToPG->failCount;
                ++m_setToPG->doneCount;
                emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);
            }
        }

        setBlockingOperationTitle(tr("Downloading price guide data from BrickLink"));
        setBlockingOperationCancelCallback(std::bind(&Window::cancelPriceGuideUpdates, this));

        if (m_setToPG->priceGuides.isEmpty())
            priceGuideUpdated(nullptr);
    }
}

void Window::priceGuideUpdated(BrickLink::PriceGuide *pg)
{
    if (m_setToPG && pg) {
        const auto items = m_setToPG->priceGuides.values(pg);
        for (auto item : items) {
            if (!m_setToPG->canceled) {
                double price = pg->isValid() ? (pg->price(m_setToPG->time, item->condition(),
                                                          m_setToPG->price) * m_setToPG->currencyRate)
                                             : 0;

                if (!qFuzzyCompare(price, item->price())) {
                    Document::Item newItem = *item;
                    newItem.setPrice(price);
                    m_setToPG->changes.emplace_back(item, newItem);
                }
            }
            pg->release();
        }

        m_setToPG->doneCount += items.size();
        emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);

        if (!pg->isValid() || (pg->updateStatus() == BrickLink::UpdateStatus::UpdateFailed))
            m_setToPG->failCount += items.size();
        m_setToPG->priceGuides.remove(pg);
    }

    if (m_setToPG && m_setToPG->priceGuides.isEmpty()
            && (m_setToPG->doneCount == m_setToPG->totalCount)) {
        int failCount = m_setToPG->failCount;
        int successCount = m_setToPG->totalCount - failCount;
        m_doc->changeItems(m_setToPG->changes);
        m_setToPG.reset();

        m_doc->endMacro(tr("Set price to guide on %n item(s)", nullptr, successCount));

        endBlockingOperation();

        QString s = tr("Prices of the selected items have been updated to Price Guide values.");
        if (failCount) {
            s += "<br /><br />" + tr("%1 have been skipped, because of missing Price Guide records or network errors.")
                    .arg(CMB_BOLD(QString::number(failCount)));
        }

        MessageBox::information(this, { }, s);
    }
}

void Window::cancelPriceGuideUpdates()
{
    if (m_setToPG) {
        m_setToPG->canceled = true;
        const auto pgs = m_setToPG->priceGuides.uniqueKeys();
        for (BrickLink::PriceGuide *pg : pgs) {
            if (pg->updateStatus() == BrickLink::UpdateStatus::Updating)
                pg->cancelUpdate();
        }
    }
}


void Window::on_edit_price_inc_dec_triggered()
{
    if (selection().isEmpty())
        return;

    bool showTiers = !w_header->isSectionHidden(Document::TierQ1);

    IncDecPricesDialog dlg(tr("Increase or decrease the prices of the selected items by"),
                           showTiers, m_doc->currencyCode(), this);

    if (dlg.exec() == QDialog::Accepted) {
        double fixed     = dlg.fixed();
        double percent   = dlg.percent();
        double factor    = (1.+ percent / 100.);
        bool tiers       = dlg.applyToTiers();

        applyTo(selection(), QT_TR_N_NOOP("Adjusted price on %n item(s)"),
                         [=](const auto &from, auto &to) {
            double price = from.price();

            if (!qFuzzyIsNull(percent))
                price *= factor;
            else if (!qFuzzyIsNull(fixed))
                price += fixed;

            if (!qFuzzyCompare(price, from.price())) {
                (to = from).setPrice(price);

                if (tiers) {
                    for (int i = 0; i < 3; i++) {
                        price = from.tierPrice(i);

                        if (!qFuzzyIsNull(percent))
                            price *= factor;
                        else if (!qFuzzyIsNull(fixed))
                            price += fixed;

                        to.setTierPrice(i, price);
                    }
                }
                return true;
            }
            return false;
        });
    }
}

void Window::on_edit_cost_set_triggered()
{
    if (selection().isEmpty())
        return;

    double cost = selection().front()->cost();

    if (MessageBox::getDouble(this, { }, tr("Enter the new cost for all selected items:"),
                              m_doc->currencyCode(), cost, 0, FrameWork::maxPrice, 3)) {
        applyTo(selection(), QT_TR_N_NOOP("Set cost on %n item(s)"),
                         [cost](const auto &from, auto &to) {
            (to = from).setCost(cost); return true;
        });
    }
}

void Window::on_edit_cost_round_triggered()
{
    applyTo(selection(), QT_TR_N_NOOP("Rounded cost on %n item(s)"),
                     [](const auto &from, auto &to) {
        double cost = int(from.cost() * 100 + .5) / 100.;
        if (!qFuzzyCompare(cost, from.cost())) {
            (to = from).setCost(cost);
            return true;
        }
        return false;
    });
}

void Window::on_edit_cost_inc_dec_triggered()
{
    if (selection().isEmpty())
        return;

    IncDecPricesDialog dlg(tr("Increase or decrease the costs of the selected items by"),
                           false, m_doc->currencyCode(), this);

    if (dlg.exec() == QDialog::Accepted) {
        double fixed     = dlg.fixed();
        double percent   = dlg.percent();
        double factor    = (1.+ percent / 100.);

        applyTo(selection(), QT_TR_N_NOOP("Adjusted cost on %n item(s)"),
                         [=](const auto &from, auto &to) {
            double cost = from.cost();

            if (!qFuzzyIsNull(percent))
                cost *= factor;
            else if (!qFuzzyIsNull(fixed))
                cost += fixed;

            if (!qFuzzyCompare(cost, from.cost())) {
                (to = from).setCost(cost);
                return true;
            }
            return false;
        });
    }
}

void Window::on_edit_cost_spread_triggered()
{
    if (selection().size() < 2)
        return;

    double spreadAmount = 0;

    if (MessageBox::getDouble(this, { }, tr("Enter the cost amount to spread over all the selected items:"),
                              m_doc->currencyCode(), spreadAmount, 0, FrameWork::maxPrice, 3)) {
        double priceTotal = 0;

        foreach (Document::Item *item, selection())
            priceTotal += (item->price() * item->quantity());
        if (qFuzzyIsNull(priceTotal))
            return;
        double f = spreadAmount / priceTotal;
        if (qFuzzyCompare(f, 1))
            return;

        applyTo(selection(), QT_TR_N_NOOP("Spreaded cost over %n item(s)"),
                         [=](const auto &from, auto &to) {
            (to = from).setCost(from.price() * f); return true;
        });
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
        if (divisor <= 1)
            return;

        int lotsNotDivisible = 0;

        foreach (Document::Item *item, selection()) {
            if (qAbs(item->quantity()) % divisor)
                ++lotsNotDivisible;
        }

        if (lotsNotDivisible) {
            MessageBox::information(this, { },
                                    tr("The quantities of %n lot(s) are not divisible without remainder by %1.<br /><br />Nothing has been modified.",
                                       nullptr, lotsNotDivisible).arg(divisor));
            return;
        }

        applyTo(selection(), QT_TR_N_NOOP("Quantity divide on %n item(s)"),
                         [=](const auto &from, auto &to) {
            (to = from).setQuantity(from.quantity() / divisor); return true;
        });
    }
}

void Window::on_edit_qty_multiply_triggered()
{
    if (selection().isEmpty())
        return;

    int factor = 1;

    if (MessageBox::getInteger(this, { }, tr("Multiply the quantities of all selected items with this factor."),
                               tr("x"), factor, -1000, 1000)) {
        if ((factor == 0) || (factor == 1))
            return;

        int lotsTooLarge = 0;
        int maxQty = FrameWork::maxQuantity / qAbs(factor);

        foreach(Document::Item *item, selection()) {
            if (qAbs(item->quantity()) > maxQty)
                lotsTooLarge++;
        }

        if (lotsTooLarge) {
            MessageBox::information(this, { },
                                    tr("The quantities of %n lot(s) would exceed the maximum allowed item quantity (%2) when multiplied by %1.<br><br>Nothing has been modified.",
                                       nullptr, lotsTooLarge).arg(factor).arg(FrameWork::maxQuantity));
            return;
        }

        applyTo(selection(), QT_TR_N_NOOP("Quantity multiply on %n item(s)"),
                         [=](const auto &from, auto &to) {
            (to = from).setQuantity(from.quantity() * factor); return true;
        });
    }
}

void Window::on_edit_sale_triggered()
{
    if (selection().isEmpty())
        return;

    int sale = selection().front()->sale();

    if (MessageBox::getInteger(this, { }, tr("Set sale in percent for the selected items (this will <u>not</u> change any prices).<br />Negative values are also allowed."),
                               tr("%"), sale, -1000, 99)) {
        applyTo(selection(), QT_TR_N_NOOP("Set sale on %n item(s)"),
                         [sale](const auto &from, auto &to) {
            (to = from).setSale(sale); return true;
        });
    }
}

void Window::on_edit_bulk_triggered()
{
    if (selection().isEmpty())
        return;

    int bulk = selection().front()->bulkQuantity();

    if (MessageBox::getInteger(this, { }, tr("Set bulk quantity for the selected items:"),
                               QString(), bulk, 1, 99999)) {
        applyTo(selection(), QT_TR_N_NOOP("Set bulk quantity on %n item(s)"),
                         [bulk](const auto &from, auto &to) {
            (to = from).setBulkQuantity(bulk); return true;
        });
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
        const auto color = d.color();
        applyTo(selection(), QT_TR_N_NOOP("Set color on %n item(s)"),
                         [color](const auto &from, auto &to) {
            (to = from).setColor(color); return true;
        });
    }
}

void Window::on_edit_item_triggered()
{
    if (selection().isEmpty())
        return;

    SelectItemDialog d(false, this);
    d.setWindowTitle(tr("Modify Item"));
    d.setItem(selection().front()->item());

    if (d.exec() == QDialog::Accepted) {
        const auto item = d.item();
        applyTo(selection(), QT_TR_N_NOOP("Set color on %n item(s)"),
                         [item](const auto &from, auto &to) {
            (to = from).setItem(item); return true;
        });
    }
}

void Window::on_edit_qty_set_triggered()
{
    if (selection().isEmpty())
        return;

    int quantity = selection().front()->quantity();

    if (MessageBox::getInteger(this, { }, tr("Enter the new quantities for all selected items:"),
                               QString(), quantity, -FrameWork::maxQuantity, FrameWork::maxQuantity)) {
        applyTo(selection(), QT_TR_N_NOOP("Set quantity on %n item(s)"),
                         [quantity](const auto &from, auto &to) {
            (to = from).setQuantity(quantity); return true;
        });
    }
}

void Window::on_edit_remark_set_triggered()
{
    if (selection().isEmpty())
        return;

    QString remarks = selection().front()->remarks();

    if (MessageBox::getString(this, { }, tr("Enter the new remark for all selected items:"),
                              remarks)) {
        applyTo(selection(), QT_TR_N_NOOP("Set remark on %n item(s)"),
                         [remarks](const auto &from, auto &to) {
            (to = from).setRemarks(remarks); return true;
        });
    }
}

void Window::on_edit_remark_clear_triggered()
{
    applyTo(selection(), QT_TR_N_NOOP("Cleared remark on %n item(s)"),
                     [](const auto &from, auto &to) {
        (to = from).setRemarks({ }); return true;
    });
}

void Window::on_edit_remark_add_triggered()
{
    if (selection().isEmpty())
        return;

    QString addRemarks;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be added to the remarks of all selected items:"),
                              addRemarks)) {
        applyTo(selection(), QT_TR_N_NOOP("Modified remark on %n item(s)"),
                         [=](const auto &from, auto &to) {
            to = from;
            Document::Item tmp = from;
            tmp.setRemarks(addRemarks);

            return Document::mergeItemFields(tmp, to, Document::MergeMode::Ignore,
                                             {{ Document::Remarks, Document::MergeMode::MergeText }});
        });
    }
}

void Window::on_edit_remark_rem_triggered()
{
    if (selection().isEmpty())
        return;

    QString remRemarks;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be removed from the remarks of all selected items:"),
                              remRemarks)) {
        QRegularExpression regexp(u"\\b" % QRegularExpression::escape(remRemarks) % u"\\b");

        applyTo(selection(), QT_TR_N_NOOP("Modified remark on %n item(s)"),
                         [=](const auto &from, auto &to) {
            QString remark = from.remarks();
            if (!remark.isEmpty())
                remark = remark.remove(regexp).simplified();
            if (from.remarks() != remark) {
                (to = from).setRemarks(remark);
                return true;
            }
            return false;
        });
    }
}


void Window::on_edit_comment_set_triggered()
{
    if (selection().isEmpty())
        return;

    QString comments = selection().front()->comments();

    if (MessageBox::getString(this, { }, tr("Enter the new comment for all selected items:"),
                              comments)) {
        applyTo(selection(), QT_TR_N_NOOP("Set comment on %n item(s)"),
                         [comments](const auto &from, auto &to) {
            (to = from).setComments(comments); return true;
        });
    }
}

void Window::on_edit_comment_clear_triggered()
{
    applyTo(selection(), QT_TR_N_NOOP("Cleared comment on %n item(s)"),
                     [](const auto &from, auto &to) {
        (to = from).setComments({ }); return true;
    });
}

void Window::on_edit_comment_add_triggered()
{
    if (selection().isEmpty())
        return;

    QString addComments;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be added to the comments of all selected items:"),
                              addComments)) {
        applyTo(selection(), QT_TR_N_NOOP("Modified comment on %n item(s)"),
                         [=](const auto &from, auto &to) {
            to = from;
            Document::Item tmp = from;
            tmp.setRemarks(addComments);

            return Document::mergeItemFields(tmp, to, Document::MergeMode::Ignore,
                                             {{ Document::Comments, Document::MergeMode::MergeText }});
        });
    }
}

void Window::on_edit_comment_rem_triggered()
{
    if (selection().isEmpty())
        return;

    QString remComment;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be removed from the comments of all selected items:"),
                              remComment)) {
        QRegularExpression regexp(u"\\b" % QRegularExpression::escape(remComment) % u"\\b");

        applyTo(selection(), QT_TR_N_NOOP("Modified comment on %n item(s)"),
                         [=](const auto &from, auto &to) {
            QString comment = from.comments();
            if (!comment.isEmpty())
                comment = comment.remove(regexp).simplified();
            if (from.comments() != comment) {
                (to = from).setComments(comment);
                return true;
            }
            return false;
        });
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
        applyTo(selection(), QT_TR_N_NOOP("Set reservation on %n item(s)"),
                         [reserved](const auto &from, auto &to) {
            (to = from).setReserved(reserved); return true;
        });
    }
}

void Window::updateItemFlagsMask()
{
    quint64 em = 0;
    quint64 dm = 0;

    if (Config::inst()->showInputErrors())
        em = (1ULL << Document::FieldCount) - 1;
    if (Config::inst()->showDifferenceIndicators())
        dm = (1ULL << Document::FieldCount) - 1;

    m_doc->setItemFlagsMask({ em, dm });
}


void Window::on_edit_copy_fields_triggered()
{
    SelectCopyMergeDialog dlg(document(),
                              tr("Select the document that should serve as a source to fill in the corresponding fields in the current document"),
                              tr("Choose how fields are getting copied or merged."));

    if (dlg.exec() != QDialog::Accepted )
        return;

    auto srcItems = dlg.items();
    if (srcItems.isEmpty())
        return;

    auto defaultMergeMode = dlg.defaultMergeMode();
    auto fieldMergeModes = dlg.fieldMergeModes();
    int copyCount = 0;
    std::vector<std::pair<Document::Item *, Document::Item>> changes;
    changes.reserve(m_doc->items().size()); // just a guestimate

    document()->beginMacro();

    foreach (Document::Item *dstItem, m_doc->items()) {
        foreach (Document::Item *srcItem, srcItems) {
            if (!Document::canItemsBeMerged(*dstItem, *srcItem))
                continue;

            Document::Item newItem = *dstItem;
            if (Document::mergeItemFields(*srcItem, newItem, defaultMergeMode, fieldMergeModes)) {
                changes.emplace_back(dstItem, newItem);
                ++copyCount;
                break;
            }
        }
    }
    if (!changes.empty())
        document()->changeItems(changes);
    document()->endMacro(tr("Copied or merged %n item(s)", nullptr, copyCount));
    qDeleteAll(srcItems);
}

void Window::on_edit_subtractitems_triggered()
{
    SelectDocumentDialog dlg(document(), tr("Which items should be subtracted from the current document:"));

    if (dlg.exec() == QDialog::Accepted ) {
        const Document::ItemList subItems = dlg.items();
        if (subItems.isEmpty())
            return;
        const Document::ItemList &items = document()->items();

        std::vector<std::pair<Document::Item *, Document::Item>> changes;
        changes.reserve(subItems.size() * 2); // just a guestimate
        Document::ItemList newItems;

        document()->beginMacro();

        for (const Document::Item *subItem : subItems) {
            int qty = subItem->quantity();
            if (!subItem->item() || !subItem->color() || !qty)
                continue;

            bool hadMatch = false;

            for (Document::Item *item : items) {
                if (!Document::canItemsBeMerged(*item, *subItem))
                    continue;

                Document::Item newItem = *item;

                if (item->quantity() >= qty) {
                    newItem.setQuantity(item->quantity() - qty);
                    qty = 0;
                } else {
                    newItem.setQuantity(0);
                    qty -= item->quantity();
                }
                changes.emplace_back(item, newItem);
                hadMatch = true;
            }
            if (qty) {   // still a qty left
                if (hadMatch) {
                    Document::Item &lastChange = changes.back().second;
                    lastChange.setQuantity(lastChange.quantity() - qty);
                } else {
                    auto *newItem = new Document::Item();
                    newItem->setItem(subItem->item());
                    newItem->setColor(subItem->color());
                    newItem->setCondition(subItem->condition());
                    newItem->setSubCondition(subItem->subCondition());
                    newItem->setQuantity(-qty);

                    newItems.append(newItem);
                }
            }
        }

        if (!changes.empty())
            document()->changeItems(changes);
        if (!newItems.isEmpty())
            document()->appendItems(newItems);
        document()->endMacro(tr("Subtracted %n item(s)", nullptr, subItems.size()));

        qDeleteAll(subItems);
    }
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
        auto pom = Config::inst()->partOutMode();
        bool inplace = (pom == Config::PartOutMode::InPlace);

        if (pom == Config::PartOutMode::Ask) {
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

void Window::gotoNextErrorOrDifference(bool difference)
{
    auto startIdx = m_selection_model->currentIndex();
    auto skipIdx = startIdx; // skip the field we're on right now...
    if (!startIdx.isValid() || !m_selection_model->isSelected(startIdx)) {
        startIdx = m_doc->index(0, 0);
        skipIdx = { }; // ... but no skipping when we're not anywhere at all
    }

    bool wrapped = false;
    int startCol = startIdx.column();

    for (int row = startIdx.row(); row < m_doc->rowCount(); ) {
        if (wrapped && (row > startIdx.row()))
            return;

        auto flags = m_doc->itemFlags(m_doc->item(m_doc->index(row, 0)));
        quint64 mask = difference ? flags.second : flags.first;
        if (mask) {
            for (int col = startCol; col < m_doc->columnCount(); ++col) {
                if (wrapped && (row == startIdx.row()) && (col > startIdx.column()))
                    return;

                if (mask & (1ULL << col)) {
                    auto gotoIdx = m_doc->index(row, col);
                    if (skipIdx.isValid() && (gotoIdx == skipIdx)) {
                        skipIdx = { };
                    } else {
                        m_selection_model->setCurrentIndex(gotoIdx, QItemSelectionModel::Clear
                                                           | QItemSelectionModel::Select
                                                           | QItemSelectionModel::Current
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

void Window::contextMenu(const QPoint &pos)
{
    if (!m_contextMenu)
        m_contextMenu = new QMenu(this);
    m_contextMenu->clear();
    QAction *defaultAction = nullptr;

    auto idx = w_list->indexAt(pos);
    if (idx.isValid()) {
        QVector<QByteArray> actionNames;

        switch (idx.column()) {
        case Document::Status:
            actionNames = { "edit_status_include", "edit_status_exclude", "edit_status_extra", "-",
                            "edit_status_toggle" };
            break;
        case Document::Picture:
        case Document::PartNo:
        case Document::Description:
            actionNames = { "edit_item" };
            break;
        case Document::Condition:
            actionNames = { "edit_cond_new", "edit_cond_used", "-", "edit_subcond_none",
                            "edit_subcond_sealed", "edit_subcond_complete",
                            "edit_subcond_incomplete" };
            break;
        case Document::Color:
            actionNames = { "edit_color" };
            break;
        case Document::QuantityOrig:
        case Document::QuantityDiff:
        case Document::Quantity:
            actionNames = { "edit_qty_set", "edit_qty_multiply", "edit_qty_divide" };
            break;
        case Document::PriceOrig:
        case Document::PriceDiff:
        case Document::Price:
        case Document::Total:
            actionNames = { "edit_price_set", "edit_price_inc_dec", "edit_price_round",
                            "edit_price_to_priceguide" };
            break;
        case Document::Cost:
            actionNames = { "edit_cost_set", "edit_cost_inc_dec", "edit_cost_round",
                            "edit_cost_spread" };
            break;
        case Document::Bulk:
            actionNames = { "edit_bulk" };
            break;
        case Document::Sale:
            actionNames = { "edit_sale" };
            break;
        case Document::Comments:
            actionNames = { "edit_comment_set", "edit_comment_add", "edit_comment_rem",
                            "edit_comment_clear" };
            break;
        case Document::Remarks:
            actionNames = { "edit_remark_set", "edit_remark_add", "edit_remark_rem",
                            "edit_remark_clear" };
            break;
        case Document::Retain:
            actionNames = { "edit_retain_yes", "edit_retain_no", "-", "edit_retain_toggle" };
            break;
        case Document::Stockroom:
            actionNames = { "edit_stockroom_no", "-", "edit_stockroom_a", "edit_stockroom_b",
                            "edit_stockroom_c" };
            break;
        case Document::Reserved:
            actionNames = { "edit_reserved" };
            break;
        }
        for (const auto &actionName : actionNames) {
            if (actionName == "-")
                m_contextMenu->addSeparator();
            else
                m_contextMenu->addAction(FrameWork::inst()->findAction(actionName.constData()));
        }
    }

    if (!m_contextMenu->isEmpty())
        m_contextMenu->addSeparator();

    const auto actions = FrameWork::inst()->contextMenuActions();
    for (auto action : qAsConst(actions)) {
        if (action)
            m_contextMenu->addAction(action);
        else
            m_contextMenu->addSeparator();
    }
    m_contextMenu->popup(w_list->viewport()->mapToGlobal(pos), defaultAction);
}

void Window::on_document_close_triggered()
{
    close();
}

void Window::closeEvent(QCloseEvent *e)
{
    if (m_doc->isModified()) {
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
    if (!selection().isEmpty()) {
        BrickLink::core()->openUrl(BrickLink::URL_CatalogInfo, (*selection().front()).item(),
                                   (*selection().front()).color());
    }
}

void Window::on_bricklink_priceguide_triggered()
{
    if (!selection().isEmpty()) {
        BrickLink::core()->openUrl(BrickLink::URL_PriceGuideInfo, (*selection().front()).item(),
                                   (*selection().front()).color());
    }
}

void Window::on_bricklink_lotsforsale_triggered()
{
    if (!selection().isEmpty()) {
        BrickLink::core()->openUrl(BrickLink::URL_LotsForSale, (*selection().front()).item(),
                                   (*selection().front()).color());
    }
}

void Window::on_bricklink_myinventory_triggered()
{
    if (!selection().isEmpty()) {
        uint lotid = (*selection().front()).lotId();
        if (lotid) {
            BrickLink::core()->openUrl(BrickLink::URL_StoreItemDetail, &lotid);
        } else {
            BrickLink::core()->openUrl(BrickLink::URL_StoreItemSearch,
                                       (*selection().front()).item(), (*selection().front()).color());
        }
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
    auto clc = columnLayoutCommandFromId(layoutId);
    QString undoName = columnLayoutCommandName(clc);
    QByteArray userLayout;
    if (clc == ColumnLayoutCommand::User) {
        userLayout = Config::inst()->columnLayout(layoutId);
        if (userLayout.isEmpty())
            return;
        undoName = Config::inst()->columnLayoutName(layoutId);
    }

    document()->undoStack()->beginMacro(tr("Set column layout:") % u' ' % undoName);

    switch (clc) {
    case ColumnLayoutCommand::BrickStoreDefault:
    case ColumnLayoutCommand::BrickStoreSimpleDefault:
        resizeColumnsToDefault(clc == ColumnLayoutCommand::BrickStoreSimpleDefault);
        w_list->sortByColumn(0, Qt::AscendingOrder);
        break;
    case ColumnLayoutCommand::AutoResize:
        w_list->resizeColumnsToContents();
        break;
    case ColumnLayoutCommand::UserDefault:
        userLayout = Config::inst()->columnLayout(layoutId);
        if (userLayout.isEmpty()) { // use brickstore default
            resizeColumnsToDefault(false);
            w_list->sortByColumn(0, Qt::AscendingOrder);
            break;
        }
        Q_FALLTHROUGH();
    case ColumnLayoutCommand::User: {
        w_header->restoreLayout(userLayout);
        w_list->sortByColumn(w_header->sortIndicatorSection(), w_header->sortIndicatorOrder());
        break;
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

void Window::resizeColumnsToDefault(bool simpleMode)
{
    int em = w_list->fontMetrics().averageCharWidth();
    for (int i = 0; i < w_list->model()->columnCount(); i++) {
        if (w_header->visualIndex(i) != i)
            w_header->moveSection(w_header->visualIndex(i), i);

        if (w_header->isSectionAvailable(i))
            w_header->setSectionHidden(i, false);

        int mw = w_list->model()->headerData(i, Qt::Horizontal, Document::HeaderDefaultWidthRole).toInt();
        int width = qMax((mw < 0 ? -mw : mw * em) + 8, w_header->sectionSizeHint(i));
        if (width)
            w_header->resizeSection(i, width);
    }

    static const int hiddenColumns[] = {
        Document::PriceOrig,
        Document::PriceDiff,
        Document::QuantityOrig,
        Document::QuantityDiff,
    };

    for (auto i : hiddenColumns)
        w_header->hideSection(i);

    if (simpleMode) {
        static const int hiddenColumnsInSimpleMode[] = {
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
            Document::Comments,
            Document::PriceOrig,
            Document::PriceDiff,
            Document::QuantityOrig,
            Document::QuantityDiff,
        };

        for (auto i : hiddenColumnsInSimpleMode)
            w_header->hideSection(i);
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
       << qint32(5) // version
       << doc->title()
       << doc->fileName()
       << doc->currencyCode()
       << currentColumnLayout()
       << doc->saveSortFilterState()
       << qint32(doc->items().count());

    for (auto item : items) {
        item->save(ds);
        auto base = doc->differenceBaseItem(item);
        ds << bool(base);
        if (base)
            base->save(ds);
    }

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
            QHash<const BrickLink::InvItem *, BrickLink::InvItem> differenceBase;
            qint32 count = 0;

            QDataStream ds(&f);
            ds >> magic >> version >> savedTitle >> savedFileName >> savedCurrencyCode
                    >> savedColumnLayout >> savedSortFilterState >> count;

            if ((count > 0) && (magic == QByteArray(autosaveMagic)) && (version == 5)) {
                BrickLink::InvItemList items;

                for (int i = 0; i < count; ++i) {
                    if (auto item = BrickLink::InvItem::restore(ds)) {
                        items.append(item);
                        bool hasBase = false;
                        ds >> hasBase;
                        if (hasBase) {
                            if (auto base = BrickLink::InvItem::restore(ds)) {
                                differenceBase.insert(item, *base);
                                delete base;
                                continue;
                            }
                        }
                        differenceBase.insert(item, *item);
                    }
                }
                ds >> magic;

                if (magic == QByteArray(autosaveMagic)) {
                    QString restoredTag = tr("RESTORED", "Tag for document restored from autosave");

                    // Document owns the items now
                    auto doc = new Document(items, savedCurrencyCode, differenceBase,
                                            true /*mark as modified*/);
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
