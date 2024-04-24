// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only


#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlInfo>
#include <QFile>
#include <QUrl>
#include <QGuiApplication>
#include <QPalette>
#include <QWindow>
#include <QtCore/private/qabstractanimation_p.h>

#include "utility/utility.h"
#include "utility/exception.h"
#include "common/currency.h"
#include "bricklink/order.h"
#include "bricklink/dimensions.h"
#include "ldraw/library.h"
#include "common/actionmanager.h"
#include "common/application.h"
#include "common/document.h"
#include "common/documentmodel.h"
#include "common/documentio.h"
#include "common/recentfiles.h"
#include "brickstore_wrapper.h"
#include "brickstore_wrapper_p.h"
#include "version.h"

using namespace std::chrono_literals;

/*! \qmltype BrickStore
    \inherits QtObject
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This singleton represents global settings and state.

    A kitchen sink type singleton for all global state, settings and utility functions.
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
    connect(Application::inst(), &Application::mainWindowChanged,
            this, &QmlBrickStore::mainWindowChanged);

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

/*! \qmlproperty string BrickStore::versionNumber
    \readonly
    BrickStore's version as a string (e.g. \c "2021.10.2").
*/
QString QmlBrickStore::versionNumber() const
{
    return u"" BRICKSTORE_VERSION ""_qs;
}

