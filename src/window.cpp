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
#include <QColorDialog>
#include <QProgressBar>
#include <QStackedLayout>

#include "utility/messagebox.h"
#include "config.h"
#include "framework.h"
#include "utility/utility.h"
#include "utility/currency.h"
#include "document.h"
#include "documentio.h"
#include "utility/undo.h"
#include "window.h"
#include "window_p.h"
#include "utility/headerview.h"
#include "documentdelegate.h"
#include "script/scriptmanager.h"
#include "script/script.h"
#include "utility/exception.h"
#include "changecurrencydialog.h"

#include "printdialog.h"
#include "selectcolordialog.h"
#include "selectitemdialog.h"
#include "selectdocumentdialog.h"
#include "settopriceguidedialog.h"
#include "incdecpricesdialog.h"
#include "consolidateitemsdialog.h"

using namespace std::chrono_literals;


template <typename E>
static E nextEnumValue(E current, std::initializer_list<E> values)
{
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (current == values.begin()[i])
            return values.begin()[(i + 1) % values.size()];
    }
    return current;
}


void View::applyTo(const LotList &lots, std::function<bool(const Lot &, Lot &)> callback)
{
    if (lots.isEmpty())
        return;

    QString actionText;
    if (auto a = qobject_cast<QAction *>(sender())) {
        actionText = a->text();
        if (actionText.endsWith("..."_l1))
            actionText.chop(3);
    }

    if (!actionText.isEmpty())
        document()->beginMacro();

    int count = lots.size();
    std::vector<std::pair<Lot *, Lot>> changes;
    changes.reserve(count);

    for (Lot *from : lots) {
        Lot to = *from;
        if (callback(*from, to)) {
            changes.emplace_back(from, to);
        } else {
            --count;
        }
    }
    document()->changeLots(changes);

    if (!actionText.isEmpty()) {
        //: Generic undo/redo text: %1 == action name (e.g. "Set price")
        document()->endMacro(tr("%1 on %Ln item(s)", nullptr, count).arg(actionText));
    }
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ColumnChangeWatcher::ColumnChangeWatcher(View *view, HeaderView *header)
    : QObject(view)
    , m_view(view)
    , m_header(header)
{
    // redirect the signals, so that we can block the them while we are applying new values
    connect(header, &HeaderView::sectionMoved,
            this, &ColumnChangeWatcher::internalColumnMoved);
    connect(this, &ColumnChangeWatcher::internalColumnMoved,
            this, [this](int logical, int oldVisual, int newVisual) {
        m_view->document()->undoStack()->push(new ColumnCmd(this, true,
                                                              ColumnCmd::Type::Move,
                                                              logical, oldVisual, newVisual));
    });
    connect(header, &HeaderView::sectionResized,
            this, &ColumnChangeWatcher::internalColumnResized);
    connect(this, &ColumnChangeWatcher::internalColumnResized,
            this, [this](int logical, int oldSize, int newSize) {
        m_view->document()->undoStack()->push(new ColumnCmd(this, true,
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
    addTab(" "_l1);
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


StatusBar::StatusBar(View *view)
    : QFrame(view)
    , m_view(view)
    , m_doc(view->document())
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
        m_order->setIcon(QIcon::fromTheme("help-about"_l1));
        m_order->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_order->setAutoRaise(true);
        layout->addWidget(m_order);

        connect(m_order, &QToolButton::clicked,
                this, [this]() {
            auto order = m_doc->order();
            MessageBox::information(m_view, tr("Order information"), order->address());
        });
    } else {
        m_order = nullptr;
    }
    layout->addStretch(1);

    m_differencesSeparator = addSeparator();
    m_differences = new QToolButton();
    m_differences->setDefaultAction(FrameWork::inst()->findAction("view_goto_next_diff"));
    connect(m_differences->defaultAction(), &QAction::changed,
            this, &StatusBar::updateStatistics);
    m_differences->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_differences->setAutoRaise(true);
    layout->addWidget(m_differences);

    m_errorsSeparator = addSeparator();
    m_errors = new QToolButton();
    m_errors->setDefaultAction(FrameWork::inst()->findAction("view_goto_next_input_error"));
    connect(m_errors->defaultAction(), &QAction::changed,
            this, &StatusBar::updateStatistics);
    m_errors->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_errors->setAutoRaise(true);
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
    m_blockCancel->setIcon(QIcon::fromTheme("process-stop"_l1));
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
    connect(view, &View::blockingOperationActiveChanged,
            this, &StatusBar::updateBlockState);
    connect(view, &View::blockingOperationTitleChanged,
            m_blockTitle, &QLabel::setText);
    connect(view, &View::blockingOperationCancelableChanged,
            m_blockCancel, &QWidget::setEnabled);
    connect(view, &View::blockingOperationProgress,
            this, [this](int done, int total) {
        if (m_blockProgress->maximum() != total)
            m_blockProgress->setRange(0, total);
        m_blockProgress->setValue(done);
    });
    connect(m_blockCancel, &QToolButton::clicked,
            view, &View::cancelBlockingOperation);

    updateCurrencyRates();
    updateBlockState(false);
    documentCurrencyChanged(m_doc->currencyCode());

    paletteChange();
    languageChange();
}

void StatusBar::updateCurrencyRates()
{
    delete m_currency->menu();
    auto m = new QMenu(m_currency);

    QString defCurrency = Config::inst()->defaultCurrencyCode();

    if (!defCurrency.isEmpty()) {
        auto a = m->addAction(tr("Default currency (%1)").arg(defCurrency));
        a->setObjectName(defCurrency);
        m->addSeparator();
    }

    foreach (const QString &c, Currency::inst()->currencyCodes()) {
        auto a = m->addAction(c);
        a->setObjectName(c);
        if (c == m_doc->currencyCode())
            a->setEnabled(false);
    }
    m_currency->setMenu(m);
}

void StatusBar::documentCurrencyChanged(const QString &ccode)
{
    m_currency->setText(ccode + "  "_l1);
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
    auto stat = m_doc->statistics(m_doc->lots(), true /* ignoreExcluded */);

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
        m_blockTitle->setText(m_view->blockingOperationTitle());
        m_blockProgress->setRange(0, 0);
    } else {
        m_blockTitle->clear();
        m_blockProgress->reset();
    }
}

void StatusBar::paletteChange()
{
    QColor c = CheckColorTabBar().color();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, c);
    setPalette(pal);
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
    if (e->type() == QEvent::PaletteChange)
        paletteChange();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QVector<View *> View::s_views;

View::View(Document *doc, const QByteArray &columnLayout, const QByteArray &sortFilterState,
               QWidget *parent)
    : QWidget(parent)
    , m_uuid(QUuid::createUuid())
{
    setAttribute(Qt::WA_DeleteOnClose);

    m_doc = doc;

    bool isForceModified = doc->isModified();

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
    w_header->setSortIndicatorShown(false);
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

    connect(w_header, &HeaderView::isSortedChanged,
            this, [this](bool b) {
        if (b && !document()->isSorted()) {
            document()->reSort();
            w_list->scrollTo(m_selection_model->currentIndex());
        }
    });
    connect(doc, &Document::isSortedChanged,
            w_header, [this](bool b) {
        if (b != w_header->isSorted())
            w_header->setSorted(b);
    });

    connect(w_header, &HeaderView::sortColumnsChanged,
            this, [this](const QVector<QPair<int, Qt::SortOrder>> &sortColumns) {
        if (document()->sortColumns() != sortColumns) {
            document()->sort(sortColumns);
            w_list->scrollTo(m_selection_model->currentIndex());
        }
    });
    connect(doc, &Document::sortColumnsChanged,
            w_header, [this](const QVector<QPair<int, Qt::SortOrder>> &sortColumns) {
        if (w_header->sortColumns() != sortColumns)
            w_header->setSortColumns(sortColumns);
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
            this, &View::ensureLatestVisible);
    connect(w_list, &QWidget::customContextMenuRequested,
            this, &View::contextMenu);

    connect(m_selection_model, &QItemSelectionModel::selectionChanged,
            this, &View::updateSelection);

    connect(BrickLink::core(), &BrickLink::Core::priceGuideUpdated,
            this, &View::priceGuideUpdated);

    connect(Config::inst(), &Config::showInputErrorsChanged,
            this, &View::updateItemFlagsMask);
    connect(Config::inst(), &Config::showDifferenceIndicatorsChanged,
            this, &View::updateItemFlagsMask);
    connect(Config::inst(), &Config::measurementSystemChanged,
            w_list->viewport(), QOverload<>::of(&QWidget::update));

    connect(m_doc, &Document::titleChanged,
            this, &View::updateCaption);
    connect(m_doc, &Document::fileNameChanged,
            this, &View::updateCaption);
    connect(m_doc, &Document::modificationChanged,
            this, &View::updateCaption);
    connect(m_doc, &Document::dataChanged,
            this, &View::documentDataChanged);

    connect(this, &View::blockingOperationActiveChanged,
            w_list, &QWidget::setDisabled);

    updateMinimumSectionSize();
    bool columnsSet = false;

    if (!columnLayout.isEmpty())
        columnsSet = w_header->restoreLayout(columnLayout);
    if (!sortFilterState.isEmpty())
        m_doc->restoreSortFilterState(sortFilterState);

    if (!columnsSet) {
        auto layout = Config::inst()->columnLayout("user-default"_l1);
        if (!w_header->restoreLayout(layout))
            resizeColumnsToDefault();
    }

    m_ccw = new ColumnChangeWatcher(this, w_header);

    updateItemFlagsMask();
    updateCaption();

    setFocusProxy(w_list);

    connect(doc->undoStack(), &QUndoStack::indexChanged,
            this, [this]() { m_autosaveClean = false; });
    connect(&m_autosaveTimer, &QTimer::timeout,
            this, &View::autosave);
    m_autosaveTimer.start(1min); // every minute

    s_views.append(this);

    doc->undoStack()->clear();
    if (isForceModified)
        doc->undoStack()->resetClean();
}

View::~View()
{
    m_autosaveTimer.stop();
    deleteAutosave();
    s_views.removeAll(this);

    delete m_doc;
}

const QVector<View *> &View::allViews()
{
    return s_views;
}

void View::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        updateCaption();
    else if (e->type() == QEvent::FontChange)
        updateMinimumSectionSize();
    QWidget::changeEvent(e);
}

void View::updateCaption()
{
    QString cap = m_doc->fileNameOrTitle();
    if (cap.isEmpty())
        cap = tr("Untitled");

    cap += "[*]"_l1;

    setWindowTitle(cap);
    setWindowModified(m_doc->isModified());
}

void View::updateMinimumSectionSize()
{
    int s = qMax(16, fontMetrics().height() * 2);
    w_header->setMinimumSectionSize(s);
    w_list->setIconSize({ s, s });
}

QByteArray View::currentColumnLayout() const
{
    return w_header->saveLayout();
}

bool View::isBlockingOperationActive() const
{
    return m_blocked;
}

QString View::blockingOperationTitle() const
{
    return m_blockTitle;
}

bool View::isBlockingOperationCancelable() const
{
    return bool(m_blockCancelCallback);
}

void View::setBlockingOperationCancelCallback(std::function<void ()> cancelCallback)
{
    const bool wasCancelable = isBlockingOperationCancelable();
    m_blockCancelCallback = cancelCallback;
    if (wasCancelable != isBlockingOperationCancelable())
        emit blockingOperationCancelableChanged(!wasCancelable);
}

void View::setBlockingOperationTitle(const QString &title)
{
    if (title != m_blockTitle) {
        m_blockTitle = title;
        emit blockingOperationTitleChanged(title);
    }
}

void View::startBlockingOperation(const QString &title, std::function<void ()> cancelCallback)
{
    if (!m_blocked) {
        m_blocked = true;
        setBlockingOperationCancelCallback(cancelCallback);
        setBlockingOperationTitle(title);

        emit blockingOperationActiveChanged(m_blocked);
    }
}

void View::endBlockingOperation()
{
    if (m_blocked) {
        m_blocked = false;
        setBlockingOperationCancelCallback({ });
        setBlockingOperationTitle({ });

        emit blockingOperationActiveChanged(m_blocked);
    }
}

void View::cancelBlockingOperation()
{
    if (m_blocked) {
        if (m_blockCancelCallback)
            m_blockCancelCallback();
    }
}

void View::ensureLatestVisible()
{
    if (m_latest_row >= 0) {
        int xOffset = w_list->horizontalScrollBar()->value();
        w_list->scrollTo(m_doc->index(m_latest_row, w_header->logicalIndexAt(-xOffset)));
        m_latest_row = -1;
    }
}

int View::addLots(const LotList &lots, AddLotMode addLotMode)
{
    // we own the items now

    if (lots.isEmpty())
        return 0;

    bool startedMacro = false;

    bool wasEmpty = (document()->lotCount() == 0);
    Lot *lastAdded = nullptr;
    int addCount = 0;
    int consolidateCount = 0;
    Consolidate conMode = Consolidate::IntoExisting;
    bool repeatForRemaining = false;
    bool costQtyAvg = true;

    for (int i = 0; i < lots.count(); ++i) {
        Lot *lot = lots.at(i);
        bool justAdd = true;

        if (addLotMode != AddLotMode::AddAsNew) {
            Lot *mergeLot = nullptr;

            const auto documentLots = document()->sortedLots();
            for (int j = documentLots.count() - 1; j >= 0; --j) {
                Lot *otherLot = documentLots.at(j);
                if ((!lot->isIncomplete() && !otherLot->isIncomplete())
                        && (lot->item() == otherLot->item())
                        && (lot->color() == otherLot->color())
                        && (lot->condition() == otherLot->condition())
                        && ((lot->status() == BrickLink::Status::Exclude) ==
                            (otherLot->status() == BrickLink::Status::Exclude))) {
                    mergeLot = otherLot;
                    break;
                }
            }

            if (mergeLot) {
                int mergeIndex = -1;

                if ((addLotMode == AddLotMode::ConsolidateInteractive) && !repeatForRemaining) {
                    LotList list { mergeLot, lot };

                    ConsolidateItemsDialog dlg(this, list,
                                               conMode == Consolidate::IntoExisting ? 0 : 1,
                                               conMode, i + 1, lots.count(), this);
                    bool yesClicked = (dlg.exec() == QDialog::Accepted);
                    repeatForRemaining = dlg.repeatForAll();
                    costQtyAvg = dlg.costQuantityAverage();


                    if (yesClicked) {
                        mergeIndex = dlg.consolidateToIndex();

                        if (repeatForRemaining) {
                            conMode = dlg.consolidateRemaining();
                            mergeIndex = (conMode == Consolidate::IntoExisting) ? 0 : 1;
                        }
                    } else {
                        if (repeatForRemaining)
                            addLotMode = AddLotMode::AddAsNew;
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
                        Lot changedLot = *mergeLot;
                        changedLot.mergeFrom(*lot, costQtyAvg);
                        m_doc->changeLot(mergeLot, changedLot);
                        delete lot; // we own it, but we don't need it anymore
                    } else {
                        // merge existing into new, add new, remove existing
                        lot->mergeFrom(*mergeLot, costQtyAvg);
                        m_doc->appendLot(lot); // pass on ownership
                        m_doc->removeLot(mergeLot);
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

            m_doc->appendLot(lot);  // pass on ownership to the doc
            ++addCount;
            lastAdded = lot;
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

    return lots.count();
}


void View::consolidateLots(const LotList &lots)
{
    if (lots.count() < 2)
        return;

    QVector<LotList> mergeList;
    LotList sourceLots = lots;

    for (int i = 0; i < sourceLots.count(); ++i) {
        Lot *lot = sourceLots.at(i);
        LotList mergeLots;

        for (int j = i + 1; j < sourceLots.count(); ++j) {
            Lot *otherLot = sourceLots.at(j);
            if ((!lot->isIncomplete() && !otherLot->isIncomplete())
                    && (lot->item() == otherLot->item())
                    && (lot->color() == otherLot->color())
                    && (lot->condition() == otherLot->condition())
                    && ((lot->status() == BrickLink::Status::Exclude) ==
                        (otherLot->status() == BrickLink::Status::Exclude))) {
                mergeLots << sourceLots.takeAt(j--);
            }
        }
        if (mergeLots.isEmpty())
            continue;

        mergeLots.prepend(sourceLots.at(i));
        mergeList << mergeLots;
    }

    if (mergeList.isEmpty())
        return;

    bool startedMacro = false;

    auto conMode = Consolidate::IntoLowestIndex;
    bool repeatForRemaining = false;
    bool costQtyAvg = true;
    int consolidateCount = 0;

    for (int mi = 0; mi < mergeList.count(); ++mi) {
        const LotList &mergeLots = mergeList.at(mi);
        int mergeIndex = -1;

        if (!repeatForRemaining) {
            ConsolidateItemsDialog dlg(this, mergeLots, consolidateLotsHelper(mergeLots, conMode),
                                       conMode, mi + 1, mergeList.count(), this);
            bool yesClicked = (dlg.exec() == QDialog::Accepted);
            repeatForRemaining = dlg.repeatForAll();
            costQtyAvg = dlg.costQuantityAverage();

            if (yesClicked) {
                mergeIndex = dlg.consolidateToIndex();

                if (repeatForRemaining) {
                    conMode = dlg.consolidateRemaining();
                    mergeIndex = consolidateLotsHelper(mergeLots, conMode);
                }
            } else {
                if (repeatForRemaining)
                    break;
                else
                    continue;
            }
        } else {
            mergeIndex = consolidateLotsHelper(mergeLots, conMode);
        }

        if (!startedMacro) {
            m_doc->beginMacro();
            startedMacro = true;
        }

        Lot newitem = *mergeLots.at(mergeIndex);
        for (int i = 0; i < mergeLots.count(); ++i) {
            if (i != mergeIndex) {
                newitem.mergeFrom(*mergeLots.at(i), costQtyAvg);
                m_doc->removeLot(mergeLots.at(i));
            }
        }
        m_doc->changeLot(mergeLots.at(mergeIndex), newitem);

        ++consolidateCount;
    }
    if (startedMacro)
        m_doc->endMacro(tr("Consolidated %n item(s)", nullptr, consolidateCount));
}

int View::consolidateLotsHelper(const LotList &lots, Consolidate conMode) const
{
    switch (conMode) {
    case Consolidate::IntoTopSorted:
        return 0;
    case Consolidate::IntoBottomSorted:
        return lots.count() - 1;
    case Consolidate::IntoLowestIndex: {
        const auto di = document()->lots();
        auto it = std::min_element(lots.cbegin(), lots.cend(), [di](const auto &a, const auto &b) {
            return di.indexOf(a) < di.indexOf(b);
        });
        return int(std::distance(lots.cbegin(), it));
    }
    case Consolidate::IntoHighestIndex: {
        const auto di = document()->lots();
        auto it = std::max_element(lots.cbegin(), lots.cend(), [di](const auto &a, const auto &b) {
            return di.indexOf(a) < di.indexOf(b);
        });
        return int(std::distance(lots.cbegin(), it));
    }
    default:
        break;
    }
    return -1;
}

static const struct {
    View::ColumnLayoutCommand cmd;
    const char *name;
    const char *id;
} columnLayoutCommandList[] = {
{ View::ColumnLayoutCommand::BrickStoreDefault,
    QT_TRANSLATE_NOOP("View", "BrickStore default"), "brickstore-default" },
{ View::ColumnLayoutCommand::BrickStoreSimpleDefault,
    QT_TRANSLATE_NOOP("View", "BrickStore buyer/collector default"), "brickstore-simple-default" },
{ View::ColumnLayoutCommand::AutoResize,
    QT_TRANSLATE_NOOP("View", "Auto-resize once"), "auto-resize" },
{ View::ColumnLayoutCommand::UserDefault,
    QT_TRANSLATE_NOOP("View", "User default"), "user-default" },
};

std::vector<View::ColumnLayoutCommand> View::columnLayoutCommands()
{
    return { ColumnLayoutCommand::BrickStoreDefault, ColumnLayoutCommand::BrickStoreSimpleDefault,
                ColumnLayoutCommand::AutoResize, ColumnLayoutCommand::UserDefault };
}

QString View::columnLayoutCommandName(ColumnLayoutCommand clc)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (cmd.cmd == clc)
            return tr(cmd.name);
    }
    return { };
}

QString View::columnLayoutCommandId(View::ColumnLayoutCommand clc)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (cmd.cmd == clc)
            return QString::fromLatin1(cmd.id);
    }
    return { };
}

View::ColumnLayoutCommand View::columnLayoutCommandFromId(const QString &id)
{
    for (const auto cmd : columnLayoutCommandList) {
        if (id == QLatin1String(cmd.id))
            return cmd.cmd;
    }
    return ColumnLayoutCommand::User;

}

void View::on_edit_cut_triggered()
{
    if (!selectedLots().isEmpty()) {
        QApplication::clipboard()->setMimeData(new DocumentLotsMimeData(selectedLots()));
        m_doc->removeLots(selectedLots());
    }
}

void View::on_edit_copy_triggered()
{
    if (!selectedLots().isEmpty())
        QApplication::clipboard()->setMimeData(new DocumentLotsMimeData(selectedLots()));
}

void View::on_edit_paste_triggered()
{
    LotList lots = DocumentLotsMimeData::lots(QApplication::clipboard()->mimeData());

    if (!lots.isEmpty()) {
        if (!selectedLots().isEmpty()) {
            if (MessageBox::question(this, { }, tr("Overwrite the currently selected items?"),
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
                                     ) == QMessageBox::Yes) {
                m_doc->removeLots(selectedLots());
            }
        }
        addLots(lots, AddLotMode::ConsolidateInteractive);
    }
}

void View::on_edit_paste_silent_triggered()
{
    LotList lots = DocumentLotsMimeData::lots(QApplication::clipboard()->mimeData());
    if (!lots.isEmpty())
        addLots(lots, AddLotMode::AddAsNew);
}

void View::on_edit_duplicate_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QItemSelection newSelection;
    const QModelIndex oldCurrentIdx = w_list->currentIndex();
    QModelIndex newCurrentIdx;

    applyTo(selectedLots(), [&](const auto &from, auto &to) {
        auto l = new Lot(from);
        m_doc->insertLotsAfter(&from, { l });
        QModelIndex idx = m_doc->index(l);
        newSelection.merge(QItemSelection(idx, idx), QItemSelectionModel::Select);
        if (m_doc->lot(oldCurrentIdx) == &from)
            newCurrentIdx = m_doc->index(idx.row(), oldCurrentIdx.column());

        // this isn't necessary and we should just return false, but the counter would be wrong then
        to = from;
        return true;
    });

    w_list->selectionModel()->select(newSelection, QItemSelectionModel::ClearAndSelect
                                     | QItemSelectionModel::Rows);
    if (newCurrentIdx.isValid()) {
        w_list->selectionModel()->setCurrentIndex(newCurrentIdx, QItemSelectionModel::Current);
    }
}

void View::on_edit_delete_triggered()
{
    if (!selectedLots().isEmpty())
        m_doc->removeLots(selectedLots());
}

void View::on_edit_select_all_triggered()
{
    w_list->selectAll();
}

void View::on_edit_select_none_triggered()
{
    w_list->clearSelection();
}

void View::on_edit_filter_from_selection_triggered()
{
    if (selectedLots().count() == 1) {
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
            if (idx.column() == Document::Weight)
                s = Utility::weightToString(v.toDouble(), Config::inst()->measurementSystem());
            if (s.isEmpty() || s.contains(' '_l1))
                s = u'"' % s % u'"';

            FrameWork::inst()->setFilter(m_doc->headerData(idx.column(), Qt::Horizontal).toString()
                                         % " == "_l1 % s);
        }
    }
}

void View::on_view_reset_diff_mode_triggered()
{
    m_doc->resetDifferenceMode(selectedLots());
}

void View::on_edit_status_include_triggered()
{
    setStatus(BrickLink::Status::Include);
}

void View::on_edit_status_exclude_triggered()
{
    setStatus(BrickLink::Status::Exclude);
}

void View::on_edit_status_extra_triggered()
{
    setStatus(BrickLink::Status::Extra);
}

void View::on_edit_status_toggle_triggered()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        (to = from).setStatus(nextEnumValue(from.status(), { BrickLink::Status::Include,
                                                             BrickLink::Status::Exclude }));
        return true;
    });
}

