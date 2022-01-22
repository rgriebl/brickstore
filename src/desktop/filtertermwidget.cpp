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
#include <QComboBox>
#include <QLineEdit>
#include <QToolButton>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QBoxLayout>
#include <QStringListModel>
#include <QFocusEvent>
#include <QPainter>
#include <QStackedLayout>
#include <QToolButton>
#include <QGuiApplication>
#include <QClipboard>
#include <QMenu>
#include <QTimer>

#include "common/document.h"
#include "utility/utility.h"
#include "desktopuihelpers.h"
#include "filtertermwidget.h"
#include "flowlayout.h"
#include "historylineedit.h"
#include "menucombobox.h"
#include "view.h"

using namespace std::chrono_literals;

static QVector<int> visualColumnOrder(const QVector<ColumnData> &columns)
{
    QVector<int> result;
    result.resize(columns.size());

    for (int li = 0; li < columns.size(); ++li)
        result[columns[li].m_visualIndex] = li;
    return result;
}


FilterTermWidget::FilterTermWidget(Document *doc, const Filter &filter, QWidget *parent)
    : QWidget(parent)
    , m_parser(doc ? doc->model()->filterParser() : nullptr)
    , m_doc(doc)
    , m_filter(filter)
{
   // setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    m_valueDelay = new QTimer(this);
    m_valueDelay->setInterval(800ms);
    m_valueDelay->setSingleShot(true);

    m_fields = new MenuComboBox();
    m_fields->setObjectName("filter-fields"_l1);
    m_fields->view()->setFont(m_fields->font());
    m_comparisons = new MenuComboBox();
    m_comparisons->setObjectName("filter-comparisons"_l1);
    m_value = new QComboBox();
    m_value->setEditable(true);
    m_value->setMinimumContentsLength(16);
    m_value->setInsertPolicy(QComboBox::NoInsert);
    m_value->setMaxVisibleItems(30);
    m_value->lineEdit()->setPlaceholderText(tr("Filter expression"));
    m_value->lineEdit()->setClearButtonEnabled(true);
    m_value->setItemDelegate(new QStyledItemDelegate(m_value));
    m_value->setProperty("transparentCombo", true);
    m_value->setObjectName("filter-value"_l1);
    auto bdel = new QToolButton();
    bdel->setAutoRaise(true);
    bdel->setText(QString(QChar(0x00d7)));
    bdel->setObjectName("filter-delete"_l1);
    auto band = new QToolButton();
    band->setAutoRaise(true);
    band->setCheckable(true);
    band->setObjectName("filter-and"_l1);
    auto bor = new QToolButton();
    bor->setAutoRaise(true);
    bor->setCheckable(true);
    bor->setObjectName("filter-or"_l1);

    setFocusProxy(m_value);

    m_andOrGroup = new QButtonGroup(this);
    m_andOrGroup->addButton(band, int(Filter::Combination::And));
    m_andOrGroup->addButton(bor, int(Filter::Combination::Or));

    auto layout = new QHBoxLayout(this);
    int d = this->fontMetrics().height() / 2;
    layout->setContentsMargins(d, 0, d, 0);
    layout->setSpacing(2);

    layout->addWidget(bdel);
    int frameStyle = int(QFrame::VLine);
    auto f = new QFrame();
    f->setFrameStyle(frameStyle);
    f->setDisabled(true);
    layout->addWidget(f);
    layout->addWidget(m_fields);
    f = new QFrame();
    f->setFrameStyle(frameStyle);
    f->setDisabled(true);
    layout->addWidget(f);
    layout->addWidget(m_comparisons);
    f = new QFrame();
    f->setFrameStyle(frameStyle);
    f->setDisabled(true);
    layout->addWidget(f);
    layout->addWidget(m_value, 10);
    f = new QFrame();
    f->setFrameStyle(frameStyle);
    f->setDisabled(true);
    layout->addWidget(f);
    layout->addWidget(band);
    layout->addWidget(bor);

    auto populateFields = [this]() {
        auto newOrder = visualColumnOrder(m_doc->columnLayout());

        if (newOrder == m_visualColumnOrder)
            return;
        m_visualColumnOrder = newOrder;

        int oldField = m_filter.field();

        auto unblock = m_fields->blockSignals(true);

        m_fields->clear();

        const auto anyToken = m_parser->fieldTokens().constFirst();
        m_fields->insertItem(m_fields->count(), anyToken.second, anyToken.first);

        for (int col : m_visualColumnOrder) {
            if (m_doc->model()->headerData(col, Qt::Horizontal, DocumentModel::HeaderFilterableRole).toBool()) {
                auto colName = m_doc->model()->headerData(col, Qt::Horizontal).toString();
                m_fields->insertItem(m_fields->count(), colName, col);
            }
        }
        m_fields->setMaxVisibleItems(m_fields->count() + 1);

        m_fields->blockSignals(unblock);

        m_fields->setCurrentIndex(qMax(0, m_fields->findData(oldField)));
    };
    connect(m_doc, &Document::columnPositionChanged,
            this, populateFields);
    connect(m_doc, &Document::columnHiddenChanged,
            this, populateFields);
    populateFields();

    const auto compTokens = m_parser->comparisonTokens();
    Filter::Comparisons comp { };
    for (const auto &token : compTokens) {
        if (comp & token.first)
            continue;
        comp |= token.first;
        m_comparisons->insertItem(m_comparisons->count(), token.second, token.first);
        m_comparisons->setItemData(m_comparisons->count() - 1, false, Qt::CheckStateRole);
        if (filter.comparison() == token.first)
            m_comparisons->setCurrentIndex(m_comparisons->count() - 1);
    }
    m_comparisons->setMaxVisibleItems(m_comparisons->count());

    const auto combTokens = m_parser->combinationTokens();
    Filter::Combinations comb;
    for (const auto &token : combTokens) {
        if (comb & token.first)
            continue;
        comb |= token.first;
        bool check = (filter.combination() == token.first);
        if (token.first == Filter::Combination::And) {
            band->setText(token.second);
            band->setChecked(check);
        } else if (token.first == Filter::Combination::Or) {
            bor->setText(token.second);
            bor->setChecked(check);
        }
    }

    connect(bdel, &QToolButton::clicked, this, &FilterTermWidget::deleteClicked);
    connect(m_andOrGroup, &QButtonGroup::idToggled, this, [=, this](int id, bool checked) {
        if ((id > 0) && checked) {
            emit combinationChanged(static_cast<Filter::Combination>(id));
            emitFilterChanged();
        }
    });
    connect(m_fields, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [&](int index) {
        updateValueModel(m_fields->itemData(index).toInt());
        emitFilterChanged();
        m_value->setFocus();
    });
    updateValueModel(filter.field());

    int valueIndex = m_value->findText(filter.expression(), Qt::MatchFixedString);
    if (valueIndex >= 0)
        m_value->setCurrentIndex(valueIndex);
    else
        m_value->setCurrentText(filter.expression());

    connect(m_comparisons, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [&]() {
        emitFilterChanged();
        m_value->setFocus();
    });
    connect(m_value, &QComboBox::currentTextChanged, this, [this]() {
        m_valueDelay->start();
    });
    connect(m_valueDelay, &QTimer::timeout,
            this, &FilterTermWidget::emitFilterChanged);

    m_value->installEventFilter(this);
    m_value->lineEdit()->installEventFilter(this);
}

