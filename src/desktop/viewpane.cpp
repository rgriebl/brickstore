// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QLocale>
#include <QStyle>
#include <QBoxLayout>
#include <QApplication>
#include <QToolBar>
#include <QToolButton>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QMessageBox>
#include <QSplitter>
#include <QMainWindow>
#include <QKeyEvent>
#include <QDebug>
#include <QTreeView>
#include <QScrollBar>
#include <QShortcut>

#include <QCoro/QCoroSignal>

#include "common/actionmanager.h"
#include "common/config.h"
#include "common/currency.h"
#include "common/document.h"
#include "common/documentlist.h"
#include "common/documentmodel.h"
#include "common/eventfilter.h"
#include "utility/utility.h"
#include "changecurrencydialog.h"
#include "filtertermwidget.h"
#include "menucombobox.h"
#include "orderinformationdialog.h"
#include "view.h"
#include "viewpane.h"


class CollapsibleLabel : public QLabel
{
    // Q_OBJECT

public:
    CollapsibleLabel(const QString &str = { })  : QLabel(str)
    {
        setSizePolicy({ QSizePolicy::Maximum, QSizePolicy::Fixed });
        setMinimumSize(0, 0);
    }

    QSize minimumSizeHint() const override;
};

QSize CollapsibleLabel::minimumSizeHint() const
{
    return {0, 0};
}


class OpenDocumentsMenu : public QFrame
{
public:
    OpenDocumentsMenu(ViewPane *viewPane)
        : QFrame(viewPane, Qt::Popup)
        , m_viewPane(viewPane)
        , m_list(new QTreeView(this))
    {
        setMinimumSize(300, 200);
        setFocusProxy(m_list);
        m_list->setContextMenuPolicy(Qt::NoContextMenu);
        m_list->setAlternatingRowColors(false);
        m_list->setHeaderHidden(true);
        m_list->setAllColumnsShowFocus(true);
        m_list->setUniformRowHeights(true);
        m_list->setRootIsDecorated(false);
        m_list->setTextElideMode(Qt::ElideMiddle);
        m_list->setSelectionMode(QAbstractItemView::SingleSelection);
        m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        m_list->setModel(DocumentList::inst());
        int is = style()->pixelMetric(QStyle::PM_ListViewIconSize, nullptr, m_list);
        m_list->setIconSize({ int(is * 2), int(is * 2) });

        // We disable the frame on this list view and use a QFrame around it instead.
        // This improves the look with QGTKStyle. (from QtCreator)
#if !defined(Q_OS_MACOS)
        setFrameStyle(m_list->frameStyle());
#endif
        m_list->setFrameStyle(QFrame::NoFrame);

        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_list);

        new EventFilter(m_list, { QEvent::KeyPress, QEvent::KeyRelease }, [this](QObject *, QEvent *e) {
            if (e->type() == QEvent::KeyPress) {
                auto ke = static_cast<QKeyEvent*>(e);
                if (ke->key() == Qt::Key_Escape) {
                    hide();
                    return EventFilter::StopEventProcessing;
                }
                if (ke->key() == Qt::Key_Return
                        || ke->key() == Qt::Key_Enter) {
                    activateIndex(m_list->currentIndex());
                    return EventFilter::StopEventProcessing;
                }
            } else if (e->type() == QEvent::KeyRelease) {
                auto ke = static_cast<QKeyEvent*>(e);
                if (ke->modifiers() == 0
                        /*HACK this is to overcome some event inconsistencies between platforms*/
                        || (ke->modifiers() == Qt::AltModifier
                            && (ke->key() == Qt::Key_Alt || ke->key() == -1))) {
                    activateIndex(m_list->currentIndex());
                    hide();
                }
            }
            return EventFilter::ContinueEventProcessing;
        });