void View::setStatus(BrickLink::Status status)
{
    applyTo(selectedLots(), [status](const auto &from, auto &to) {
        (to = from).setStatus(status); return true;
    });
}


void View::on_edit_cond_new_triggered()
{
    setCondition(BrickLink::Condition::New);
}

void View::on_edit_cond_used_triggered()
{
    setCondition(BrickLink::Condition::Used);
}

void View::setCondition(BrickLink::Condition condition)
{
    applyTo(selectedLots(), [condition](const auto &from, auto &to) {
        (to = from).setCondition(condition);
        return true;
    });
}


void View::on_edit_subcond_none_triggered()
{
    setSubCondition(BrickLink::SubCondition::None);
}

void View::on_edit_subcond_sealed_triggered()
{
    setSubCondition(BrickLink::SubCondition::Sealed);
}

void View::on_edit_subcond_complete_triggered()
{
    setSubCondition(BrickLink::SubCondition::Complete);
}

void View::on_edit_subcond_incomplete_triggered()
{
    setSubCondition(BrickLink::SubCondition::Incomplete);
}

void View::setSubCondition(BrickLink::SubCondition subCondition)
{
    applyTo(selectedLots(), [subCondition](const auto &from, auto &to) {
        (to = from).setSubCondition(subCondition); return true;
    });
}


