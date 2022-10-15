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

#include <QQmlEngine>
#include <QQmlContext>
#include <QFile>
#include <QUrl>
#include <QGuiApplication>

#include "utility/utility.h"
#include "common/currency.h"
#include "bricklink/order.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/document.h"
#include "common/documentmodel.h"
#include "common/documentio.h"
#include "common/recentfiles.h"
#include "brickstore_wrapper.h"
#include "version.h"


/*! \qmltype BrickStore
    \inherits QtObject
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This singleton represents global settings and state.

    A kitchen sink type singleton for all global state, settings and utility functions.
*/

/*! \qmlsignal BrickStore::showSettings(string page)
    \internal
*/

/*! \qmlproperty string BrickStore::defaultCurrencyCode
    \readonly
    The user's default ISO currency code (e.g. \c EUR).
*/
/*! \qmlproperty string BrickStore::versionNumber
    \readonly
    BrickStore's version as a string (e.g. \c "2021.10.2").
*/
/*! \qmlproperty string BrickStore::buildNumber
    \readonly
    BrickStore's build number as a string (e.g. \c "42").
*/
/*! \qmlproperty bool BrickStore::databaseValid
    \readonly
    Returns whether the current database is valid or not.
*/
/*! \qmlproperty date BrickStore::lastDatabaseUpdate
    \readonly
    This property holds the date and time of the last successful database update.
*/
/*! \qmlproperty bool BrickStore::online
    \readonly
    The current online state of the application. This is mirroring the operating system's online
    state.
*/
/*! \qmlproperty Document BrickStore::activeDocument
    \readonly
    The currently active document, i.e. the document the user is working on. Can be \c null, if
    no documents are open, but also if the quickstart page is active.
*/

QmlBrickStore *QmlBrickStore::s_inst = nullptr;

QmlBrickStore::QmlBrickStore()
    : QObject()
    , m_docList(new QmlDocumentList(this))
    , m_columnLayouts(new ColumnLayoutsModel(this))
{
    setObjectName(u"BrickStore"_qs);

    connect(Application::inst(), &Application::showSettings,
            this, &QmlBrickStore::showSettings);

    connect(ActionManager::inst(), &ActionManager::activeDocumentChanged,
            this, [this](Document *doc) {
        emit activeDocumentChanged(m_docList->map(doc));
    });

    connect(Config::inst(), &Config::defaultCurrencyCodeChanged,
            this, &QmlBrickStore::defaultCurrencyCodeChanged);
}

QmlBrickStore *QmlBrickStore::inst()
{
    return s_inst;
}

QmlBrickStore *QmlBrickStore::create(QQmlEngine *, QJSEngine *)
{
    if (!s_inst) {
        s_inst = new QmlBrickStore();
        QJSEngine::setObjectOwnership(s_inst, QJSEngine::CppOwnership);
    }
    return s_inst;
}

QmlDocumentList *QmlBrickStore::documents() const
{
    return m_docList;
}

Config *QmlBrickStore::config() const
{
    return Config::inst();
}

QString QmlBrickStore::versionNumber() const
{
    return QLatin1String(BRICKSTORE_VERSION);
}

QString QmlBrickStore::buildNumber() const
{
    return QLatin1String(BRICKSTORE_BUILD_NUMBER);
}

RecentFiles *QmlBrickStore::recentFiles() const
{
    return RecentFiles::inst();
}

ColumnLayoutsModel *QmlBrickStore::columnLayouts() const
{
    return m_columnLayouts;
}

QVariantMap QmlBrickStore::about() const
{
    return Application::inst()->about();
}

QmlDebug *QmlBrickStore::debug() const
{
    if (!m_debug)
        m_debug = new QmlDebug(const_cast<QmlBrickStore *>(this));
    return m_debug;
}

QString QmlBrickStore::defaultCurrencyCode() const
{
    return Config::inst()->defaultCurrencyCode();
}