        connect(m_list, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
            activateIndex(idx);
            hide();
        });
    }

    QSize sizeHint() const override
    {
        struct MyTreeView : public QTreeView {
            static QSize publicViewportSizeHint(QTreeView *view) {
                return (view->*(&MyTreeView::viewportSizeHint))();
            }
            static int publicSizeHintForColumn(QTreeView *view, int col) {
                return (view->*(&MyTreeView::sizeHintForColumn))(col);
            }
        };

        return QSize(MyTreeView::publicSizeHintForColumn(m_list, 0)
                         + m_list->verticalScrollBar()->width() + m_list->frameWidth() * 2,
                     MyTreeView::publicViewportSizeHint(m_list).height() + m_list->frameWidth() * 2)
                + QSize(frameWidth() * 2, frameWidth() * 2);
    }

    void openAndSelect(int delta)
    {
        int row = m_list->currentIndex().row();

        if (!isVisible()) {
            auto maxSize = m_viewPane->size() / 2;
            resize(sizeHint().boundedTo(maxSize));
            move(m_viewPane->mapToGlobal(m_viewPane->rect().center()) - rect().center());
            show();
            setFocus();
            if (auto *doc = m_viewPane->activeDocument())
                row = int(DocumentList::inst()->documents().indexOf(doc));
        }
        int cnt = DocumentList::inst()->count();
        row = ((row + delta) % cnt + cnt) % cnt;
        m_list->setCurrentIndex(DocumentList::inst()->index(row, 0));
        m_list->scrollTo(m_list->currentIndex(), QAbstractItemView::PositionAtCenter);
    }

private:
    void activateIndex(const QModelIndex &idx)
    {
        if (idx.isValid()) {
            if (auto *doc = idx.data(Qt::UserRole).value<Document *>())
                m_viewPane->activateDocument(doc);
        }
    }

    ViewPane *m_viewPane;
    QTreeView *m_list;
};

ViewPane::ViewPane(const std::function<ViewPane *(Document *, QWidget *)> &viewPaneCreate,
                   Document *activeDocument)
    : QWidget()
    , m_viewPaneCreate(viewPaneCreate)
    , m_openDocumentsMenu(new OpenDocumentsMenu(this))
{
    Q_ASSERT(viewPaneCreate);

    auto *top = new QFrame(this);
    top->setFrameStyle(int(QFrame::StyledPanel) | int(QFrame::Plain));
    top->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // hide the bottom frame line
    top->setContentsMargins(top->contentsMargins() + QMargins(0, 0, 0, -top->frameWidth()));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(top);

    m_viewStack = new QStackedWidget();
    setFocusProxy(m_viewStack);
    layout->addWidget(m_viewStack);

    m_filter = new FilterWidget();
    m_filter->setVisible(false);
    createToolBar();

    auto *topLayout = new QVBoxLayout(top);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(2);
    topLayout->addWidget(m_toolBar, 0);
    topLayout->addWidget(m_filter, 1'000'000);

    connect(m_currency, &QToolButton::triggered,
            this, &ViewPane::changeDocumentCurrency);
    connect(Currency::inst(), &Currency::ratesChanged,
            this, &ViewPane::updateCurrencyRates);

    auto setIconSizeLambda = [this](Config::UISize iconSize) {
        static const QMap<Config::UISize, QStyle::PixelMetric> map = {
            { Config::UISize::System, QStyle::PM_SmallIconSize },
            { Config::UISize::Small, QStyle::PM_SmallIconSize },
            { Config::UISize::Large, QStyle::PM_ToolBarIconSize },
        };
        auto pm = map.value(iconSize, QStyle::PM_SmallIconSize);
        int s = style()->pixelMetric(pm, nullptr, this);
        for (QToolButton *tb : { m_filterOnOff, m_order, m_errors, m_differences, m_split }) {
            if (tb)
                tb->setIconSize({s, s});
        }
        m_viewList->setIconSize({ s, s });
        m_filter->setIconSize({ s, s});

    };
    connect(Config::inst(), &Config::iconSizeChanged, this, setIconSizeLambda);
    setIconSizeLambda(Config::inst()->iconSize());

    fontChange();
    paletteChange();
    languageChange();

    m_viewStack->setMinimumHeight(std::max(m_viewStack->minimumHeight(), fontMetrics().height() * 20));

    setupViewStack();

    auto nextDoc = new QAction(this);
    nextDoc->setShortcut(
#if defined(Q_OS_MACOS)
                u"Alt+Tab"_qs
#else
                u"Ctrl+Tab"_qs
#endif
                );
    nextDoc->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(nextDoc, &QAction::triggered, this, [this]() { m_openDocumentsMenu->openAndSelect(1); });
    addAction(nextDoc);

    auto prevDoc = new QAction(this);
    prevDoc->setShortcut(
#if defined(Q_OS_MACOS)
                u"Alt+Shift+Tab"_qs
#else
                u"Ctrl+Shift+Tab"_qs
#endif
                );
    prevDoc->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(prevDoc, &QAction::triggered, this, [this]() { m_openDocumentsMenu->openAndSelect(-1); });
    addAction(prevDoc);

    if (activeDocument)
        activateDocument(activeDocument);
}