void View::on_edit_retain_yes_triggered()
{
    setRetain(true);
}

void View::on_edit_retain_no_triggered()
{
    setRetain(false);
}

void View::on_edit_retain_toggle_triggered()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        (to = from).setRetain(!from.retain()); return true;
    });
}

void View::setRetain(bool retain)
{
    applyTo(selectedLots(), [retain](const auto &from, auto &to) {
        (to = from).setRetain(retain); return true;
    });
}


void View::on_edit_stockroom_no_triggered()
{
    setStockroom(BrickLink::Stockroom::None);
}

void View::on_edit_stockroom_a_triggered()
{
    setStockroom(BrickLink::Stockroom::A);
}

void View::on_edit_stockroom_b_triggered()
{
    setStockroom(BrickLink::Stockroom::B);
}

void View::on_edit_stockroom_c_triggered()
{
    setStockroom(BrickLink::Stockroom::C);
}

void View::setStockroom(BrickLink::Stockroom stockroom)
{
    applyTo(selectedLots(), [stockroom](const auto &from, auto &to) {
        (to = from).setStockroom(stockroom); return true;
    });
}


void View::on_edit_price_set_triggered()
{
    if (selectedLots().isEmpty())
        return;

    double price = selectedLots().front()->price();

    if (MessageBox::getDouble(this, { }, tr("Enter the new price for all selected items:"),
                              m_doc->currencyCode(), price, 0, FrameWork::maxPrice, 3)) {
        applyTo(selectedLots(), [price](const auto &from, auto &to) {
            (to = from).setPrice(price); return true;
        });
    }
}