void FilterTermWidget::updateValueModel(int field)
{
    auto model = m_doc->model()->headerData(field, Qt::Horizontal,
                                            DocumentModel::HeaderValueModelRole)
            .value<QAbstractItemModel *>();
    QString oldEditText = m_value->currentText();
    if (model) {
        model->setParent(m_value); // the combobox now owns the model
        m_value->setModel(model);
        m_value->setCurrentIndex(-1);
    } else {
        m_value->setModel(new QStringListModel(m_value));
    }
    m_value->setEditText(oldEditText);
}

bool FilterTermWidget::eventFilter(QObject *o, QEvent *e)
{
    if ((o == m_value) && (e->type() == QEvent::FocusIn)) {
        auto fe = static_cast<QFocusEvent *>(e);
        static const QVector<Qt::FocusReason> validReasons = {
            Qt::MouseFocusReason, Qt::TabFocusReason, Qt::BacktabFocusReason,
            Qt::ShortcutFocusReason, Qt::OtherFocusReason
        };
        if (validReasons.contains(fe->reason())) {
            m_nextMouseUpOpensPopup = (fe->reason() == Qt::MouseFocusReason);
            if (!m_nextMouseUpOpensPopup) {
                QMetaObject::invokeMethod(m_value->lineEdit(), &QLineEdit::selectAll,
                                          Qt::QueuedConnection);
                m_value->showPopup();
            }
        }
    } else if ((e->type() == QEvent::MouseButtonRelease)
               && m_nextMouseUpOpensPopup
               && ((o == m_value) || (o == m_value->lineEdit()))) {
        m_value->showPopup();
        m_nextMouseUpOpensPopup = false;
    }

    return QWidget::eventFilter(o, e);
}