ViewPane::~ViewPane()
{
    setView(nullptr);
    disconnect(m_viewStack, &QStackedWidget::currentChanged, this, nullptr);
    while (auto *w = m_viewStack->widget(0)) {
        m_viewStack->removeWidget(w);
        disconnect(w, &QObject::destroyed, this, nullptr);
        delete w;
    }

    emit beingDestroyed();
}

void ViewPane::newWindow()
{
    auto *nw = new QMainWindow(window());
    const auto allActions = ActionManager::inst()->allActions();
    for (auto a : allActions) {
        if (auto *qa = a->qAction())
            nw->addAction(qa);
    }
    nw->setAttribute(Qt::WA_DeleteOnClose);
    auto *rootSplitter = new QSplitter();
    rootSplitter->setObjectName(u"WindowSplitter"_qs);
    auto *vp = m_viewPaneCreate(nullptr, nw);
    rootSplitter->addWidget(vp);
    nw->setCentralWidget(rootSplitter);
    nw->show();
    vp->activateDocument(activeDocument());

    connect(vp->m_viewStack, &QStackedWidget::widgetRemoved, this, [vp]() {
        if (vp->m_viewStack->count() == 0)
            vp->window()->close();
    });
}

void ViewPane::split(Qt::Orientation o)
{
    if (auto parentSplitter = qobject_cast<QSplitter *>(parentWidget())) {
        int idx = parentSplitter->indexOf(this);
        auto newSplitter = new QSplitter(o);
        newSplitter->setChildrenCollapsible(false);
        parentSplitter->replaceWidget(idx, newSplitter);
        newSplitter->insertWidget(0, this);
        newSplitter->insertWidget(1, m_viewPaneCreate(activeDocument(), window()));
        auto sizes = newSplitter->sizes();
        int h = (sizes.at(0) + sizes.at(1)) / 2;
        newSplitter->setSizes({ h, h });
    }
}

bool ViewPane::canUnsplit() const
{
    if (auto parentSplitter = qobject_cast<QSplitter *>(parentWidget()))
        return (parentSplitter->count() > 1);
    return false;
}

void ViewPane::unsplit()
{
    if (auto parentSplitter = qobject_cast<QSplitter *>(parentWidget())) {
        Q_ASSERT(parentSplitter->count() <= 2);
        if (parentSplitter->count() > 1) {
            setParent(nullptr);
            Q_ASSERT(parentSplitter->count() == 1);

            if (auto grandParentSplitter = qobject_cast<QSplitter *>(parentSplitter->parentWidget())) {
                int idx = grandParentSplitter->indexOf(parentSplitter);
                Q_ASSERT(idx == 0 || idx == 1);
                grandParentSplitter->replaceWidget(idx, parentSplitter->widget(0));
                delete parentSplitter;
            }

            deleteLater();
        }
    }
}

void ViewPane::setupViewStack()
{
    connect(m_viewStack, &QStackedWidget::currentChanged,
            this, [this](int stackIdx) {
        auto *view = qobject_cast<View *>(m_viewStack->widget(stackIdx));
        if (view) {
            auto *doc = view->document();
            m_viewList->setCurrentIndex(m_viewList->findData(QVariant::fromValue(doc)));
        }
        if ((stackIdx == -1) && (m_viewStack->count() == 0)) {
            // the stack is empty -> try to unsplit
            if (canUnsplit())
                QMetaObject::invokeMethod(this, &ViewPane::unsplit, Qt::QueuedConnection);
        }
        setView(view);
        emit viewActivated(view);
    });
    connect(m_viewList, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int docIdx) {
        if (auto *doc = m_viewList->itemData(docIdx).value<Document *>())
            activateDocument(doc);
        else
            m_viewList->setToolTip({ });
    });

    connect(DocumentList::inst(), &DocumentList::rowsAboutToBeRemoved,
            this, [this](const QModelIndex &, int first, int last) {
        for (int i = first; i <= last; ++i) {
            auto *doc = m_viewList->itemData(i).value<Document *>();
            auto *view = m_viewStackMapping.value(doc);
            if (view) {
                m_viewStack->removeWidget(view);
                delete view;
            }
        }
    });