void View::on_edit_price_round_triggered()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        double price = int(from.price() * 100 + .5) / 100.;
        if (!qFuzzyCompare(price, from.price())) {
            (to = from).setPrice(price);
            return true;
        }
        return false;
    });
}


void View::on_edit_price_to_priceguide_triggered()
{
    if (selectedLots().isEmpty())
        return;

    Q_ASSERT(m_setToPG.isNull());

    SetToPriceGuideDialog dlg(this);

    if (dlg.exec() == QDialog::Accepted) {
        const auto sel = selectedLots();

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

        for (Lot *item : sel) {
            BrickLink::PriceGuide *pg = BrickLink::core()->priceGuide(item->item(), item->color());

            if (pg && (forceUpdate || !pg->isValid())
                    && (pg->updateStatus() != BrickLink::UpdateStatus::Updating)) {
                pg->update();
            }

            if (pg && ((pg->updateStatus() == BrickLink::UpdateStatus::Loading)
                       || (pg->updateStatus() == BrickLink::UpdateStatus::Updating))) {
                m_setToPG->priceGuides.insert(pg, item);
                pg->addRef();

            } else if (pg && pg->isValid()) {
                double price = pg->price(m_setToPG->time, item->condition(), m_setToPG->price)
                        * m_setToPG->currencyRate;

                if (!qFuzzyCompare(price, item->price())) {
                    Lot newItem = *item;
                    newItem.setPrice(price);
                    m_setToPG->changes.emplace_back(item, newItem);
                }
                ++m_setToPG->doneCount;
                emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);

            } else {
                Lot newItem = *item;
                newItem.setPrice(0);
                m_setToPG->changes.emplace_back(item, newItem);

                ++m_setToPG->failCount;
                ++m_setToPG->doneCount;
                emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);
            }
        }

        setBlockingOperationTitle(tr("Downloading price guide data from BrickLink"));
        setBlockingOperationCancelCallback(std::bind(&View::cancelPriceGuideUpdates, this));

        if (m_setToPG->priceGuides.isEmpty())
            priceGuideUpdated(nullptr);
    }
}

void View::priceGuideUpdated(BrickLink::PriceGuide *pg)
{
    if (m_setToPG && pg) {
        const auto lots = m_setToPG->priceGuides.values(pg);

        if (lots.isEmpty())
            return; // not a PG requested by us
        if (pg->updateStatus() == BrickLink::UpdateStatus::Updating)
            return; // loaded now, but still needs an online update

        for (auto lot : lots) {
            if (!m_setToPG->canceled) {
                double price = pg->isValid() ? (pg->price(m_setToPG->time, lot->condition(),
                                                          m_setToPG->price) * m_setToPG->currencyRate)
                                             : 0;

                if (!qFuzzyCompare(price, lot->price())) {
                    Lot newLot = *lot;
                    newLot.setPrice(price);
                    m_setToPG->changes.emplace_back(lot, newLot);
                }
            }
            pg->release();
        }

        m_setToPG->doneCount += lots.size();
        emit blockingOperationProgress(m_setToPG->doneCount, m_setToPG->totalCount);

        if (!pg->isValid() || (pg->updateStatus() == BrickLink::UpdateStatus::UpdateFailed))
            m_setToPG->failCount += lots.size();
        m_setToPG->priceGuides.remove(pg);
    }

    if (m_setToPG && m_setToPG->priceGuides.isEmpty()
            && (m_setToPG->doneCount == m_setToPG->totalCount)) {
        int failCount = m_setToPG->failCount;
        int successCount = m_setToPG->totalCount - failCount;
        m_doc->changeLots(m_setToPG->changes);
        m_setToPG.reset();

        m_doc->endMacro(tr("Set price to guide on %n item(s)", nullptr, successCount));

        endBlockingOperation();

        QString s = tr("Prices of the selected items have been updated to Price Guide values.");
        if (failCount) {
            s = s % u"<br><br>" % tr("%1 have been skipped, because of missing Price Guide records or network errors.")
                    .arg(CMB_BOLD(QString::number(failCount)));
        }

        FrameWork::inst()->setActiveView(this);
        MessageBox::information(this, { }, s);
    }
}

void View::cancelPriceGuideUpdates()
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

void View::editCurrentItem(int column)
{
    static_cast<TableView *>(w_list)->editCurrentItem(column);
}