/*! \qmlmethod string BrickStore::symbolForCurrencyCode(string currencyCode)

    Returns the currency symbol for the ISO \a currencyCode if available or the \a currencyCode
    itself otherwise. E.g. \c EUR will be mapped to \c €.
*/
QString QmlBrickStore::symbolForCurrencyCode(const QString &currencyCode) const
{
    static QHash<QString, QString> cache;
    QString s = cache.value(currencyCode);
    if (s.isEmpty()) {
         const auto allLoc = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript,
                                                      QLocale::AnyCountry);

         for (auto &loc : allLoc) {
             if (loc.currencySymbol(QLocale::CurrencyIsoCode) == currencyCode) {
                 s = loc.currencySymbol(QLocale::CurrencySymbol);
                 break;
             }
         }
         cache.insert(currencyCode, s.isEmpty() ? currencyCode : s);
    }
    return s;
}

/*! \qmlmethod string BrickStore::toCurrencyString(real value, string symbol = "", int precision = 3)

    Correctly formats the given currency \a value according to the user's locale and returns it as
    a string. Also appends a currency \a symbol if provided.
    The default number of decimal places is \c 3, but you can change this via the \a precision
    parameter.
*/
QString QmlBrickStore::toCurrencyString(double value, const QString &symbol, int precision) const
{
    return Currency::toDisplayString(value, symbol, precision);
}

/*! \qmlmethod string BrickStore::toWeightString(real value, bool showUnit = false)

    Correctly formats the given weight \a value according to the user's locale and the
    metric/imperial setting in BrickStore and returns it as a string. Also
    appends the corresponding unit if \a showUnit is set to \c true.
*/
QString QmlBrickStore::toWeightString(double value, bool showUnit) const
{
    return Utility::weightToString(value, Config::inst()->measurementSystem(), true, showUnit);
}


/*! \qmlmethod stringlist BrickStore::nameFiltersForBrickLinkXML(bool includeAll = false)

    Returns a list of file extension that can be used when creating a FileDialog to open
    BrickLink XML files. If \a includeAll is set, a match-everything filter \c * will be added to
    this list.
*/
QStringList QmlBrickStore::nameFiltersForBrickLinkXML(bool includeAll) const
{
    return DocumentIO::nameFiltersForBrickLinkXML(includeAll);
}

/*! \qmlmethod stringlist BrickStore::nameFiltersForBrickStoreXML(bool includeAll = false)

    Returns a list of file extension that can be used when creating a FileDialog to open
    BrickStore document files. If \a includeAll is set, a match-everything filter \c * will be
    added to this list.
*/
QStringList QmlBrickStore::nameFiltersForBrickStoreXML(bool includeAll) const
{
    return DocumentIO::nameFiltersForBrickStoreXML(includeAll);
}

/*! \qmlmethod stringlist BrickStore::nameFiltersForLDraw(bool includeAll = false)

    Returns a list of file extension that can be used when creating a FileDialog to open
    LDraw files. If \a includeAll is set, a match-everything filter \c * will be added to this
    list.
*/
QStringList QmlBrickStore::nameFiltersForLDraw(bool includeAll) const
{
    return DocumentIO::nameFiltersForLDraw(includeAll);
}

void QmlBrickStore::importBrickLinkStore(BrickLink::Store *store)
{
    DocumentIO::importBrickLinkStore(store);
}

void QmlBrickStore::importBrickLinkOrder(BrickLink::Order *order)
{
    DocumentIO::importBrickLinkOrder(order);
}

void QmlBrickStore::importBrickLinkCart(BrickLink::Cart *cart)
{
    DocumentIO::importBrickLinkCart(cart);
}

void QmlBrickStore::importPartInventory(BrickLink::QmlItem item, BrickLink::QmlColor color,
                                        int multiply, BrickLink::Condition condition,
                                        BrickLink::Status extraParts,
                                        bool includeInstructions, bool includeAlternates,
                                        bool includeCounterParts)
{
    Document::fromPartInventory(item.wrappedObject(), color.wrappedObject(),
                                multiply, condition, extraParts,
                                includeInstructions, includeAlternates, includeCounterParts);
}

/*! \qmlmethod void BrickStore::updateDatabase()

    Starts an asychronous database update in the background.
*/
void QmlBrickStore::updateDatabase()
{
    Application::inst()->updateDatabase();
}

QmlDocument *QmlBrickStore::activeDocument() const
{
    return m_docList->map(ActionManager::inst()->activeDocument());
}

QmlThenable *QmlBrickStore::checkBrickLinkLogin()
{
    auto *thenable = new QmlThenable(qmlEngine(this));

    Application::inst()->checkBrickLinkLogin().then([thenable](bool ok) {
        thenable->callThen({ ok });
    });
    return thenable;
}