//  for (int i = 0; i <= 9; ++i) {
//        //: Shortcut to activate window 0-9
//        auto sc = new QShortcut(tr("Alt+%1").arg(i), this);
//        sc->setContext(Qt::WidgetWithChildrenShortcut);
//        connect(sc, &QShortcut::activated, this, [this, i]() {
//            int docIdx = (i == 0) ? 9 : i - 1;
//            if (auto *doc = m_viewList->itemData(docIdx).value<Document *>())
//                activateDocument(doc);
//        });
//    }
}

void ViewPane::setView(View *view)
{
    delete m_viewConnectionContext;
    m_viewConnectionContext = nullptr;
    m_view = nullptr;
    m_model = nullptr;
    m_filter->setDocument(nullptr);
    if (!view)
        return;
    m_viewConnectionContext = new QObject(this);
    m_view = view;
    m_model = view->model();

    connect(m_model, &DocumentModel::currencyCodeChanged,
            m_viewConnectionContext, [this](const QString &c) { documentCurrencyChanged(c); });
    connect(m_model, &DocumentModel::statisticsChanged,
            m_viewConnectionContext, [this]() { updateStatistics(); });

    connect(m_view->document(), &Document::blockingOperationActiveChanged,
            m_viewConnectionContext, [this](bool b) { updateBlockState(b); });

    m_filter->setDocument(view->document());

    auto checkOrder = [this]() {
        bool hasOrder = (m_view->document()->order());
        m_order->setVisible(hasOrder);
        m_orderSeparator->setVisible(hasOrder);
    };
    connect(m_view->document(), &Document::orderChanged,
            m_viewConnectionContext, checkOrder);
    checkOrder();

    updateCurrencyRates();
    updateBlockState(false);
    documentCurrencyChanged(m_model->currencyCode());
    updateStatistics();
}


void ViewPane::activateDocument(Document *document)
{
    Q_ASSERT(document);
    auto *view = m_viewStackMapping.value(document);
    if (!view) {
        view = newView(document);
        m_viewStackMapping[document] = view;
        m_viewStack->addWidget(view);
    }
    m_viewStack->setCurrentWidget(view);
    view->setFocus();
    m_viewList->setToolTip(document->filePathOrTitle());
}

Document *ViewPane::activeDocument() const
{
    if (auto *view = activeView())
        return view->document();
    return nullptr;
}

View *ViewPane::activeView() const
{
    if (auto *view = qobject_cast<View *>(m_viewStack->currentWidget()))
        return view;
    return nullptr;
}

View *ViewPane::viewForDocument(Document *document)
{
    return m_viewStackMapping.value(document);
}

bool ViewPane::isActive() const
{
    return m_active;
}

void ViewPane::setActive(bool b)
{
    if (m_active != b) {
        m_active = b;

        fontChange();
        paletteChange();
        if (b)
            activateWindow();
        emit viewActivated(activeView());
    }
}


void ViewPane::updateCurrencyRates()
{
    if (!m_model)
        return;

    delete m_currency->menu();
    auto m = new QMenu(m_currency);

    QString defCurrency = Config::inst()->defaultCurrencyCode();

    if (!defCurrency.isEmpty()) {
        auto a = m->addAction(tr("Default currency (%1)").arg(defCurrency));
        a->setObjectName(defCurrency);
        m->addSeparator();
    }

    const auto ccodes = Currency::inst()->currencyCodes();
    for (const QString &c : ccodes) {
        auto a = m->addAction(c);
        a->setObjectName(c);
        if (c == m_model->currencyCode())
            a->setEnabled(false);
    }
    m_currency->setMenu(m);
}

void ViewPane::documentCurrencyChanged(const QString &ccode)
{
    m_currency->setText(ccode + u"  ");
    // the menu might still be open right now, so we need to delay deleting the actions
    QMetaObject::invokeMethod(this, &ViewPane::updateCurrencyRates, Qt::QueuedConnection);
}