void FilterTermWidget::paintEvent(QPaintEvent *)
{
    QColor c = palette().color(QPalette::Window);
    bool dark = ((c.lightnessF() * c.alphaF()) < 0.5);

    QPainter p(this);
    QLinearGradient g(0, 0, 0, 1);
    g.setCoordinateMode(QGradient::ObjectMode);
    g.setColorAt(0, dark ? QColor::fromRgbF(1, 1, 1, 0.3f)
                         : QColor::fromRgbF(0, 0, 0, 0.1f));
    g.setColorAt(1, dark ? QColor::fromRgbF(1, 1, 1, 0.1f)
                         : QColor::fromRgbF(0, 0, 0, 0.3f));

    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(rect().adjusted(0, 0, 1, 1), height() / 2, height() / 2);
}

void FilterTermWidget::resetCombination()
{
    if (auto b = m_andOrGroup->checkedButton()) {
        m_andOrGroup->setExclusive(false);
        b->setChecked(false);
        m_andOrGroup->setExclusive(true);
    }
}

const Filter &FilterTermWidget::filter() const
{
    return m_filter;
}

QString FilterTermWidget::filterString() const
{
    QString s = m_fields->currentText() % u' ' % m_comparisons->currentText() %
            u" '" % m_value->currentText() % u"'";
    if (m_andOrGroup->checkedId() >= 0)
        s = s % u' ' % m_andOrGroup->checkedButton()->text();
    return s;
}