double QmlBrickStore::maxLocalPrice(const QString &currencyCode)
{
    return DocumentModel::maxLocalPrice(currencyCode);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

QmlDocument::QmlDocument(Document *doc)
    : QAbstractProxyModel(doc)
    , m_doc(doc)
    , m_forceLayoutDelay(new QTimer(this))
    , m_columnModel(new QmlDocumentColumnModel(this))
{
    m_forceLayoutDelay->setInterval(100);
    m_forceLayoutDelay->setSingleShot(true);
    connect(m_forceLayoutDelay, &QTimer::timeout,
            this, &QmlDocument::forceLayout);

    setDocument(doc); //TODO: inline

    // relay signals for the QML API

    connect(doc, &Document::closeAllViewsForDocument,
            this, &QmlDocument::closeAllViewsForDocument);
    connect(doc, &Document::requestActivation,
            this, &QmlDocument::requestActivation);


    connect(model(), &DocumentModel::isSortedChanged,
            this, &QmlDocument::isSortedChanged);
    connect(model(), &DocumentModel::isFilteredChanged,
            this, &QmlDocument::isFilteredChanged);

    connect(model(), &DocumentModel::currencyCodeChanged,
            this, &QmlDocument::currencyCodeChanged);
    connect(model(), &DocumentModel::lotCountChanged,
            this, &QmlDocument::lotCountChanged);
    connect(model(), &DocumentModel::modificationChanged,
            this, &QmlDocument::modificationChanged);

    connect(doc, &Document::filePathChanged,
            this, &QmlDocument::filePathChanged);
    connect(doc, &Document::fileNameChanged,
            this, &QmlDocument::fileNameChanged);
    connect(doc, &Document::titleChanged,
            this, &QmlDocument::titleChanged);
    connect(doc, &Document::thumbnailChanged,
            this, &QmlDocument::thumbnailChanged);
    connect(doc, &Document::orderChanged,
            this, &QmlDocument::orderChanged);
    connect(doc, &Document::blockingOperationActiveChanged,
            this, &QmlDocument::blockingOperationActiveChanged);
    connect(doc, &Document::blockingOperationCancelableChanged,
            this, &QmlDocument::blockingOperationCancelableChanged);
    connect(doc, &Document::blockingOperationTitleChanged,
            this, &QmlDocument::blockingOperationTitleChanged);
    connect(doc, &Document::blockingOperationProgress,
            this, &QmlDocument::blockingOperationProgress);

    connect(model(), &DocumentModel::sortColumnsChanged,
            this, [this]() {
        emit qmlSortColumnsChanged();
    });
    connect(doc, &Document::selectedLotsChanged,
            this, &QmlDocument::qmlSelectedLotsChanged);
}

QVariantList QmlDocument::qmlSortColumns() const
{
    QVariantList result;
    const auto sortColumns = m_doc->model()->sortColumns();
    for (auto &sc : sortColumns)
        result.append(QVariantMap { { u"column"_qs, sc.first }, { u"order"_qs, sc.second } });
    return result;
}

QmlDocumentLots *QmlDocument::qmlLots()
{
    if (!m_qmlLots) {
        m_qmlLots = new QmlDocumentLots(m_doc->model());
        m_qmlLots->setParent(this);
    }
    return m_qmlLots;
}

QList<BrickLink::QmlLot> QmlDocument::qmlSelectedLots()
{
    const auto lots = m_doc->selectedLots();
    QList<BrickLink::QmlLot> qmlLots;
    qmlLots.reserve(lots.size());

    for (const auto *lot : lots)
        qmlLots << BrickLink::QmlLot(lot);

    return qmlLots;
}

void QmlDocument::sort(int column, Qt::SortOrder order)
{
    model()->sort({ { column, order } });
}

void QmlDocument::sortAdditionally(int column, Qt::SortOrder order)
{
    auto sc = model()->sortColumns();
    sc.append({ column, order });
    model()->sort(sc);
}

DocumentStatistics QmlDocument::selectionStatistics(bool ignoreExcluded) const
{
    const auto &selected = m_doc->selectedLots();

    return m_doc->model()->statistics(selected.isEmpty() ? m_doc->model()->filteredLots() : selected,
                                      ignoreExcluded);
}

void QmlDocument::saveCurrentColumnLayout()
{
    // decouple the co-routine
    QMetaObject::invokeMethod(this, [this]() { m_doc->saveCurrentColumnLayout(); });
}

void QmlDocument::setColumnLayoutFromId(const QString &layoutId)
{
    m_doc->setColumnLayoutFromId(layoutId);
}

void QmlDocument::cancelBlockingOperation()
{
    m_doc->cancelBlockingOperation();
}

void QmlDocument::setPriceToGuide(BrickLink::Time time, BrickLink::Price price, bool forceUpdate)
{
    m_doc->setPriceToGuide(time, price, forceUpdate);
}

void QmlDocument::setColor(BrickLink::QmlColor color)
{
    m_doc->setColor(color.wrappedObject());
}

void QmlDocument::priceAdjust(bool isFixed, double value, bool applyToTiers)
{
    m_doc->priceAdjust(isFixed, value, applyToTiers);
}

void QmlDocument::costAdjust(bool isFixed, double value)
{
    m_doc->costAdjust(isFixed, value);
}


void QmlDocument::setDocument(Document *doc)
{
    beginResetModel();

    delete m_connectionContext;
    m_connectionContext = nullptr;

    m_doc = doc;
    setSourceModel(doc ? doc->model() : nullptr);

    if (m_doc) {
        m_connectionContext = new QObject(this);

        connect(m_doc, &Document::columnPositionChanged,
                m_connectionContext, [this](int /*li*/, int /*vi*/) {
            m_columnModel->beginResetModel();
            beginResetModel();
            update();
            endResetModel();
            m_columnModel->endResetModel();
            emitForceLayout(); //TODO: check if needed
        });
        connect(m_doc, &Document::columnSizeChanged,
                m_connectionContext, [this](int li, int /*size*/) {
            emit headerDataChanged(Qt::Horizontal, l2v[li], l2v[li]);
            emitForceLayout();
        });
        connect(m_doc, &Document::columnHiddenChanged,
                m_connectionContext, [this](int li, bool /*hidden*/) {
            emit headerDataChanged(Qt::Horizontal, l2v[li], l2v[li]);
            emitForceLayout();
        });
        connect(m_doc, &Document::columnLayoutChanged,
                m_connectionContext, [this]() {
            emitForceLayout();
        });

        connect(m_doc->model(), &QAbstractItemModel::dataChanged,
                m_connectionContext, [this](const QModelIndex &from, const QModelIndex &to, const QVector<int> &roles) {
            if (from.column() == to.column()) {
                emit dataChanged(mapFromSource(from), mapFromSource(to), roles);
            } else {
                emit dataChanged(index(from.row(), 0), index(to.row(), columnCount() - 1), roles);
            }
        });
        connect(m_doc->model(), &QAbstractItemModel::headerDataChanged,
                m_connectionContext, [this](Qt::Orientation o, int section, int role) {
            emit headerDataChanged(o, section < 0 ? -1 : l2v[section], role);
        });
        connect(m_doc->model(), &QAbstractItemModel::modelAboutToBeReset,
                m_connectionContext, [this]() {
            beginResetModel();
        });
        connect(m_doc->model(), &QAbstractItemModel::modelReset,
                m_connectionContext, [this]() {
            endResetModel();
        });
        connect(m_doc->model(), &QAbstractItemModel::layoutAboutToBeChanged,
                m_connectionContext, [this](const QList<QPersistentModelIndex> &, LayoutChangeHint hint) {
            emit layoutAboutToBeChanged({ }, hint);
        });
        connect(m_doc->model(), &QAbstractItemModel::layoutChanged,
                m_connectionContext, [this](const QList<QPersistentModelIndex> &, LayoutChangeHint hint) {
            emit layoutChanged({ }, hint);
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsAboutToBeInserted,
                m_connectionContext, [this](const QModelIndex &, int start, int end) {
            beginInsertRows({ }, start, end);
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsInserted,
                m_connectionContext, [this](const QModelIndex &, int, int) {
            endInsertRows();
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsAboutToBeRemoved,
                m_connectionContext, [this](const QModelIndex &, int start, int end) {
            beginRemoveRows({ }, start, end);
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsRemoved,
                m_connectionContext, [this](const QModelIndex &, int, int) {
            endRemoveRows();
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsAboutToBeMoved,
                m_connectionContext, [this](const QModelIndex &, int sstart, int send, const QModelIndex &, int dstart) {
            beginMoveRows({ }, sstart, send, { }, dstart);
        });
        connect(m_doc->model(), &QAbstractItemModel::rowsMoved,
                m_connectionContext, [this](const QModelIndex &, int, int, const QModelIndex &, int) {
            endMoveRows();
        });
        connect(this, &QmlDocument::headerDataChanged,
                this, [this](Qt::Orientation orientation, int, int) {
            if (orientation != Qt::Horizontal)
                return;
            auto tl = m_columnModel->index(0);
            auto br = m_columnModel->index(m_columnModel->rowCount() - 1);
            emit m_columnModel->dataChanged(tl, br);
        });
    }
    update();
    endResetModel();
}

Document *QmlDocument::document() const
{
    return m_doc;
}


int QmlDocument::rowCount(const QModelIndex &parent) const
{
    return sourceModel()->rowCount(mapToSource(parent));
}

int QmlDocument::columnCount(const QModelIndex &parent) const
{
    return sourceModel()->columnCount(mapToSource(parent));
}

QModelIndex QmlDocument::index(int row, int column, const QModelIndex &) const
{
    auto sindex = sourceModel()->index(row, column < 0 ? -1 : v2l[column], { });
    return mapFromSource(sindex);
}

QModelIndex QmlDocument::parent(const QModelIndex &) const
{
    return QModelIndex();
}

QVariant QmlDocument::headerData(int section, Qt::Orientation o, int role) const
{
    if ((o != Qt::Horizontal) || (section < 0) || (section >= m_doc->model()->columnCount()))
        return { };

    switch (role) {
    case Qt::SizeHintRole: {
        const auto &cd = m_doc->columnLayout().value(v2l[section]);
        return cd.m_size;
    }
    case Qt::CheckStateRole: {
        const auto &cd = m_doc->columnLayout().value(v2l[section]);
        return cd.m_hidden;
    }
    default:
        return sourceModel()->headerData(v2l[section], o, role);
    }
}

QModelIndex QmlDocument::mapToSource(const QModelIndex &index) const
{
    auto sindex = createSourceIndex(index.row(), index.column() < 0 ? -1 : v2l[index.column()],
                                    index.internalPointer());
    return sindex;
}

QModelIndex QmlDocument::mapFromSource(const QModelIndex &sindex) const
{
    auto index = createIndex(sindex.row(), sindex.column() < 0 ? -1 : l2v[sindex.column()],
                             sindex.internalPointer());
    return index;
}

int QmlDocument::logicalColumn(int visual) const
{
    return ((visual >= 0) && (visual < v2l.size())) ? v2l[visual] : -1;
}

int QmlDocument::visualColumn(int logical) const
{
    return ((logical >= 0) && (logical < l2v.size())) ? l2v[logical] : -1;
}

QmlDocumentColumnModel *QmlDocument::columnModel()
{
    return m_columnModel;
}

void QmlDocument::update()
{
    if (!m_doc)
        return;

    const auto layout = m_doc->columnLayout();
    auto count = layout.count();
    v2l.resize(count);
    l2v.resize(count);

    for (int li = 0; li < layout.size(); ++li) {
        l2v[li] = layout[li].m_visualIndex;
        v2l[l2v[li]] = li;
    }
}

void QmlDocument::emitForceLayout()
{
    m_forceLayoutDelay->start();
}

void QmlDocument::internalMoveColumn(int viFrom, int viTo)
{
    int li = v2l[viFrom];
    m_doc->moveColumn(li, viFrom, viTo);
}

bool QmlDocument::internalIsColumnHidden(int li) const
{
    const auto &cd = m_doc->columnLayout().value(li);
    return cd.m_hidden;
}

void QmlDocument::internalHideColumn(int vi, bool hidden)
{
    int li = v2l[vi];
    const auto &cd = m_doc->columnLayout().value(li);

    if (cd.m_hidden != hidden)
        m_doc->hideColumn(li, cd.m_hidden, hidden);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlDocumentColumnModel::QmlDocumentColumnModel(QmlDocument *proxyModel)
    : QAbstractListModel(proxyModel)
    , m_proxyModel(proxyModel)
{ }

int QmlDocumentColumnModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_proxyModel->columnCount();
}

QVariant QmlDocumentColumnModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return { };
    return m_proxyModel->headerData(index.row(), Qt::Horizontal, role);
}

QHash<int, QByteArray> QmlDocumentColumnModel::roleNames() const
{
    static QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "name" },
        { Qt::CheckStateRole, "hidden" },
    };
    return roles;
}

void QmlDocumentColumnModel::moveColumn(int viFrom, int viTo)
{
    m_proxyModel->internalMoveColumn(viFrom, viTo);
}

void QmlDocumentColumnModel::hideColumn(int vi, bool hidden)
{
    m_proxyModel->internalHideColumn(vi, hidden);
}

bool QmlDocumentColumnModel::isColumnHidden(int li) const
{
    return m_proxyModel->internalIsColumnHidden(li);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ColumnLayoutsModel::ColumnLayoutsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(Config::inst(), &Config::columnLayoutIdsChanged,
            this, &ColumnLayoutsModel::update);
    connect(Config::inst(), &Config::columnLayoutIdsOrderChanged,
            this, &ColumnLayoutsModel::update);
    connect(Config::inst(), &Config::columnLayoutNameChanged,
            this, &ColumnLayoutsModel::update);
    update();
}

int ColumnLayoutsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_idAndName.size());
}

QVariant ColumnLayoutsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return { };

    switch (role) {
    case Qt::DisplayRole:
        return m_idAndName.at(index.row()).second;
    case Qt::UserRole:
        return m_idAndName.at(index.row()).first;
    default:
        return { };
    }
}