QCoro::Task<> ViewPane::changeDocumentCurrency(QAction *a)
{
    if (!m_model)
        co_return;

    QString ccode = a->objectName();

    if (ccode != m_model->currencyCode()) {
        ChangeCurrencyDialog dlg(m_model->currencyCode(), ccode, m_model->legacyCurrencyCode(), this);
        dlg.setWindowModality(Qt::ApplicationModal);
        dlg.show();

        if (co_await qCoro(&dlg, &QDialog::finished) == QDialog::Accepted) {
            double rate = dlg.exchangeRate();

            if (rate > 0)
                m_model->setCurrencyCode(ccode, rate);
        }
    }
}

void ViewPane::updateStatistics()
{
    if (!m_model)
        return;

    static QLocale loc;
    auto stat = m_model->statistics(m_model->lots(), true /* ignoreExcluded */);

    bool b = (stat.differences() > 0);
    if (b && Config::inst()->showDifferenceIndicators()) {
        auto oldShortcut = m_differences->shortcut();
        m_differences->setText(u' ' + loc.toString(stat.differences()));
        m_differences->setShortcut(oldShortcut);
    }
    m_differences->setVisible(b);
    m_differencesSeparator->setVisible(b);

    b = (stat.errors() > 0);
    if (b && Config::inst()->showInputErrors()) {
        auto oldShortcut = m_errors->shortcut();
        m_errors->setText(u' ' + loc.toString(stat.errors()));
        m_errors->setShortcut(oldShortcut);
    }
    m_errors->setVisible(b);
    m_errorsSeparator->setVisible(b);

    QString cntstr = tr("Items") + u": " + loc.toString(stat.items())
            + u" (" + loc.toString(stat.lots()) + u")";
    m_count->setText(cntstr);

    QString wgtstr;
    if (qFuzzyCompare(stat.weight(), -std::numeric_limits<double>::min())) {
        wgtstr = u"-"_qs;
    } else {
        wgtstr = Utility::weightToString(std::abs(stat.weight()),
                                         Config::inst()->measurementSystem(),
                                         true /*optimize*/, true /*add unit*/);
        if (stat.weight() < 0)
            wgtstr.prepend(u"\u2265 "_qs);
    }
    m_weight->setText(wgtstr);

    QString valstr = Currency::toDisplayString(stat.value());
    if (stat.minValue() < stat.value())
        valstr.prepend(u"\u2264 "_qs);
    m_value->setText(valstr);

    b = !qFuzzyIsNull(stat.cost());
    if (b) {
        int percent = int(std::round(stat.value() / stat.cost() * 100. - 100.));
        QString profitstr = (percent > 0 ? u"(+" : u"(") + loc.toString(percent) + u" %)";
        m_profit->setText(profitstr);
    }
    m_profit->setVisible(/*b*/ false);
}

void ViewPane::updateBlockState(bool blocked)
{
    for (QToolButton *tb : { m_closeView, m_differences, m_errors, m_currency, m_filterOnOff })
        tb->setEnabled(!blocked);
    m_filter->setEnabled(!blocked);
}

View *ViewPane::newView(Document *doc)
{
    auto view = new View(doc, this);
    Q_ASSERT(view);
    connect(view, &QObject::destroyed,
            this, [this, view]() {
        // view is not a View anymore at this point ... just a QObject
        m_viewStackMapping.remove(m_viewStackMapping.key(view));
    });
    return view;
}

void ViewPane::focusFilter()
{
    if (m_filter->hasFocus() || !m_filterOnOff->isChecked())
        m_filterOnOff->defaultAction()->toggle();

    if (m_filterOnOff->isChecked())
        m_filter->setFocus(Qt::ShortcutFocusReason);
}

void ViewPane::setFilterFavoritesModel(QStringListModel *model)
{
    m_filter->setFavoritesModel(model);
}

void ViewPane::fontChange()
{
    // Windows' menu font is slightly larger, making the ViewPane look odd in comparison
    m_toolBar->parentWidget()->setFont(qApp->font("QMenuBar"));

    if (m_viewList) {
        QFont f = font();
        f.setBold(m_active);
        m_viewList->setFont(f);
    }
}