void FilterTermWidget::emitFilterChanged()
{
    Filter f;

    f.setField(m_fields->currentData().toInt());
    f.setComparison(static_cast<Filter::Comparison>(m_comparisons->currentData().toInt()));
    f.setExpression(m_value->currentText());
    int combId = m_andOrGroup->checkedId();
    f.setCombination(combId == -1 ? Filter::Combination::And : static_cast<Filter::Combination>(combId));

    if (f != m_filter) {
        m_filter = f;
        emit filterChanged(m_filter);
    }
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


FilterWidget::FilterWidget(QWidget *parent)
    : QWidget(parent)
{
    m_onOff = new QAction(this);
    m_onOff->setIcon(QIcon::fromTheme("view-filter"_l1));
    m_onOff->setCheckable(true);

    m_editDelay = new QTimer(this);
    m_editDelay->setInterval(800ms);
    m_editDelay->setSingleShot(true);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 4);
    layout->setSpacing(4);

    m_refilter = new QToolButton();
    m_refilter->setIcon(QIcon::fromTheme("view-refresh"_l1));
    m_refilter->setAutoRaise(true);
    QSizePolicy sp = m_refilter->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    m_refilter->setSizePolicy(sp);
    m_refilter->setVisible(false);
    layout->addWidget(m_refilter);

    m_termsContainer = new QWidget();
    m_termsFlow = new FlowLayout(m_termsContainer, FlowLayout::HorizontalFirst, 0, 4, 1);
//    m_termsFlow = new QVBoxLayout(m_termsContainer);
//    m_termsFlow->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_termsContainer, 1);

    m_edit = new HistoryLineEdit();
    m_edit->hide();
    m_edit->installEventFilter(DesktopUIHelpers::selectAllFilter());
    layout->addWidget(m_edit, 1);

    m_menu = new QToolButton();
    m_menu->setIcon(QIcon::fromTheme("overflow-menu"_l1));
    m_menu->setProperty("noMenuArrow", true);
    m_menu->setAutoRaise(true);
    layout->addWidget(m_menu);

    auto *filterMenu = new QMenu(this);
    m_copy = filterMenu->addAction(QIcon::fromTheme("edit-copy"_l1), { });
    m_paste = filterMenu->addAction(QIcon::fromTheme("edit-paste"_l1), { });
    filterMenu->addSeparator();
    m_mode = filterMenu->addAction(QString());
    m_mode->setCheckable(true);
    m_mode->setChecked(false);

    m_menu->setMenu(filterMenu);
    m_menu->setPopupMode(QToolButton::InstantPopup);

    connect(m_refilter, &QToolButton::clicked,
            this, [this]() {
        if (m_doc)
            m_doc->model()->reFilter();
    });

    connect(m_copy, &QAction::triggered,
            this, [this]() {
        if (m_doc) {
            QString s = m_doc->model()->filterParser()->toString(m_filter, true /*symbolic*/);
            QGuiApplication::clipboard()->setText(s);
        }
    });
    connect(m_paste, &QAction::triggered,
            this, [this]() {
        if (m_doc) {
            QString s = QGuiApplication::clipboard()->text();
            auto filter = m_doc->model()->filterParser()->parse(s);
            setFilter(filter);
        }
    });
    connect(m_mode, &QAction::toggled,
            this, [this](bool textFilter) {
        m_termsContainer->setVisible(!textFilter);
        m_edit->setVisible(textFilter);
        setFocusProxy(textFilter ? m_edit : m_termsContainer);
    });
    connect(m_edit, &QLineEdit::textEdited, this, [this]() {
        m_editDelay->start();
    });
    connect(m_editDelay, &QTimer::timeout,
            this, [this]() {
        if (m_doc && m_edit->isVisible()) {
            auto filter = m_doc->model()->filterParser()->parse(m_edit->text());

            bool error = (!m_edit->text().isEmpty() && filter.isEmpty());
            m_edit->setProperty("showInputError", error ? QVariant(true) : QVariant());

            if (!error)
                emitFilterChanged(filter, ChangedVia::Edit);
        }
    });

    languageChange();
    setFocusProxy(m_termsContainer);
}

void FilterWidget::setIconSize(const QSize &s)
{
    m_refilter->setIconSize(s);
    m_menu->setIconSize(s);
}

void FilterWidget::setDocument(Document *doc)
{
    delete m_viewConnectionContext;
    m_viewConnectionContext = nullptr;

    m_doc = doc;
    if (m_doc) {
        m_viewConnectionContext = new QObject(this);

        connect(m_onOff, &QAction::toggled,
                m_viewConnectionContext, [this](bool checked) {
            if (!checked) {
                auto lastFilter = m_doc->model()->filter();
                setFilter({ });
                m_doc->setProperty("_viewPaneLastFilter", QVariant::fromValue(lastFilter));
            } else {
                if (m_doc->model()->filter().isEmpty()) {
                    auto lastFilter = m_doc->property("_viewPaneLastFilter").value<QVector<Filter>>();
                    if (lastFilter.isEmpty())
                        lastFilter.append(Filter { });
                    setFilter(lastFilter);
                }
            }

            setVisible(checked);
            emit visibilityChanged(checked);
        });
        connect(m_doc->model(), &DocumentModel::isFilteredChanged,
                m_viewConnectionContext, [this](bool isFiltered) {
            bool hasFilters = !m_doc->model()->filter().isEmpty();
            m_onOff->setChecked(hasFilters);
            m_refilter->setVisible(!isFiltered && hasFilters);
        });
        connect(m_doc->model(), &DocumentModel::filterChanged,
                m_viewConnectionContext, [this](const QVector<Filter> &filter) {
            m_onOff->setChecked(!filter.isEmpty());
            if (filter != m_filter)
                setFilterFromModel();
        });
        setFilterFromModel();

        m_edit->setToolTip(m_doc->model()->filterParser()->toolTip());
    }
}

QAction *FilterWidget::action()
{
    return m_onOff;
}