QHash<int, QByteArray> ColumnLayoutsModel::roleNames() const
{
    return { { Qt::DisplayRole, "name" }, { Qt::UserRole, "id" } };
}

void ColumnLayoutsModel::update()
{
    beginResetModel();
    m_idAndName.clear();

    const auto specialCommands = Document::columnLayoutCommands();
    for (auto cmd : specialCommands) {
        auto id = Document::columnLayoutCommandId(cmd);
        auto name = Document::columnLayoutCommandName(cmd);
        m_idAndName.append({ id, name });
    }
    const auto userLayoutIds = Config::inst()->columnLayoutIds();
    if (!userLayoutIds.isEmpty())
        m_idAndName.append({ u""_qs, u"-"_qs });

    QMap<int, QString> orderedUserLayoutIds;
    for (const auto &layoutId : userLayoutIds)
        orderedUserLayoutIds.insert(Config::inst()->columnLayoutOrder(layoutId), layoutId);

    for (const auto &layoutId : qAsConst(orderedUserLayoutIds)) {
        auto name = Config::inst()->columnLayoutName(layoutId);
        m_idAndName.append({ layoutId, name });
    }
    endResetModel();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlSortFilterProxyModel::QmlSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QSortFilterProxyModel::rowsInserted, this, [this]() {
        emit countChanged(count());
    });
    connect(this, &QSortFilterProxyModel::rowsRemoved, this,  [this]() {
        emit countChanged(count());
    });
    connect(this, &QSortFilterProxyModel::filterRoleChanged, this, [this](int role) {
        emit filterRoleNameChanged(QString::fromLatin1(roleNames().value(role)));
    });
    connect(this, &QSortFilterProxyModel::sortRoleChanged, this, [this](int role) {
        emit sortRoleNameChanged(QString::fromLatin1(roleNames().value(role)));
    });
}