void ViewPane::paletteChange()
{
    if (!m_viewList || !m_viewListBackground)
        return;

    QPalette fgpal = QApplication::palette(m_viewList);
    QPalette bgpal = fgpal;

    QPalette ivp = QApplication::palette("QAbstractItemView");

    QColor ihigh = ivp.color(QPalette::Inactive, QPalette::Highlight);
    QColor itext = ivp.color(QPalette::Inactive, QPalette::HighlightedText);
    QColor iwin = bgpal.color(QPalette::Inactive, QPalette::Window);

    QColor high = ivp.color(QPalette::Active, QPalette::Highlight);
    QColor text = ivp.color(QPalette::Active, QPalette::HighlightedText);
    QColor win = bgpal.color(QPalette::Active, QPalette::Window);

    if (high == ihigh) {
        ihigh = iwin;
        itext = ivp.color(QPalette::Inactive, QPalette::Text);
    }

    if (m_active) {
        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectMode);
        gradient.setStops({ { 0, high },
                            { .8, Utility::gradientColor(high, win, 0.5) },
                            { 1, win } });
        bgpal.setBrush(QPalette::All, QPalette::Window, gradient);
        fgpal.setColor(QPalette::All, QPalette::Text, text);
        fgpal.setColor(QPalette::All, QPalette::ButtonText, text);
    } else {
        bgpal.setColor(QPalette::All, QPalette::Window, iwin);
        fgpal.setColor(QPalette::All, QPalette::Text, itext);
        fgpal.setColor(QPalette::All, QPalette::ButtonText, itext);
    }

    m_viewList->setPalette(fgpal);
    m_viewListBackground->setPalette(bgpal);
}

void ViewPane::languageChange()
{
    m_differences->setToolTip(ActionManager::toolTipLabel(tr("Go to the next difference"),
                                                          m_differences->shortcut()));
    m_errors->setToolTip(ActionManager::toolTipLabel(tr("Go to the next error"), m_errors->shortcut()));

    m_order->setToolTip(tr("Show order information"));
    m_value->setText(tr("Currency:"));

    m_split->setToolTip(tr("Split"));
    m_splitH->setText(tr("Split horizontally"));
    m_splitV->setText(tr("Split vertically"));
    m_splitClose->setText(tr("Remove split"));
    m_splitWindow->setText(tr("Open in new window"));

    updateStatistics();
}

void ViewPane::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        languageChange();
    else if (e->type() == QEvent::PaletteChange)
        paletteChange();
    else if (e->type() == QEvent::FontChange)
        fontChange();
    QWidget::changeEvent(e);
}