void FilterWidget::languageChange()
{
    m_onOff->setToolTip(tr("Filter"));
    m_refilter->setToolTip(tr("Re-apply filter"));
    m_menu->setToolTip(tr("More actions for this filter"));
    m_copy->setText(tr("Copy filter"));
    m_paste->setText(tr("Paste filter"));
    m_mode->setText(tr("Text-only filter mode"));
    if (m_doc)
        m_edit->setToolTip(m_doc->model()->filterParser()->toolTip());
}

void FilterWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        languageChange();
}

void FilterWidget::emitFilterChanged(const QVector<Filter> &filter, ChangedVia changedVia)
{
    if (filter != m_filter) {
        if ((changedVia == ChangedVia::Terms) || changedVia == (ChangedVia::Externally))
            setFilterText(filter);
        if ((changedVia == ChangedVia::Edit) || changedVia == (ChangedVia::Externally))
            setFilterTerms(filter);

        m_filter = filter;
        if (m_doc)
            m_doc->model()->setFilter(m_filter);

        m_onOff->setChecked(!filter.isEmpty());
    }
}

QVector<Filter> FilterWidget::filterFromTerms() const
{
    QVector<Filter> result;

    const auto ftws = m_termsContainer->findChildren<FilterTermWidget *>();
    for (const auto &ftw : ftws) {
        if (m_termsFlow->indexOf(ftw) >= 0)
            result << ftw->filter();
    }

    // remove any trailing and'ed default filters
    for (int i = result.size() - 1; i > 0; ++i) {
        if ((result.at(i - 1).combination() == Filter::Combination::And)
                && (result.at(i) == Filter { })) {
            result.removeLast();
        } else {
            break;
        }
    }
    return result;
}

void FilterWidget::setFilterFromModel()
{
    m_filter = m_doc->model()->filter();
    setFilterTerms(m_filter);
    setFilterText(m_filter);
    m_onOff->setChecked(!m_filter.isEmpty());
}

void FilterWidget::addFilterTerm(const Filter &filter)
{
    auto ftw = new FilterTermWidget(m_doc, filter);
    ftw->resetCombination();
    m_termsFlow->addWidget(ftw);
    m_termsContainer->setFocusProxy(ftw);

    connect(ftw, &FilterTermWidget::deleteClicked, this, [ftw, this]() {
        bool wasLast = (m_termsFlow->indexOf(ftw) == (m_termsFlow->count() - 1));
        ftw->deleteLater();
        m_termsFlow->removeWidget(ftw);
        if (wasLast && m_termsFlow->count()) {
            auto lastFtw = static_cast<FilterTermWidget *>(m_termsFlow->itemAt(m_termsFlow->count() - 1)->widget());
            if (lastFtw) {
                lastFtw->resetCombination();
                m_termsContainer->setFocusProxy(lastFtw);
            }
        }

        emitFilterChanged(filterFromTerms(), ChangedVia::Terms);

        if (!m_termsFlow->count())
            m_onOff->setChecked(false);
    });
    connect(ftw, &FilterTermWidget::combinationChanged, this, [this, ftw](Filter::Combination) {
        if (m_termsFlow->indexOf(ftw) == (m_termsFlow->count() - 1))
            addFilterTerm();
    });
    connect(ftw, &FilterTermWidget::filterChanged,
            this, [this]() {
        emitFilterChanged(filterFromTerms(), ChangedVia::Terms);
    });
}

void FilterWidget::setFilter(const QVector<Filter> &filter)
{
    emitFilterChanged(filter, ChangedVia::Externally);
}

void FilterWidget::setFilterTerms(const QVector<Filter> &filter)
{
    qDeleteAll(m_termsContainer->findChildren<FilterTermWidget *>());
    for (const auto &f : filter)
        addFilterTerm(f);
    if (filter.isEmpty())
        addFilterTerm({ });
}

void FilterWidget::setFilterText(const QVector<Filter> &filter)
{
    QSignalBlocker blockEdit(m_edit);
    if (m_doc)
        m_edit->setText(m_doc->model()->filterParser()->toString(filter, true /*symbolic*/));
}

#include "moc_filtertermwidget.cpp"