void View::on_edit_price_inc_dec_triggered()
{
    if (selectedLots().isEmpty())
        return;

    bool showTiers = !w_header->isSectionHidden(Document::TierQ1);

    IncDecPricesDialog dlg(tr("Increase or decrease the prices of the selected items by"),
                           showTiers, m_doc->currencyCode(), this);

    if (dlg.exec() == QDialog::Accepted) {
        double fixed     = dlg.fixed();
        double percent   = dlg.percent();
        double factor    = (1.+ percent / 100.);
        bool tiers       = dlg.applyToTiers();

        applyTo(selectedLots(), [=](const auto &from, auto &to) {
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

void View::on_edit_cost_set_triggered()
{
    if (selectedLots().isEmpty())
        return;

    double cost = selectedLots().front()->cost();

    if (MessageBox::getDouble(this, { }, tr("Enter the new cost for all selected items:"),
                              m_doc->currencyCode(), cost, 0, FrameWork::maxPrice, 3)) {
        applyTo(selectedLots(), [cost](const auto &from, auto &to) {
            (to = from).setCost(cost); return true;
        });
    }
}

void View::on_edit_cost_round_triggered()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        double cost = int(from.cost() * 100 + .5) / 100.;
        if (!qFuzzyCompare(cost, from.cost())) {
            (to = from).setCost(cost);
            return true;
        }
        return false;
    });
}

void View::on_edit_cost_inc_dec_triggered()
{
    if (selectedLots().isEmpty())
        return;

    IncDecPricesDialog dlg(tr("Increase or decrease the costs of the selected items by"),
                           false, m_doc->currencyCode(), this);

    if (dlg.exec() == QDialog::Accepted) {
        double fixed     = dlg.fixed();
        double percent   = dlg.percent();
        double factor    = (1.+ percent / 100.);

        applyTo(selectedLots(), [=](const auto &from, auto &to) {
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

void View::on_edit_cost_spread_triggered()
{
    if (selectedLots().size() < 2)
        return;

    double spreadAmount = 0;

    if (MessageBox::getDouble(this, { }, tr("Enter the cost amount to spread over all the selected items:"),
                              m_doc->currencyCode(), spreadAmount, 0, FrameWork::maxPrice, 3)) {
        double priceTotal = 0;

        foreach (Lot *item, selectedLots())
            priceTotal += (item->price() * item->quantity());
        if (qFuzzyIsNull(priceTotal))
            return;
        double f = spreadAmount / priceTotal;
        if (qFuzzyCompare(f, 1))
            return;

        applyTo(selectedLots(), [=](const auto &from, auto &to) {
            (to = from).setCost(from.price() * f); return true;
        });
    }
}

void View::on_edit_qty_divide_triggered()
{
    if (selectedLots().isEmpty())
        return;

    int divisor = 1;

    if (MessageBox::getInteger(this, { },
                               tr("Divide the quantities of all selected items by this number.<br /><br />(A check is made if all quantites are exactly divisible without reminder, before this operation is performed.)"),
                               QString(), divisor, 1, 1000)) {
        if (divisor <= 1)
            return;

        int lotsNotDivisible = 0;

        foreach (Lot *item, selectedLots()) {
            if (qAbs(item->quantity()) % divisor)
                ++lotsNotDivisible;
        }

        if (lotsNotDivisible) {
            MessageBox::information(this, { },
                                    tr("The quantities of %n lot(s) are not divisible without remainder by %1.<br /><br />Nothing has been modified.",
                                       nullptr, lotsNotDivisible).arg(divisor));
            return;
        }

        applyTo(selectedLots(), [=](const auto &from, auto &to) {
            (to = from).setQuantity(from.quantity() / divisor); return true;
        });
    }
}

void View::on_edit_qty_multiply_triggered()
{
    if (selectedLots().isEmpty())
        return;

    int factor = 1;

    if (MessageBox::getInteger(this, { }, tr("Multiply the quantities of all selected items with this factor."),
                               tr("x"), factor, -1000, 1000)) {
        if ((factor == 0) || (factor == 1))
            return;

        int lotsTooLarge = 0;
        int maxQty = FrameWork::maxQuantity / qAbs(factor);

        foreach(Lot *item, selectedLots()) {
            if (qAbs(item->quantity()) > maxQty)
                lotsTooLarge++;
        }

        if (lotsTooLarge) {
            MessageBox::information(this, { },
                                    tr("The quantities of %n lot(s) would exceed the maximum allowed item quantity (%2) when multiplied by %1.<br><br>Nothing has been modified.",
                                       nullptr, lotsTooLarge).arg(factor).arg(FrameWork::maxQuantity));
            return;
        }

        applyTo(selectedLots(), [=](const auto &from, auto &to) {
            (to = from).setQuantity(from.quantity() * factor); return true;
        });
    }
}

void View::on_edit_sale_triggered()
{
    if (selectedLots().isEmpty())
        return;

    int sale = selectedLots().front()->sale();

    if (MessageBox::getInteger(this, { }, tr("Set sale in percent for the selected items (this will <u>not</u> change any prices).<br />Negative values are also allowed."),
                               tr("%"), sale, -1000, 99)) {
        applyTo(selectedLots(), [sale](const auto &from, auto &to) {
            (to = from).setSale(sale); return true;
        });
    }
}

void View::on_edit_bulk_triggered()
{
    if (selectedLots().isEmpty())
        return;

    int bulk = selectedLots().front()->bulkQuantity();

    if (MessageBox::getInteger(this, { }, tr("Set bulk quantity for the selected items:"),
                               QString(), bulk, 1, 99999)) {
        applyTo(selectedLots(), [bulk](const auto &from, auto &to) {
            (to = from).setBulkQuantity(bulk); return true;
        });
    }
}

void View::on_edit_color_triggered()
{
    if (selectedLots().isEmpty())
        return;

    editCurrentItem(Document::Color);
}

void View::on_edit_item_triggered()
{
    if (selectedLots().isEmpty())
        return;

    editCurrentItem(Document::Description);
}

void View::on_edit_qty_set_triggered()
{
    if (selectedLots().isEmpty())
        return;

    int quantity = selectedLots().front()->quantity();

    if (MessageBox::getInteger(this, { }, tr("Enter the new quantities for all selected items:"),
                               QString(), quantity, -FrameWork::maxQuantity, FrameWork::maxQuantity)) {
        applyTo(selectedLots(), [quantity](const auto &from, auto &to) {
            (to = from).setQuantity(quantity); return true;
        });
    }
}

void View::on_edit_remark_set_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QString remarks = selectedLots().front()->remarks();

    if (MessageBox::getString(this, { }, tr("Enter the new remark for all selected items:"),
                              remarks)) {
        applyTo(selectedLots(), [remarks](const auto &from, auto &to) {
            (to = from).setRemarks(remarks); return true;
        });
    }
}

void View::on_edit_remark_clear_triggered()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        (to = from).setRemarks({ }); return true;
    });
}

void View::on_edit_remark_add_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QString addRemarks;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be added to the remarks of all selected items:"),
                              addRemarks)) {
        applyTo(selectedLots(), [=](const auto &from, auto &to) {
            to = from;
            Lot tmp = from;
            tmp.setRemarks(addRemarks);

            return Document::mergeLotFields(tmp, to, Document::MergeMode::Ignore,
                                            {{ Document::Remarks, Document::MergeMode::MergeText }});
        });
    }
}

void View::on_edit_remark_rem_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QString remRemarks;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be removed from the remarks of all selected items:"),
                              remRemarks)) {
        QRegularExpression regexp(u"\\b" % QRegularExpression::escape(remRemarks) % u"\\b");

        applyTo(selectedLots(), [=](const auto &from, auto &to) {
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


void View::on_edit_comment_set_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QString comments = selectedLots().front()->comments();

    if (MessageBox::getString(this, { }, tr("Enter the new comment for all selected items:"),
                              comments)) {
        applyTo(selectedLots(), [comments](const auto &from, auto &to) {
            (to = from).setComments(comments); return true;
        });
    }
}

void View::on_edit_comment_clear_triggered()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        (to = from).setComments({ }); return true;
    });
}

void View::on_edit_comment_add_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QString addComments;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be added to the comments of all selected items:"),
                              addComments)) {
        applyTo(selectedLots(), [=](const auto &from, auto &to) {
            to = from;
            Lot tmp = from;
            tmp.setComments(addComments);

            return Document::mergeLotFields(tmp, to, Document::MergeMode::Ignore,
                                            {{ Document::Comments, Document::MergeMode::MergeText }});
        });
    }
}