int QmlSortFilterProxyModel::count() const
{
    return rowCount();
}

QString QmlSortFilterProxyModel::sortRoleName() const
{
    return QString::fromLatin1(roleNames().value(sortRole()));
}

void QmlSortFilterProxyModel::setSortRoleName(const QString &role)
{
    setSortRole(roleKey(role.toLatin1()));
    invalidate();
}

QString QmlSortFilterProxyModel::filterRoleName() const
{
    return QString::fromLatin1(roleNames().value(filterRole()));
}

void QmlSortFilterProxyModel::setFilterRoleName(const QString &role)
{
    setFilterRole(roleKey(role.toLatin1()));
}

void QmlSortFilterProxyModel::setSortColumn(int newSortColumn)
{
    if (sortColumn() != newSortColumn) {
        sort(newSortColumn, sortOrder());
        emit sortColumnChanged(newSortColumn);
    }
}

void QmlSortFilterProxyModel::setSortOrder(Qt::SortOrder newSortOrder)
{
    if (sortOrder() != newSortOrder) {
        sort(sortColumn(), newSortOrder);
        emit sortOrderChanged(newSortOrder);
    }
}

QString QmlSortFilterProxyModel::filterString() const
{
    return filterRegularExpression().pattern();
}

void QmlSortFilterProxyModel::setFilterString(const QString &filter)
{
    switch (m_filterSyntax) {
    case RegularExpression:
        setFilterRegularExpression(filter);
        break;
    case Wildcard:
        setFilterRegularExpression(QRegularExpression::fromWildcard(filter));
        break;
    case FixedString:
        setFilterFixedString(filter);
        break;
    }
}