void ViewPane::createToolBar()
{
    m_toolBar = new QWidget();
    m_toolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto pageLayout = new QHBoxLayout(m_toolBar);
    pageLayout->setContentsMargins(0, 0, 4, 0);
    pageLayout->setSpacing(4);

    auto addSeparator = [&pageLayout]() -> QWidget * {
            auto sep = new QFrame();
            sep->setFrameShape(QFrame::VLine);
            auto pal = sep->palette();
            pal.setColor(QPalette::WindowText, pal.color(QPalette::Disabled, QPalette::WindowText));
            sep->setPalette(pal);
            sep->setFixedWidth(sep->frameWidth());
            sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
            pageLayout->addWidget(sep);
            return sep;
    };

    m_viewList = new MenuComboBox();

    m_viewList->setModel(DocumentList::inst());
    m_viewList->setMinimumContentsLength(12);
    m_viewList->setMaxVisibleItems(40);
    m_viewList->setSizePolicy({ QSizePolicy::Expanding, QSizePolicy::Expanding });
    m_viewList->setFocusPolicy(Qt::NoFocus);

    m_viewListBackground = new QWidget();
    m_viewListBackground->setAutoFillBackground(true);
    m_viewListBackground->setBackgroundRole(QPalette::Window);

    auto viewListStack = new QStackedLayout();
    viewListStack->setStackingMode(QStackedLayout::StackAll);
    pageLayout->addLayout(viewListStack, 1);

    viewListStack->addWidget(m_viewList);
    viewListStack->addWidget(m_viewListBackground);

    addSeparator();

    m_closeView = new QToolButton();
    m_closeView->setIcon(QIcon::fromTheme(ActionManager::inst()->action("document_close")->iconName()));
    m_closeView->setAutoRaise(true);
    m_closeView->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_closeView);

    connect(m_closeView, &QToolButton::clicked, this, [this]() {
        if (auto *view = activeView())
            view->document()->requestClose();
    });

    m_orderSeparator = addSeparator();
    m_order = new QToolButton();
    m_order->setIcon(QIcon::fromTheme(ActionManager::inst()->action("document_import_bl_order")->iconName()));
    m_order->setAutoRaise(true);
    m_order->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_order);

    connect(m_order, &QToolButton::clicked,
            this, [this]() {
        if (m_view && m_view->document()->order()) {
            auto *dlg = new OrderInformationDialog(m_view->document()->order(), this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->setWindowModality(Qt::ApplicationModal);
            dlg->show();
        }
    });

    m_differencesSeparator = addSeparator();
    m_differences = new QToolButton();
    m_differences->setDefaultAction(ActionManager::inst()->qAction("view_goto_next_diff"));
    connect(m_differences->defaultAction(), &QAction::changed,
            this, &ViewPane::updateStatistics);
    m_differences->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_differences->setAutoRaise(true);
    m_differences->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_differences);

    m_errorsSeparator = addSeparator();
    m_errors = new QToolButton();
    m_errors->setDefaultAction(ActionManager::inst()->qAction("view_goto_next_input_error"));
    connect(m_errors->defaultAction(), &QAction::changed,
            this, &ViewPane::updateStatistics);
    m_errors->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_errors->setAutoRaise(true);
    m_errors->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_errors);

    addSeparator();
    m_count = new CollapsibleLabel();
    pageLayout->addWidget(m_count, 1'000'000);

    addSeparator();
    m_weight = new CollapsibleLabel();
    pageLayout->addWidget(m_weight, 1);

    addSeparator();

    auto *currencyLayout = new QHBoxLayout();
    currencyLayout->setSpacing(0);
    currencyLayout->setSizeConstraint(QLayout::SetMaximumSize);
    m_value = new CollapsibleLabel();
    currencyLayout->addWidget(m_value);
    m_currency = new QToolButton();
    m_currency->setPopupMode(QToolButton::InstantPopup);
    m_currency->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_currency->setAutoRaise(true);
    m_currency->setFocusPolicy(Qt::NoFocus);
    currencyLayout->addWidget(m_currency);
    pageLayout->addLayout(currencyLayout, 10'000);
    m_profit = new CollapsibleLabel();
    m_profit->hide();
    pageLayout->addWidget(m_profit, 100);

    addSeparator();

    m_filterOnOff = new QToolButton();
    m_filterOnOff->setDefaultAction(m_filter->action());
    m_filterOnOff->setAutoRaise(true);
    m_filterOnOff->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_filterOnOff);

    addSeparator();

    m_split = new QToolButton();
    m_split->setIcon(QIcon::fromTheme(u"view-split-left-right"_qs));
    m_split->setAutoRaise(true);
    m_split->setPopupMode(QToolButton::InstantPopup);
    m_split->setProperty("noMenuArrow", true);
    m_split->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_split);

    m_splitH = new QAction(this);
    m_splitH->setIcon(QIcon::fromTheme(u"view-split-left-right"_qs));
    m_splitV = new QAction(this);
    m_splitV->setIcon(QIcon::fromTheme(u"view-split-top-bottom"_qs));
    m_splitClose = new QAction(this);
    m_splitClose->setIcon(QIcon::fromTheme(u"view-close"_qs));
    m_splitWindow = new QAction(this);
    m_splitWindow->setIcon(QIcon::fromTheme(u"document-new"_qs));

    auto *splitMenu = new QMenu(m_split);
    splitMenu->addAction(m_splitClose);
    splitMenu->addSeparator();
    splitMenu->addAction(m_splitH);
    splitMenu->addAction(m_splitV);
    splitMenu->addSeparator();
    splitMenu->addAction(m_splitWindow);
    m_split->setMenu(splitMenu);

    connect(splitMenu, &QMenu::aboutToShow, this, [this]() {
        m_splitClose->setEnabled(canUnsplit());
    });

    connect(m_splitH, &QAction::triggered, this, [this]() { split(Qt::Horizontal); });
    connect(m_splitV, &QAction::triggered, this, [this]() { split(Qt::Vertical); });
    connect(m_splitClose, &QAction::triggered, this, [this]() { unsplit(); }, Qt::QueuedConnection);
    connect(m_splitWindow, &QAction::triggered, this, [this]() { newWindow(); }, Qt::QueuedConnection);

    // workaround for flicker
    connect(m_filter, &FilterWidget::visibilityChanged,
            this, [this](bool) {
        layout()->invalidate();
    });
}

//#include "moc_viewpane.cpp"  // QTBUG-98845 //TODO: fixed in 6.5
//#include "viewpane.moc"