void View::on_edit_comment_rem_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QString remComment;

    if (MessageBox::getString(this, { }, tr("Enter the text, that should be removed from the comments of all selected items:"),
                              remComment)) {
        QRegularExpression regexp(u"\\b" % QRegularExpression::escape(remComment) % u"\\b");

        applyTo(selectedLots(), [=](const auto &from, auto &to) {
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


void View::on_edit_reserved_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QString reserved = selectedLots().front()->reserved();

    if (MessageBox::getString(this, { },
                              tr("Reserve all selected items for this specific buyer (BrickLink username):"),
                              reserved)) {
        applyTo(selectedLots(), [reserved](const auto &from, auto &to) {
            (to = from).setReserved(reserved); return true;
        });
    }
}

void View::on_edit_marker_text_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QString text = selectedLots().front()->markerText();

    if (MessageBox::getString(this, { },
                              tr("Enter the new marker text for all selected items:"),
                              text)) {
        applyTo(selectedLots(), [text](const auto &from, auto &to) {
            (to = from).setMarkerText(text); return true;
        });
    }
}

void View::on_edit_marker_color_triggered()
{
    if (selectedLots().isEmpty())
        return;

    QColor color = selectedLots().front()->markerColor();

    color = QColorDialog::getColor(color, this);
    if (color.isValid()) {
        applyTo(selectedLots(), [color](const auto &from, auto &to) {
            (to = from).setMarkerColor(color); return true;
        });
    }
}

void View::on_edit_marker_clear_triggered()
{
    applyTo(selectedLots(), [](const auto &from, auto &to) {
        (to = from).setMarkerText({ }); to.setMarkerColor({ }); return true;
    });
}



void View::updateItemFlagsMask()
{
    quint64 em = 0;
    quint64 dm = 0;

    if (Config::inst()->showInputErrors())
        em = (1ULL << Document::FieldCount) - 1;
    if (Config::inst()->showDifferenceIndicators())
        dm = (1ULL << Document::FieldCount) - 1;

    m_doc->setLotFlagsMask({ em, dm });
}


void View::on_edit_copy_fields_triggered()
{
    SelectCopyMergeDialog dlg(document(),
                              tr("Select the document that should serve as a source to fill in the corresponding fields in the current document"),
                              tr("Choose how fields are getting copied or merged."));

    if (dlg.exec() != QDialog::Accepted )
        return;

    auto srcLots = dlg.lots();
    if (srcLots.isEmpty())
        return;

    auto defaultMergeMode = dlg.defaultMergeMode();
    auto fieldMergeModes = dlg.fieldMergeModes();
    int copyCount = 0;
    std::vector<std::pair<Lot *, Lot>> changes;
    changes.reserve(m_doc->lots().size()); // just a guestimate

    document()->beginMacro();

    foreach (Lot *dstLot, m_doc->lots()) {
        foreach (Lot *srcLot, srcLots) {
            if (!Document::canLotsBeMerged(*dstLot, *srcLot))
                continue;

            Lot newLot = *dstLot;
            if (Document::mergeLotFields(*srcLot, newLot, defaultMergeMode, fieldMergeModes)) {
                changes.emplace_back(dstLot, newLot);
                ++copyCount;
                break;
            }
        }
    }
    if (!changes.empty())
        document()->changeLots(changes);
    document()->endMacro(tr("Copied or merged %n item(s)", nullptr, copyCount));
    qDeleteAll(srcLots);
}

void View::on_edit_subtractitems_triggered()
{
    SelectDocumentDialog dlg(document(), tr("Which items should be subtracted from the current document:"));

    if (dlg.exec() == QDialog::Accepted ) {
        const LotList subLots = dlg.lots();
        if (subLots.isEmpty())
            return;
        const LotList &lots = document()->lots();

        std::vector<std::pair<Lot *, Lot>> changes;
        changes.reserve(subLots.size() * 2); // just a guestimate
        LotList newLots;

        document()->beginMacro();

        for (const Lot *subLot : subLots) {
            int qty = subLot->quantity();
            if (!subLot->item() || !subLot->color() || !qty)
                continue;

            bool hadMatch = false;

            for (Lot *lot : lots) {
                if (!Document::canLotsBeMerged(*lot, *subLot))
                    continue;

                Lot newItem = *lot;
                auto change = std::find_if(changes.begin(), changes.end(), [=](const auto &c) {
                    return c.first == lot;
                });
                Lot &newItemRef = (change == changes.end()) ? newItem : change->second;
                int qtyInItem = newItemRef.quantity();

                if (qtyInItem >= qty) {
                    newItemRef.setQuantity(qtyInItem - qty);
                    qty = 0;
                } else {
                    newItemRef.setQuantity(0);
                    qty -= qtyInItem;
                }

                // make sure that this is the last entry in changes, so we can reference it
                // easily below, if a qty is left
                if (&newItemRef == &newItem) {
                    changes.emplace_back(lot, newItem);
                } else {
                    auto last = std::prev(changes.end());
                    if (last != change)
                        std::swap(*change, *last);
                }
                hadMatch = true;

                if (qty == 0)
                    break;
            }
            if (qty) {   // still a qty left
                if (hadMatch) {
                    Lot &lastChange = changes.back().second;
                    lastChange.setQuantity(lastChange.quantity() - qty);
                } else {
                    auto *newItem = new Lot();
                    newItem->setItem(subLot->item());
                    newItem->setColor(subLot->color());
                    newItem->setCondition(subLot->condition());
                    newItem->setSubCondition(subLot->subCondition());
                    newItem->setQuantity(-qty);

                    newLots.append(newItem);
                }
            }
        }

        if (!changes.empty())
            document()->changeLots(changes);
        if (!newLots.isEmpty())
            document()->appendLots(newLots);
        document()->endMacro(tr("Subtracted %n item(s)", nullptr, subLots.size()));

        qDeleteAll(subLots);
    }
}

void View::on_edit_mergeitems_triggered()
{
    if (!selectedLots().isEmpty())
        consolidateLots(selectedLots());
    else
        consolidateLots(m_doc->sortedLots());
}

void View::on_edit_partoutitems_triggered()
{
    if (selectedLots().count() >= 1) {
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

        foreach(Lot *lot, selectedLots()) {
            if (inplace) {
                if (lot->item()->hasInventory() && lot->quantity()) {
                    const auto &parts = lot->item()->consistsOf();
                    if (!parts.isEmpty()) {
                        int multiply = lot->quantity();

                        LotList newLots;

                        for (const BrickLink::Item::ConsistsOf &part : parts) {
                            auto partItem = part.item();
                            auto partColor = part.color();
                            if (!partItem)
                                continue;
                            if (lot->colorId() && partItem->itemType()->hasColors()
                                    && partColor && (partColor->id() == 0)) {
                                partColor = lot->color();
                            }
                            auto *newLot = new Lot(partColor, partItem);
                            newLot->setQuantity(part.quantity() * multiply);
                            newLot->setCondition(lot->condition());
                            newLot->setAlternate(part.isAlternate());
                            newLot->setAlternateId(part.alternateId());
                            newLot->setCounterPart(part.isCounterPart());

                            newLots << newLot;
                        }
                        m_doc->insertLotsAfter(lot, newLots);
                        m_doc->removeLot(lot);
                        partcount++;
                    }
                }
            } else {
                FrameWork::inst()->fileImportBrickLinkInventory(lot->item(), lot->color(),
                                                                lot->quantity(), lot->condition());
            }
        }
        if (inplace)
            m_doc->endMacro(tr("Parted out %n item(s)", nullptr, partcount));
    }
    else
        QApplication::beep();
}

void View::gotoNextErrorOrDifference(bool difference)
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

        auto flags = m_doc->lotFlags(m_doc->lot(m_doc->index(row, 0)));
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
        } else if (wrapped && (row == startIdx.row())) {
            return;
        }
        startCol = 0;

        ++row;
        if (row >= m_doc->rowCount()) { // wrap around
            row = 0;
            wrapped = true;
        }
    }
}