QmlSortFilterProxyModel::FilterSyntax QmlSortFilterProxyModel::filterSyntax() const
{
    return m_filterSyntax;
}

void QmlSortFilterProxyModel::setFilterSyntax(FilterSyntax syntax)
{
    if (m_filterSyntax != syntax) {
        m_filterSyntax = syntax;
        emit filterSyntaxChanged(syntax);
        //setFilterString({ });
    }
}

QHash<int, QByteArray> QmlSortFilterProxyModel::roleNames() const
{
    if (auto source = sourceModel())
        return source->roleNames();
    return { };
}

int QmlSortFilterProxyModel::roleKey(const QByteArray &role) const
{
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        if (it.value() == role)
            return it.key();
    }
    return -1;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


void QmlClipboard::clear()
{
    QGuiApplication::clipboard()->clear(QClipboard::Clipboard);
}

QString QmlClipboard::text() const
{
    return QGuiApplication::clipboard()->text(QClipboard::Clipboard);
}

void QmlClipboard::setText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlThenable::QmlThenable(QJSEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

QmlThenable::~QmlThenable()
{ }

void QmlThenable::then(const QJSValue &function)
{
    m_thenable = true;
    m_function = function;
    if (!m_function.isCallable())
        qWarning() << "The argument for QmlThenable::then() is not callable";
}

void QmlThenable::callThen(const QVariantList &arguments)
{
    if (!m_thenable) { // not returned to JS yet?
        QMetaObject::invokeMethod(this, [this, arguments]() {
            if (!m_thenable) // nobody called then() on the JS side
                deleteLater();
            else
                callThenInternal(arguments);
        }, Qt::QueuedConnection);
    } else {
        callThenInternal(arguments);
    }
}

void QmlThenable::callThenInternal(const QVariantList &arguments)
{
    if (m_function.isCallable()) {
        QJSValueList vl;
        for (const auto &arg : arguments)
            vl << m_engine->toScriptValue(arg);
        auto result = m_function.call(vl);
        if (result.isError()) {
            qWarning() << "QmlThenable::then() failed:" << result.toString();
        }
    }
    QQmlEngine::setObjectOwnership(this, QQmlEngine::JavaScriptOwnership);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


namespace {
enum {
    TypeRole = Qt::UserRole + 1,
    CategoryRole,
    MessageRole,
    LineRole,
    FileRole,
};
}

QmlDebugLogModel *QmlDebugLogModel::s_inst = nullptr;

QmlDebugLogModel::QmlDebugLogModel(QObject *parent)
    : QAbstractListModel(parent)
{ }

QmlDebugLogModel *QmlDebugLogModel::inst()
{
    if (!s_inst)
        s_inst = new QmlDebugLogModel(qApp);
    return s_inst;
}

int QmlDebugLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_logs.size());
}

