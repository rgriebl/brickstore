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

#include "common/actionmanager.h"
#include "common/document.h"
#include "common/documentlist.h"
#include "common/documentmodel.h"
#include "utility/utility.h"
#include "changecurrencydialog.h"
#include "desktopuihelpers.h"
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

    QSize minimumSizeHint() const override
    {
        return QSize(0, 0);
    }
};


ViewPane::ViewPane(std::function<ViewPane *(Document *)> viewPaneCreate, std::function<void (ViewPane *)> viewPaneDelete, Document *activeDocument)
    : QWidget()
    , m_viewPaneCreate(viewPaneCreate)
    , m_viewPaneDelete(viewPaneDelete)
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

    auto setIconSizeLambda = [this](Config::IconSize iconSize) {
        static const QMap<Config::IconSize, QStyle::PixelMetric> map = {
            { Config::IconSize::System, QStyle::PM_SmallIconSize },
            { Config::IconSize::Small, QStyle::PM_SmallIconSize },
            { Config::IconSize::Large, QStyle::PM_ToolBarIconSize },
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

    setupViewStack();

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
    rootSplitter->setObjectName("WindowSplitter"_l1);
    auto *vp = m_viewPaneCreate(nullptr);
    rootSplitter->addWidget(vp);
    nw->setCentralWidget(rootSplitter);
    nw->show();
    vp->activateDocument(activeDocument());

    connect(vp->m_viewStack, &QStackedWidget::widgetRemoved, this, [nw, vp]() {
        if (vp->m_viewStack->count() == 0)
            nw->close();
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
        newSplitter->insertWidget(1, m_viewPaneCreate(activeDocument()));
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

            m_viewPaneDelete(this);
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
        auto *doc = m_viewList->itemData(docIdx).value<Document *>();
        if (doc)
            activateDocument(doc);
        m_viewList->setToolTip(doc ? doc->fileNameOrTitle() : QString());
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

/*  for (int i = 0; i < 9; ++i) {
        //: Shortcut to activate window 0-9
        auto sc = new QShortcut(tr("Alt+%1").arg(i), this);
        connect(sc, &QShortcut::activated, this, [this, i]() {
            const int j = (i == 0) ? 9 : i - 1;
            if (j < m_viewStack->count())
                m_viewStack->setCurrentIndex(j);
        });
    }  */
}

void ViewPane::setView(View *view)
{
    delete m_viewConnectionContext;
    m_viewConnectionContext = nullptr;
    m_view = nullptr;
    m_model = nullptr;
    m_filter->setDocument(nullptr);
    //m_filter->setFilter({ });
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

    bool hasOrder = (view->document()->order());
    m_order->setVisible(hasOrder);
    m_orderSeparator->setVisible(hasOrder);

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

    foreach (const QString &c, Currency::inst()->currencyCodes()) {
        auto a = m->addAction(c);
        a->setObjectName(c);
        if (c == m_model->currencyCode())
            a->setEnabled(false);
    }
    m_currency->setMenu(m);
}

void ViewPane::documentCurrencyChanged(const QString &ccode)
{
    m_currency->setText(ccode + "  "_l1);
    // the menu might still be open right now, so we need to delay deleting the actions
    QMetaObject::invokeMethod(this, &ViewPane::updateCurrencyRates, Qt::QueuedConnection);
}

void ViewPane::changeDocumentCurrency(QAction *a)
{
    if (!m_model)
        return;

    QString ccode = a->objectName();

    if (ccode != m_model->currencyCode()) {
        ChangeCurrencyDialog d(m_model->currencyCode(), ccode, m_model->legacyCurrencyCode(), this);
        if (d.exec() == QDialog::Accepted) {
            double rate = d.exchangeRate();

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
        m_differences->setText(u' ' % loc.toString(stat.differences()));
        m_differences->setShortcut(oldShortcut);
    }
    m_differences->setVisible(b);
    m_differencesSeparator->setVisible(b);

    b = (stat.errors() > 0);
    if (b && Config::inst()->showInputErrors()) {
        auto oldShortcut = m_errors->shortcut();
        m_errors->setText(u' ' % loc.toString(stat.errors()));
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
    if (!m_viewList)
        return;

    QPalette pal = QApplication::palette(m_viewList);

    QPalette ivp = QApplication::palette("QAbstractItemView");

    QColor ihigh = ivp.color(QPalette::Inactive, QPalette::Highlight);
    QColor itext = ivp.color(QPalette::Inactive, QPalette::HighlightedText);
    QColor iwin = pal.color(QPalette::Inactive, QPalette::Window);

    QColor high = ivp.color(QPalette::Active, QPalette::Highlight);
    QColor text = ivp.color(QPalette::Active, QPalette::HighlightedText);
    QColor win = pal.color(QPalette::Active, QPalette::Window);

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
        pal.setBrush(QPalette::All, QPalette::Window, gradient);
        pal.setColor(QPalette::All, QPalette::Text, text);
    } else {
        pal.setColor(QPalette::All, QPalette::Window, iwin);
        pal.setColor(QPalette::All, QPalette::Text, itext);
    }

    m_viewList->setBackgroundRole(QPalette::Window);    
    m_viewList->setPalette(pal);
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

void ViewPane::keyPressEvent(QKeyEvent *e)
{
    int d = DesktopUIHelpers::shouldSwitchViews(e);
    if (d) {
        int cnt = m_viewList->count();
        if (cnt > 1) {
            int idx = (m_viewList->currentIndex() + d + cnt) % cnt;
            m_viewList->setCurrentIndex(idx);
        }
        e->accept();
    }
    QWidget::keyPressEvent(e);
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
    m_viewList->setAutoFillBackground(true);
    m_viewList->setMinimumContentsLength(12);
    m_viewList->setMaxVisibleItems(40);
    m_viewList->setSizePolicy({ QSizePolicy::Expanding, QSizePolicy::Expanding });
    m_viewList->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_viewList, 1);
    addSeparator();

    m_closeView = new QToolButton();
    m_closeView->setIcon(QIcon::fromTheme(ActionManager::inst()->action("document_close")->iconName()));
    m_closeView->setAutoRaise(true);
    m_closeView->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_closeView);

    connect(m_closeView, &QToolButton::clicked, this, [this]() {
        if (auto *view = activeView())
            view->close();
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
            OrderInformationDialog d(m_view->document()->order(), this);
            d.exec();
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

    QHBoxLayout *currencyLayout = new QHBoxLayout();
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
    m_split->setIcon(QIcon::fromTheme("view-split-left-right"_l1));
    m_split->setAutoRaise(true);
    m_split->setPopupMode(QToolButton::InstantPopup);
    m_split->setProperty("noMenuArrow", true);
    m_split->setFocusPolicy(Qt::NoFocus);
    pageLayout->addWidget(m_split);

    m_splitH = new QAction(this);
    m_splitH->setIcon(QIcon::fromTheme("view-split-left-right"_l1));
    m_splitV = new QAction(this);
    m_splitV->setIcon(QIcon::fromTheme("view-split-top-bottom"_l1));
    m_splitClose = new QAction(this);
    m_splitClose->setIcon(QIcon::fromTheme("view-close"_l1));
    m_splitWindow = new QAction(this);
    m_splitWindow->setIcon(QIcon::fromTheme("document-new"_l1));

    QMenu *splitMenu = new QMenu(m_split);
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

//#include "moc_viewpane.cpp"  // why does this not work?
//#include "viewpane.moc"