void View::contextMenu(const QPoint &pos)
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
        case Document::Marker:
            actionNames = { "edit_marker_text", "edit_marker_color", "-", "edit_marker_clear" };
            break;
        }
        for (const auto &actionName : qAsConst(actionNames)) {
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

void View::on_document_close_triggered()
{
    close();
}

void View::closeEvent(QCloseEvent *e)
{
    if (m_doc->isModified()) {
        FrameWork::inst()->setActiveView(this);

        QMessageBox msgBox(this);
        msgBox.setText(tr("The document %1 has been modified.").arg(CMB_BOLD(windowTitle().replace("[*]"_l1, QString()))));
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

void View::on_bricklink_catalog_triggered()
{
    if (!selectedLots().isEmpty()) {
        BrickLink::core()->openUrl(BrickLink::URL_CatalogInfo, (*selectedLots().front()).item(),
                                   (*selectedLots().front()).color());
    }
}

void View::on_bricklink_priceguide_triggered()
{
    if (!selectedLots().isEmpty()) {
        BrickLink::core()->openUrl(BrickLink::URL_PriceGuideInfo, (*selectedLots().front()).item(),
                                   (*selectedLots().front()).color());
    }
}

void View::on_bricklink_lotsforsale_triggered()
{
    if (!selectedLots().isEmpty()) {
        BrickLink::core()->openUrl(BrickLink::URL_LotsForSale, (*selectedLots().front()).item(),
                                   (*selectedLots().front()).color());
    }
}

void View::on_bricklink_myinventory_triggered()
{
    if (!selectedLots().isEmpty()) {
        uint lotid = (*selectedLots().front()).lotId();
        if (lotid) {
            BrickLink::core()->openUrl(BrickLink::URL_StoreItemDetail, &lotid);
        } else {
            BrickLink::core()->openUrl(BrickLink::URL_StoreItemSearch,
                                       (*selectedLots().front()).item(), (*selectedLots().front()).color());
        }
    }
}

void View::on_view_column_layout_save_triggered()
{
    QString name;

    if (MessageBox::getString(this, { },
                              tr("Enter an unique name for this column layout. Leave empty to change the user default layout."),
                              name)) {
        QString layoutId;
        if (name.isEmpty()) {
            layoutId = "user-default"_l1;
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

void View::on_view_column_layout_list_load(const QString &layoutId)
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
//        w_list->sortByColumn(w_header->sortIndicatorSection(), w_header->sortIndicatorOrder());
        break;
    }
    }

    document()->undoStack()->endMacro();
}

void View::on_view_goto_next_diff_triggered()
{
    gotoNextErrorOrDifference(true);
}

void View::on_view_goto_next_input_error_triggered()
{
    gotoNextErrorOrDifference(false);
}

void View::on_document_print_triggered()
{
    print(false);
}

void View::on_document_print_pdf_triggered()
{
    print(true);
}

void View::print(bool as_pdf)
{
    if (m_doc->filteredLots().isEmpty())
        return;

    QPrinter prt(QPrinter::HighResolution);
    if (as_pdf)
        prt.setPrinterName(QString { });

    bool selectionOnly = (prt.printRange() == QPrinter::Selection);
    PrintDialog pd(&prt, this);
    connect(&pd, &PrintDialog::paintRequested,
            this, [=, this](QPrinter *previewPrt, const QList<uint> &pages, double scaleFactor,
            uint *maxPageCount, double *maxWidth) {
        printPages(previewPrt, selectionOnly ? selectedLots() : document()->filteredLots(),
                   pages, scaleFactor, maxPageCount, maxWidth);
    });
    pd.exec();
}

void View::printScriptAction(PrintingScriptAction *printingAction)
{
    QPrinter prt(QPrinter::HighResolution);
    bool failOnce = false;
    PrintDialog pd(&prt, this);
    connect(&pd, &PrintDialog::paintRequested,
            this, [&](QPrinter *previewPrt, const QList<uint> &pages, double scaleFactor,
            uint *maxPageCount, double *maxWidth) {
        try {
            Q_UNUSED(pages)
            Q_UNUSED(scaleFactor)
            Q_UNUSED(maxWidth)
            previewPrt->setFullPage(true);
            printingAction->executePrint(previewPrt, this, previewPrt->printRange() == QPrinter::Selection, maxPageCount);
        } catch (const Exception &e) {
            QString msg = e.error();
            if (msg.isEmpty())
                msg = tr("Printing failed.");
            else
                msg.replace('\n'_l1, "<br>"_l1);

            QMetaObject::invokeMethod(this, [=, &pd, &failOnce]() {
                if (!failOnce) {
                    failOnce = true;
                    pd.close();
                    MessageBox::warning(nullptr, { }, msg);
                }
            }, Qt::QueuedConnection);
        }
    });
    pd.exec();
}

bool View::printPages(QPrinter *prt, const LotList &lots, const QList<uint> &pages,
                        double scaleFactor, uint *maxPageCount, double *maxWidth)
{
    if (maxPageCount)
        *maxPageCount = 0;
    if (maxWidth) // add a safety margin for rounding errors
        *maxWidth = 0.001;

    if (!prt)
        return false;

    prt->setDocName(document()->fileNameOrTitle());

    QPainter p;
    if (!p.begin(prt))
        return false;

    double prtDpi = double(prt->logicalDpiX() + prt->logicalDpiY()) / 2;
    double winDpi = double(w_list->logicalDpiX() + w_list->logicalDpiX()) / 2 * w_list->devicePixelRatioF();

    QRectF pageRect = prt->pageLayout().paintRect(QPageLayout::Inch);
    pageRect = QRectF(QPointF(), pageRect.size() * prtDpi);

    double rowHeight = double(w_list->verticalHeader()->defaultSectionSize()) * prtDpi / winDpi * scaleFactor;
    int rowsPerPage = int((pageRect.height() - /* all headers and footers */ 2 * rowHeight) / rowHeight);

    if (rowsPerPage <= 0)
        return false;

    int pagesDown = (lots.size() + rowsPerPage - 1) / rowsPerPage;

    QMap<int, QPair<Document::Field, double>> colWidths;
    for (int f = Document::Index; f < Document::FieldCount; ++f) {
        if (w_header->isSectionHidden(f))
            continue;
        int cw = w_header->sectionSize(f);
        if (cw <= 0)
            continue;
        double fcw = cw * prtDpi / winDpi;
        if (maxWidth)
            *maxWidth += fcw;
        colWidths.insert(w_header->visualIndex(f), qMakePair(Document::Field(f), fcw * scaleFactor));
    }

    QVector<QVector<QPair<Document::Field, double>>> colWidthsPerPageAcross;
    QVector<QPair<Document::Field, double>> curPageColWidths;
    double cwUsed = 0;
    for (const auto &cw : colWidths) {
        if (cwUsed && ((cwUsed + cw.second) > pageRect.width())) {
            colWidthsPerPageAcross.append(curPageColWidths);
            curPageColWidths.clear();
            cwUsed = 0;
        }
        auto cwClamped = qMakePair(cw.first, qMin(cw.second, double(pageRect.width())));
        cwUsed += cwClamped.second;
        curPageColWidths.append(cwClamped);
    }
    if (!curPageColWidths.isEmpty())
        colWidthsPerPageAcross.append(curPageColWidths);

    int pagesAcross = colWidthsPerPageAcross.size();
    int pageCount = 0;
    bool needNewPage = false;

    auto *dd = static_cast<DocumentDelegate *>(w_list->itemDelegate());

    QFont f = w_list->font();
    f.setPointSizeF(f.pointSizeF() * scaleFactor / w_list->devicePixelRatioF());
    p.setFont(f);

    QPen gridPen(Qt::darkGray, prtDpi / 96 * scaleFactor);
    QColor headerTextColor(Qt::white);
    double margin = 2 * prtDpi / 96 * scaleFactor;

    if (maxPageCount)
        *maxPageCount = pagesDown * pagesAcross;

    for (int pd = 0; pd < pagesDown; ++pd) {
        for (int pa = 0; pa < pagesAcross; ++pa) {
            pageCount++;

            if (!pages.isEmpty() && !pages.contains(uint(pageCount)))
                continue;

            if (needNewPage)
                prt->newPage();
            else
                needNewPage = true;

            // print footer
            {
                QRectF footerRect(pageRect);
                footerRect.setTop(footerRect.bottom() - rowHeight);

                p.setPen(gridPen);
                QRectF boundingRect;
                p.drawText(footerRect, Qt::AlignRight | Qt::AlignBottom, tr("Page %1/%2")
                           .arg(pageCount).arg(pagesAcross * pagesDown), &boundingRect);
                footerRect.setWidth(footerRect.width() - boundingRect.width() - 5 * margin);
                p.drawText(footerRect, Qt::AlignLeft | Qt::AlignBottom, document()->fileNameOrTitle());
            }

            const auto &colWidths = colWidthsPerPageAcross.at(pa);
            double dx = pageRect.left();
            bool firstColumn = true;

            for (const auto &cw : colWidths) {
                QString title = document()->headerData(cw.first, Qt::Horizontal).toString();
                Qt::Alignment align = Qt::Alignment(document()->headerData(cw.first, Qt::Horizontal,
                                                             Qt::TextAlignmentRole).toInt());
                double w = cw.second;

                double dy = pageRect.top();

                // print header
                QRectF headerRect(dx, dy, w, rowHeight);
                p.setPen(gridPen);
                p.fillRect(headerRect, gridPen.color());
                p.drawRect(headerRect);

                p.setPen(headerTextColor);
                headerRect = headerRect.marginsRemoved({ margin, 0, margin, 0});
                p.drawText(headerRect, align, title);

                dy += rowHeight;

                int li = pd * rowsPerPage; // start at lot index
                for (int l = li; l < qMin(li + rowsPerPage, lots.size()); ++l) {
                    const Lot *lot = lots.at(l);
                    QModelIndex idx = document()->index(lot, cw.first);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    QStyleOptionViewItem options = w_list->viewOptions();
#else
                    QStyleOptionViewItem options;
                    w_list->initViewItemOption(&options);
#endif
                    options.rect = QRectF(dx, dy, w, rowHeight).toRect();
                    options.decorationSize *= (prtDpi / winDpi) * scaleFactor;
                    options.font = p.font();
                    options.fontMetrics = p.fontMetrics();
                    options.index = idx;
                    options.state &= ~QStyle::State_Selected;
                    options.palette = QPalette(Qt::lightGray);

                    dd->paint(&p, options, idx);

                    p.setPen(gridPen);
                    p.drawLine(options.rect.topRight(), options.rect.bottomRight());
                    p.drawLine(options.rect.bottomRight(), options.rect.bottomLeft());
                    if (firstColumn)
                        p.drawLine(options.rect.topLeft(), options.rect.bottomLeft());

                    dy += rowHeight;
                }
                dx += w;
                firstColumn = false;
            }
        }
    }
    return true;
}

void View::on_document_save_triggered()
{
    DocumentIO::save(this);
    deleteAutosave();
}

void View::on_document_save_as_triggered()
{
    DocumentIO::saveAs(this);
    deleteAutosave();
}

void View::on_document_export_bl_xml_triggered()
{
    LotList lots = exportCheck();

    if (!lots.isEmpty())
        DocumentIO::exportBrickLinkXML(lots);
}

void View::on_document_export_bl_xml_clip_triggered()
{
    LotList lots = exportCheck();

    if (!lots.isEmpty())
        DocumentIO::exportBrickLinkXMLClipboard(lots);
}

void View::on_document_export_bl_update_clip_triggered()
{
    LotList lots = exportCheck();

    if (!lots.isEmpty())
        DocumentIO::exportBrickLinkUpdateClipboard(m_doc, lots);
}

void View::on_document_export_bl_invreq_clip_triggered()
{
    LotList lots = exportCheck();

    if (!lots.isEmpty())
        DocumentIO::exportBrickLinkInvReqClipboard(lots);
}

void View::on_document_export_bl_wantedlist_clip_triggered()
{
    LotList lots = exportCheck();

    if (!lots.isEmpty())
        DocumentIO::exportBrickLinkWantedListClipboard(lots);
}

LotList View::exportCheck() const
{
    const LotList lots = selectedLots().isEmpty() ? m_doc->lots() : selectedLots();

    if (m_doc->statistics(lots, true /* ignoreExcluded */).errors()) {
        if (MessageBox::question(nullptr, { },
                                 tr("This list contains items with errors.<br /><br />Do you really want to export this list?"))
                != MessageBox::Yes) {
            return { };
        }
    }
    return m_doc->sortLotList(lots);
}

void View::resizeColumnsToDefault(bool simpleMode)
{
    static const QVector<int> columnOrder { // invert
        Document::Index,
        Document::Status,
        Document::Picture,
        Document::PartNo,
        Document::Description,
        Document::Condition,
        Document::Color,
        Document::QuantityOrig,
        Document::QuantityDiff,
        Document::Quantity,
        Document::PriceOrig,
        Document::PriceDiff,
        Document::Price,
        Document::Total,
        Document::Cost,
        Document::Bulk,
        Document::Sale,
        Document::Marker,
        Document::Comments,
        Document::Remarks,
        Document::Category,
        Document::ItemType,
        Document::TierQ1,
        Document::TierP1,
        Document::TierQ2,
        Document::TierP2,
        Document::TierQ3,
        Document::TierP3,
        Document::LotId,
        Document::Retain,
        Document::Stockroom,
        Document::Reserved,
        Document::Weight,
        Document::YearReleased,
    };

    int em = w_list->fontMetrics().averageCharWidth();
    for (int i = 0; i < w_list->model()->columnCount(); i++) {
        int vi = columnOrder.indexOf(i);

        if (w_header->visualIndex(i) != vi)
            w_header->moveSection(w_header->visualIndex(i), vi);

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

void View::updateSelection()
{
    if (!m_delayedSelectionUpdate) {
        m_delayedSelectionUpdate = new QTimer(this);
        m_delayedSelectionUpdate->setSingleShot(true);
        m_delayedSelectionUpdate->setInterval(0);

        connect(m_delayedSelectionUpdate, &QTimer::timeout, this, [this]() {
            m_selectedLots.clear();

            auto sel = m_selection_model->selectedRows();
            std::sort(sel.begin(), sel.end(), [](const auto &idx1, const auto &idx2) {
                return idx1.row() < idx2.row(); });

            for (const QModelIndex &idx : qAsConst(sel))
                m_selectedLots.append(m_doc->lot(idx));

            emit selectedLotsChanged(m_selectedLots);

            if (m_selection_model->currentIndex().isValid())
                w_list->scrollTo(m_selection_model->currentIndex());
        });
    }
    m_delayedSelectionUpdate->start();
}

void View::setSelection(const LotList &lst)
{
    QItemSelection idxs;

    foreach (const Lot *item, lst) {
        QModelIndex idx(m_doc->index(item));
        idxs.select(idx, idx);
    }
    m_selection_model->select(idxs, QItemSelectionModel::Clear | QItemSelectionModel::Select
                              | QItemSelectionModel::Current | QItemSelectionModel::Rows);
}

void View::documentDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    for (int r = topLeft.row(); r <= bottomRight.row(); ++r) {
        auto lot = m_doc->lot(topLeft.siblingAtRow(r));
        if (m_selectedLots.contains(lot)) {
            emit selectedLotsChanged(m_selectedLots);
            break;
        }
    }
}


static const char *autosaveMagic = "||BRICKSTORE AUTOSAVE MAGIC||";
static const char *autosaveTemplate = "brickstore_%1.autosave";

void View::deleteAutosave()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    QString filename = QString::fromLatin1(autosaveTemplate).arg(m_uuid.toString());
    temp.remove(filename);
}

void View::moveColumnDirect(int /*logical*/, int oldVisual, int newVisual)
{
    w_header->moveSection(oldVisual, newVisual);
}

void View::resizeColumnDirect(int logical, int /*oldSize*/, int newSize)
{
    w_header->resizeSection(logical, newSize);
}

class AutosaveJob : public QRunnable
{
public:
    explicit AutosaveJob(View *win, const QByteArray &contents)
        : QRunnable()
        , m_win(win)
        , m_uuid(win->m_uuid)
        , m_contents(contents)
    { }

    void run() override;
private:
    View *m_win;
    const QUuid m_uuid;
    const QByteArray m_contents;
};

void AutosaveJob::run()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    QString fileName = QString::fromLatin1(autosaveTemplate).arg(m_uuid.toString());
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

        View *win = m_win;
        QMetaObject::invokeMethod(qApp, [=]() {
            if (View::allViews().contains(win)) {
                if (!QDir(temp).rename(newFileName, fileName))
                    qWarning() << "Autosave rename from" << newFileName << "to" << fileName << "failed";
                win->m_autosaveClean = true;
            }
        });
    }
}


void View::autosave() const
{
    auto doc = document();
    auto lots = document()->lots();

    if (m_uuid.isNull() || !doc->isModified() || lots.isEmpty() || m_autosaveClean)
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
       << qint32(lots.count());

    for (auto lot : lots) {
        lot->save(ds);
        auto base = doc->differenceBaseLot(lot);
        ds << bool(base);
        if (base)
            base->save(ds);
    }

    ds << QByteArray(autosaveMagic);

    QThreadPool::globalInstance()->start(new AutosaveJob(const_cast<View *>(this), ba));
}

int View::restorableAutosaves()
{
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    return temp.entryList({ QString::fromLatin1(autosaveTemplate).arg("*"_l1) }).count();
}

const QVector<View *> View::processAutosaves(AutosaveAction action)
{
    QVector<View *> restored;

    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    const auto ondisk = temp.entryList({ QString::fromLatin1(autosaveTemplate).arg("*"_l1) });

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
            QHash<const Lot *, Lot> differenceBase;
            qint32 count = 0;

            QDataStream ds(&f);
            ds >> magic >> version >> savedTitle >> savedFileName >> savedCurrencyCode
                    >> savedColumnLayout >> savedSortFilterState >> count;

            if ((count > 0) && (magic == QByteArray(autosaveMagic)) && (version == 5)) {
                LotList lots;

                for (int i = 0; i < count; ++i) {
                    if (auto lot = Lot::restore(ds)) {
                        lots.append(lot);
                        bool hasBase = false;
                        ds >> hasBase;
                        if (hasBase) {
                            if (auto base = Lot::restore(ds)) {
                                differenceBase.insert(lot, *base);
                                delete base;
                                continue;
                            }
                        }
                        differenceBase.insert(lot, *lot);
                    }
                }
                ds >> magic;

                if (magic == QByteArray(autosaveMagic)) {
                    QString restoredTag = tr("RESTORED", "Tag for document restored from autosave");

                    // Document owns the items now
                    DocumentIO::BsxContents bsx;
                    bsx.lots = lots;
                    bsx.currencyCode = savedCurrencyCode;
                    bsx.differenceModeBase = differenceBase;
                    auto doc = new Document(bsx, true /*mark as modified*/);
                    lots.clear();

                    if (!savedFileName.isEmpty()) {
                        QFileInfo fi(savedFileName);
                        QString newFileName = fi.dir().filePath(restoredTag % u" " % fi.fileName());
                        doc->setFileName(newFileName);
                    } else {
                        doc->setTitle(restoredTag % u" " % savedTitle);
                    }
                    doc->restoreSortFilterState(savedSortFilterState);
                    auto win = new View(doc, savedColumnLayout);

                    if (!savedFileName.isEmpty())
                        DocumentIO::save(win);

                    restored.append(win);
                }
                qDeleteAll(lots);
            }
            f.close();
        }
        f.remove();
    }
    return restored;
}

#include "moc_window.cpp"