QVariant QmlDebugLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return { };

    const Log &log = m_logs.at(size_t(index.row()));

    switch (role) {
    case TypeRole    : return int(log.type);
    case LineRole    : return log.line;
    case FileRole    : return log.file;
    case CategoryRole: return log.category;
    case MessageRole : return log.message;
    }
    return { };

}

QHash<int, QByteArray> QmlDebugLogModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { TypeRole, "type" },
        { CategoryRole, "category" },
        { MessageRole, "message" },
        { LineRole, "line" },
        { FileRole, "file" },
    };
    return roles;
}

void QmlDebugLogModel::append(QtMsgType type, const QString &category, const QString &file,
                              int line, const QString &message)
{
    beginInsertRows({ }, int(m_logs.size()), int(m_logs.size()));
    m_logs.push_back({ type, line, category, file, message });
    endInsertRows();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlDebug::QmlDebug(QObject *parent)
    : QObject(parent)
{
    m_showTracers = (qEnvironmentVariableIntValue("BS_SHOW_TRACERS") == 1);
}

bool QmlDebug::showTracers() const
{
    return m_showTracers;
}

void QmlDebug::setShowTracers(bool newShowTracers)
{
    if (m_showTracers != newShowTracers) {
        m_showTracers = newShowTracers;
        emit showTracersChanged(newShowTracers);
    }
}

QAbstractListModel *QmlDebug::log() const
{
    return QmlDebugLogModel::inst();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlDocumentLots::QmlDocumentLots(DocumentModel *model)
    : QObject(model)
    , m_model(model)
{
    static bool once = false;
    if (!once) {
        BrickLink::QmlLot::setQmlSetterCallback([](QmlDocumentLots *lots, BrickLink::Lot *which,
                                                const BrickLink::Lot &value) {
            if (lots && lots->m_model)
                lots->m_model->changeLot(which, value);
        });
        once = true;
    }
}

int QmlDocumentLots::add(BrickLink::QmlItem item, BrickLink::QmlColor color)
{
    auto lot = new Lot();
    lot->setItem(item.wrappedObject());
    lot->setColor(color.wrappedObject());
    m_model->appendLot(std::move(lot));
    return int(m_model->lots().indexOf(lot));
}

void QmlDocumentLots::remove(BrickLink::QmlLot lot)
{
    if (!lot.isNull() && m_model && (lot.m_documentLots == this))
        m_model->removeLot(lot.wrappedObject());
}

void QmlDocumentLots::removeAt(int index)
{
    if ((index >= 0) && (index < m_model->lotCount())) {
        Lot *lot = m_model->lots().at(index);
        m_model->removeLot(lot);
    }
}

BrickLink::QmlLot QmlDocumentLots::at(int index)
{
    if (index < 0 || index >= m_model->lotCount())
        return BrickLink::QmlLot { };
    return BrickLink::QmlLot(m_model->lots().at(index), this);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


QmlDocumentList::QmlDocumentList(QObject *parent)
    : QIdentityProxyModel(parent)
{
    auto dl = DocumentList::inst();

    connect(dl, &DocumentList::lastDocumentClosed,
            this, &QmlDocumentList::lastDocumentClosed);
    connect(dl, &DocumentList::countChanged,
            this, &QmlDocumentList::countChanged);
    connect(dl, &DocumentList::documentCreated,
            this, [this](Document *doc) {
        auto qmlDoc = m_docMapping.value(doc);
        if (!qmlDoc)
            qmlDoc = new QmlDocument(doc);
        m_docMapping.insert(doc, qmlDoc);
    });
    connect(dl, &DocumentList::documentAdded,
            this, [this](Document *doc) {
        if (auto qmlDoc = m_docMapping.value(doc))
            emit documentAdded(qmlDoc);
    });
    connect(dl, &DocumentList::documentRemoved,
            this, [this](Document *doc) {
        if (auto qmlDoc = m_docMapping.take(doc)) {
            emit documentRemoved(qmlDoc);
            qmlDoc->deleteLater();
        }
    });
    setSourceModel(dl);
}

QmlDocument *QmlDocumentList::map(Document *doc) const
{
    return m_docMapping.value(doc);
}

QVariant QmlDocumentList::data(const QModelIndex &index, int role) const
{
    QVariant v = QIdentityProxyModel::data(index, role);
    if (index.isValid() && (role == Qt::UserRole))
        return QVariant::fromValue(map(v.value<Document *>()));
    return v;
}

QHash<int, QByteArray> QmlDocumentList::roleNames() const
{
    return {
        { Qt::DisplayRole, "fileNameOrTitle" },
        { Qt::UserRole, "document" },
    };
}

QmlDocument *QmlDocumentList::document(int index) const
{
    if (index < 0 || index >= rowCount())
        return nullptr;

    return map(QIdentityProxyModel::data(this->index(index, 0), Qt::UserRole).value<Document *>());
}

DocumentList *QmlDocumentList::docList()
{
    return DocumentList::inst();
}

#include "moc_brickstore_wrapper.cpp"