/*! \qmlproperty string BrickStore::buildNumber
    \readonly
    BrickStore's build number as a string (e.g. \c "42").
*/
QString QmlBrickStore::buildNumber() const
{
    return u"" BRICKSTORE_BUILD_NUMBER ""_qs;
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

/*! \qmlproperty string BrickStore::defaultCurrencyCode
    \readonly
    The user's default ISO currency code (e.g. \c EUR).
*/
QString QmlBrickStore::defaultCurrencyCode() const
{
    return Config::inst()->defaultCurrencyCode();
}

/*! \qmlproperty Window BrickStore::mainWindow
    \readonly
    A reference to the main \l Window of BrickStore. Can be used to open dialogs and other windows
    on top of the main window by setting \l{Window::}{transientParent}.
*/
QWindow *QmlBrickStore::mainWindow() const
{
    return Application::inst()->mainWindow();
}

/*! \qmlmethod string BrickStore::exchangeRate(string fromCode, string toCode)

    Returns the currency exchange cross-rate between the two given \a fromCode and \a toCode
    currencies, denoted as ISO currency code (e.g. \c EUR).
*/
double QmlBrickStore::exchangeRate(const QString &fromCode, const QString &toCode) const
{
    return Currency::inst()->crossRate(fromCode, toCode);
}

QString QmlBrickStore::dim(const QString &str) const
{
    auto d = BrickLink::Dimensions::parseString(str, 0, BrickLink::Dimensions::Relaxed);
    QString result = u"X: " + QString::number(d.x())
                     + u" | Y: " + QString::number(d.y())
                     + u" | Z: " + QString::number(d.z())
                     + u" | @: " + QString::number(d.offset()) + u", " + QString::number(d.length());
    return result;
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
                                        BrickLink::PartOutTraits partOutTraits,
                                        BrickLink::Status status)
{
    Document::fromPartInventory(item.wrappedObject(), color.wrappedObject(),
                                multiply, condition, extraParts, partOutTraits, status);
}

/*! \qmlmethod void BrickStore::updateDatabase()

    Starts an asynchronous database update in the background.
*/
void QmlBrickStore::updateDatabase()
{
    Application::inst()->updateDatabase();
}

/*! \qmlproperty Document BrickStore::activeDocument
    \readonly
    The currently active document, i.e. the document the user is working on. Can be \c null, if
    no documents are open, but also if the quick-start page is active.
*/
QmlDocument *QmlBrickStore::activeDocument() const
{
    return m_docList->map(ActionManager::inst()->activeDocument());
}

QCoro::QmlTask QmlBrickStore::checkBrickLinkLogin()
{
    return Application::inst()->checkBrickLinkLogin();
}

double QmlBrickStore::maxLocalPrice(const QString &currencyCode)
{
    return DocumentModel::maxLocalPrice(currencyCode);
}

QString QmlBrickStore::cacheStats() const
{
    auto pic = BrickLink::core()->pictureCache()->cacheStats();
    auto pg = BrickLink::core()->priceGuideCache()->cacheStats();
    auto ld = LDraw::library()->partCacheStats();

    QString picBar(int(double(pic.first) / pic.second * 16), u'=');
    picBar += QString(16 - picBar.length(), u' ');
    QString pgBar(int(double(pg.first) / pg.second * 16), u'=');
    pgBar += QString(16 - pgBar.length(), u' ');
    QString ldBar(int(double(ld.first) / ld.second * 16), u'=');
    ldBar += QString(16 - ldBar.length(), u' ');

    return u"Cache stats:\n"_qs
                  + u"Pictures    : [" + picBar + u"] " + QString::number(pic.first / 1000)
                  + u" / " + QString::number(pic.second / 1000) + u" MB\n"
                  + u"Price guides: [" + pgBar + u"] " + QString::number(pg.first)
                  + u" / " + QString::number(pg.second) + u" entries\n"
                  + u"LDraw parts : [" + ldBar + u"] " + QString::number(ld.first)
                  + u" / " + QString::number(ld.second) + u" lines";
}

void QmlBrickStore::crash(bool useException) const
{
    if (useException)
        throw Exception("Test exception");
    else
        static_cast<int *>(nullptr)[0] = 1; // NOLINT
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/*! \qmltype Document
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief Each instance of this type represents an open document.

    \note The \e index parameter of the lot API is the index into the internal data structure and
          not related to any sorting or filtering in the visible document window.
*/
/*! \qmlproperty string Document::currencyCode
    \readonly
    The ISO currency code used in this document, e.g. \c EUR.
*/
/*! \qmlproperty string Document::title
    \readonly
    The title of this document. Documents loaded from files do not have titles, but documents
    created from online imports do. A store import would have a title like \c{Store import <date>}.
*/
/*! \qmlproperty string Document::fileName
    \readonly
    The file name of this document. New and imported documents that have not been saved yet will
    not have fileName set.
*/
/*! \qmlproperty Order Document::order
    \readonly
    An Order object or \c null, if this document is not a BrickLink order.
*/

QmlDocument::QmlDocument(Document *doc)
    : QAbstractProxyModel(doc)
    , m_doc(doc)
    , m_columnModel(new QmlDocumentColumnModel(this))
    , m_proxySelectionModel(new ProxySelectionModel(this, doc))
{
    setDocument(doc);

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

    connect(model(), &DocumentModel::lotCountChanged,
            this, &QmlDocument::lotCountChanged);
    connect(model(), &DocumentModel::filteredLotCountChanged,
            this, &QmlDocument::visibleLotCountChanged);

    connect(model(), &DocumentModel::filterChanged,
            this, &QmlDocument::filterStringChanged);
}

QVariantList QmlDocument::qmlSortColumns() const
{
    const auto sortColumns = m_doc->model()->sortColumns();
    QVariantList result;
    result.reserve(sortColumns.size());
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

QString QmlDocument::filterString() const
{
    return const_cast<DocumentModel *>(model())->filterParser()->toString(model()->filter(), true /*symbolic*/);
}

void QmlDocument::setFilterString(const QString &newFilterString)
{
    model()->setFilter(model()->filterParser()->parse(newFilterString));
}

/*! \qmlproperty string Document::lotCount
    \readonly
    The number of lots in this document.
    This count is ignoring filtering.
*/
int QmlDocument::lotCount() const
{
    return int(model()->lots().size());
}

/*! \qmlproperty string Document::visibleLotCount
    \readonly
    The number of visible lots in this document.
    This count is after filtering: it's the the exact number of visible lots as seen in the
    document view.
*/
int QmlDocument::visibleLotCount() const
{
    return int(model()->filteredLots().size());
}

void QmlDocument::sort(int column, Qt::SortOrder order)
{
    model()->multiSort({ { column, order } });
}

void QmlDocument::sortAdditionally(int column, Qt::SortOrder order)
{
    auto sc = model()->sortColumns();
    sc.append({ column, order });
    model()->multiSort(sc);
}

DocumentStatistics QmlDocument::statistics(bool ignoreExcluded) const
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

void QmlDocument::startBlockingOperation(const QString &title, const QJSValue &cancelCallback)
{
    if (!cancelCallback.isCallable())
        return;
    m_doc->startBlockingOperation(title, [=]() { cancelCallback.call(); });
}

void QmlDocument::endBlockingOperation()
{
    m_doc->endBlockingOperation();
}

void QmlDocument::cancelBlockingOperation()
{
    m_doc->cancelBlockingOperation();
}

void QmlDocument::setPriceToGuide(BrickLink::Time time, BrickLink::Price price, bool forceUpdate)
{
    m_doc->setPriceToGuide(time, price, forceUpdate, BrickLink::NoPriceGuideOption::Marker);
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
        });
        connect(m_doc, &Document::columnSizeChanged,
                m_connectionContext, [this](int li, int /*size*/) {
            emit headerDataChanged(Qt::Horizontal, l2v[li], l2v[li]);
            emit columnLayoutChanged();
        });
        connect(m_doc, &Document::columnHiddenChanged,
                m_connectionContext, [this](int li, bool /*hidden*/) {
            emit headerDataChanged(Qt::Horizontal, l2v[li], l2v[li]);
            emit columnLayoutChanged();
        });
        connect(m_doc, &Document::columnLayoutChanged,
                m_connectionContext, [this]() {
            emit columnLayoutChanged();
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
    return { };
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

QItemSelectionModel *QmlDocument::selectionModel()
{
    return m_proxySelectionModel;
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

void QmlDocument::internalMoveColumn(int viFrom, int viTo)
{
    int li = v2l.at(viFrom);
    m_doc->moveColumn(li, viFrom, viTo);
}

bool QmlDocument::internalIsColumnHidden(int li) const
{
    const auto &cd = m_doc->columnLayout().value(li);
    return cd.m_hidden;
}

void QmlDocument::internalHideColumn(int vi, bool hidden)
{
    int li = v2l.at(vi);
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

    for (const auto &layoutId : std::as_const(orderedUserLayoutIds)) {
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

bool QmlDebug::slowAnimations() const
{
    return m_slowAnimations;
}

void QmlDebug::setSlowAnimations(bool newSlowAnimations)
{
    if (m_slowAnimations != newSlowAnimations) {
        m_slowAnimations = newSlowAnimations;
        QUnifiedTimer::instance()->setSlowModeEnabled(m_slowAnimations);
        emit slowAnimationsChanged(m_slowAnimations);
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

/*! \qmlmethod int Document::lots.add(Item item, Color color)
    Adds a new lot for the given \a item and \a color combination to the document.
    Returns the index of the newly created lot in the document.
    This index is independent of sort order and filtering.
*/
int QmlDocumentLots::add(BrickLink::QmlItem item, BrickLink::QmlColor color)
{
    auto lot = new Lot();
    lot->setItem(item.wrappedObject());
    lot->setColor(color.wrappedObject());
    m_model->appendLot(std::move(lot));
    return int(m_model->lots().indexOf(lot));
}

/*! \qmlmethod Document::lots.remove(Lot lot)
    Removes the specified \a lot from the document.
*/
void QmlDocumentLots::remove(BrickLink::QmlLot lot)
{
    if (!lot.isNull() && m_model && (lot.m_documentLots == this))
        m_model->removeLot(lot.wrappedObject());
}

/*! \qmlmethod Document::lots.removeAt(int index)
    Removes the lot at the specified \a index from the document.
    This index is independent of sort order and filtering.
*/
void QmlDocumentLots::removeAt(int index)
{
    const auto &lots = m_model->lots();
    if ((index >= 0) && (index < int(lots.size())))
        m_model->removeLot(lots.at(index));
}

/*! \qmlmethod Document::lots.removeVisibleAt(int index)
    Removes the lot at the specified visible \a index from the document.
    This index is taking sort order and filtering into account: it's the the exact visible index
    as seen in the document view.
*/
void QmlDocumentLots::removeVisibleAt(int index)
{
    const auto &filteredLots = m_model->filteredLots();
    if ((index >= 0) && (index < int(filteredLots.size())))
        m_model->removeLot(filteredLots.at(index));
}

/*! \qmlmethod Lot Document::lots.at(int index)
    Returns the lot at the specified \a index in the document.
    This index is independent of sort order and filtering.
*/
BrickLink::QmlLot QmlDocumentLots::at(int index)
{
    const auto &lots = m_model->lots();
    if (index < 0 || index >= int(lots.size()))
        return { };
    return { lots.at(index), this };
}

/*! \qmlmethod Lot Document::lots.visibleAt(int index)
    Returns the lot at the specified visible \a index in the document.
    This index is taking sort order and filtering into account: it's the the exact visible index
    as seen in the document view.
*/
BrickLink::QmlLot QmlDocumentLots::visibleAt(int index)
{
    const auto &filteredLots = m_model->filteredLots();
    if (index < 0 || index >= int(filteredLots.size()))
        return { };
    return { filteredLots.at(index), this };
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


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


/*! \qmltype OnlineState
    \inherits QtObject
    \inqmlmodule BrickStore
    \ingroup qml-api
    \brief This singleton represents the network state.

    The current online state of the application. This is mirroring the operating system's online
    state.
*/

/*! \qmlproperty bool OnlineState::online

    This property holds whether the system is currently online.
*/


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


ProxySelectionModel::ProxySelectionModel(QmlDocument *qmlDoc, Document *doc)
    : QItemSelectionModel(qmlDoc, qmlDoc)
    , m_qmlDoc(qmlDoc)
    , m_doc(doc)
{
    Q_ASSERT(qmlDoc);
    Q_ASSERT(doc);

    connect(m_doc->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) {
        transfer(m_doc->selectionModel(), this);
    });
    connect(this, &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) {
        transfer(this, m_doc->selectionModel());
    });
}

void ProxySelectionModel::transfer(const QItemSelectionModel *from, QItemSelectionModel *to)
{
    if (m_interlock)
        return;
    m_interlock = true;

    const QItemSelection fromSel = from->selection();
    QItemSelection toSel;
    toSel.reserve(fromSel.size());
    for (const auto &selRange : fromSel) {
        QModelIndex tl = (to == this) ? m_qmlDoc->mapFromSource(selRange.topLeft())
                                      : m_qmlDoc->mapToSource(selRange.topLeft());
        QModelIndex br = (to == this) ? m_qmlDoc->mapFromSource(selRange.bottomRight())
                                      : m_qmlDoc->mapToSource(selRange.bottomRight());
        toSel << QItemSelectionRange(tl, br);
    }

    to->select(toSel, QItemSelectionModel::ClearAndSelect);
    m_interlock = false;
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

// this is a copy of Qt's QQmlSettings, but adjusted to our needs:
// we cannot set the global organization name for backwards compatibility

QmlExtraConfig::QmlExtraConfig(QObject *parent)
    : QObject(parent)
{ }

QmlExtraConfig::~QmlExtraConfig()
{
    reset(); // flush pending changes
}

QString QmlExtraConfig::category() const
{
    return m_category;
}

void QmlExtraConfig::setCategory(const QString &newCategory)
{
    if (m_category != newCategory) {
        reset();
        m_category = newCategory;
        if (m_initialized)
            load();
        emit categoryChanged(newCategory);
    }
}

void QmlExtraConfig::timerEvent(QTimerEvent *event)
{
    QObject::timerEvent(event);
    if (event->timerId() != m_timerId)
        return;
    killTimer(m_timerId);
    m_timerId = 0;
    store();
}

void QmlExtraConfig::classBegin()
{ }

void QmlExtraConfig::componentComplete()
{
    init();
}

QSettings *QmlExtraConfig::instance()
{
    if (m_settings)
        return m_settings;

    m_settings = new QSettings(Config::inst()->organizationName(), Config::inst()->applicationName(), this);

    if (m_settings->status() != QSettings::NoError) {
        qmlWarning(this) << "Failed to initialize QSettings instance. Status code is: " << int(m_settings->status());
        return m_settings;
    }

    if (!m_category.isEmpty())
        m_settings->beginGroup(m_category);

    if (m_initialized)
        load();

    return m_settings;
}

void QmlExtraConfig::init()
{
    if (m_initialized)
        return;
    load();
    m_initialized = true;
}

void QmlExtraConfig::reset()
{
    if (m_initialized && m_settings && !m_changedProperties.isEmpty())
        store();
    delete m_settings;
}

void QmlExtraConfig::load()
{
    const QMetaObject *mo = metaObject();
    const int offset = mo->propertyOffset();
    const int count = mo->propertyCount();

    // don't save built-in properties if there aren't any qml properties
    if (offset == 1)
        return;

    for (int i = offset; i < count; ++i) {
        QMetaProperty property = mo->property(i);
        const QString propertyName = QString::fromUtf8(property.name());

        const QVariant previousValue = readProperty(property);
        const QVariant currentValue = instance()->value(propertyName,
                                                        previousValue);

        if (!currentValue.isNull() && (!previousValue.isValid()
                                       || (currentValue.canConvert(previousValue.metaType())
                                           && previousValue != currentValue))) {
            property.write(this, currentValue);
            //qDebug() << "BS.ExtraConfig: load " << property.name() << ":" << currentValue << "default:" << previousValue;
        }

        // ensure that a non-existent setting gets written
        // even if the property wouldn't change later
        if (!instance()->contains(propertyName))
            _q_propertyChanged();

        // setup change notifications on first load
        if (!m_initialized && property.hasNotifySignal()) {
            static const int propertyChangedIndex = mo->indexOfSlot("_q_propertyChanged()");
            QMetaObject::connect(this, property.notifySignalIndex(), this, propertyChangedIndex);
        }
    }
}

void QmlExtraConfig::store()
{
    for (auto it = m_changedProperties.cbegin(); it != m_changedProperties.cend(); ++it) {
        instance()->setValue(QString::fromUtf8(it.key()), it.value());
        //qDebug() << "BS.ExtraConfig: store" << it.key() << ":" << it.value();
    }
    m_changedProperties.clear();
}

void QmlExtraConfig::_q_propertyChanged()
{
    const QMetaObject *mo = metaObject();
    const int offset = mo->propertyOffset();
    const int count = mo->propertyCount();
    for (int i = offset; i < count; ++i) {
        const QMetaProperty &property = mo->property(i);
        const QVariant value = readProperty(property);
        m_changedProperties.insert(property.name(), value);
        //qDebug() << "BS.ExtraConfig: cache" << property.name() << ":" << value;
    }
    if (m_timerId != 0)
        killTimer(m_timerId);
    m_timerId = startTimer(500);
}

QVariant QmlExtraConfig::readProperty(const QMetaProperty &property) const
{
    QVariant var = property.read(this);
    if (var.metaType() == QMetaType::fromType<QJSValue>())
        var = var.value<QJSValue>().toVariant();
    return var;
}


#include "moc_brickstore_wrapper.cpp"
#include "moc_brickstore_wrapper_p.cpp"
