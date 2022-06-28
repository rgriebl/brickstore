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
#pragma once

#include <QtQml/QQmlParserStatus>
#include <QtQml/QJSValue>
#include <QPointer>
#include <QIdentityProxyModel>
#include <QSortFilterProxyModel>
#include <QClipboard>

#include "common/application.h"
#include "common/currency.h"
#include "common/onlinestate.h"
#include "common/config.h"
#include "common/announcements.h"
#include "common/systeminfo.h"

#include "common/document.h"
#include "common/documentlist.h"
#include "utility/utility.h"

class Currency;
class Announcements;
class SystemInfo;
class RecentFiles;
class Config;
class QmlColor;
class QmlCategory;
class QmlItemType;
class QmlItem;
class QmlLot;
class QmlPriceGuide;
class QmlPicture;
namespace LDraw {
class RenderController;
}
class QmlDocumentColumnModel;


class QmlDocumentProxyModel : public QAbstractProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DocumentProxyModel)
    Q_PROPERTY(Document *document READ document WRITE setDocument NOTIFY documentChanged REQUIRED)
    Q_PROPERTY(QAbstractListModel *columnModel READ columnModel CONSTANT)

public:
    QmlDocumentProxyModel(QObject *parent = nullptr);

    Document *document() const;
    void setDocument(Document *doc);

    int rowCount(const QModelIndex &parent = { }) const override;
    int columnCount(const QModelIndex &parent = { }) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = { }) const override;
    QModelIndex parent(const QModelIndex &child) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    QModelIndex mapToSource(const QModelIndex &idx) const override;
    QModelIndex mapFromSource(const QModelIndex &sindex) const override;

    //dropMimeData?

    Q_INVOKABLE int logicalColumn(int visual) const;
    Q_INVOKABLE int visualColumn(int logical) const;

    QHash<int, QByteArray> roleNames() const override;

    QAbstractListModel *columnModel();

signals:
    void forceLayout();
    void documentChanged(Document *doc);

private:
    void update();
    void emitForceLayout();

    void internalHideColumn(int vi, bool visible);
    void internalMoveColumn(int viFrom, int viTo);

    QVector<int> l2v;
    QVector<int> v2l;

    QPointer<Document> m_doc;
    QObject *m_connectionContext = nullptr;
    QTimer *m_forceLayoutDelay = nullptr;
    QmlDocumentColumnModel *m_columnModel = nullptr;

    friend class QmlDocumentColumnModel;
};

class QmlDocumentColumnModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DocumentColumnModel)
    QML_UNCREATABLE("")
    Q_PROPERTY(int count READ rowCount CONSTANT)

public:
    QmlDocumentColumnModel(QmlDocumentProxyModel *proxyModel);

    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void moveColumn(int viFrom, int viTo);
    Q_INVOKABLE void hideColumn(int vi, bool hidden);

private:
    QmlDocumentProxyModel *m_proxyModel;
    friend class QmlDocumentProxyModel;
};


class ColumnLayoutsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ColumnLayoutModel)
    QML_UNCREATABLE("")

public:
    ColumnLayoutsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QHash<int, QByteArray> roleNames() const override;

private:
    void update();

    QVector<QPair<QString, QString>> m_idAndName;
};


class QmlSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SortFilterProxyModel)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int sortColumn READ sortColumn WRITE setSortColumn NOTIFY sortColumnChanged)
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)

    Q_PROPERTY(QByteArray sortRoleName READ sortRoleName WRITE setSortRoleName NOTIFY sortRoleNameChanged)
    Q_PROPERTY(QByteArray filterRoleName READ filterRoleName WRITE setFilterRoleName NOTIFY filterRoleNameChanged)
    Q_PROPERTY(QString filterString READ filterString WRITE setFilterString)
    Q_PROPERTY(FilterSyntax filterSyntax READ filterSyntax WRITE setFilterSyntax NOTIFY filterSyntaxChanged)

public:
    enum FilterSyntax {
        RegularExpression,
        Wildcard,
        FixedString
    };
    Q_ENUM(FilterSyntax)

    QmlSortFilterProxyModel(QObject *parent = nullptr);

    int count() const;

    QByteArray sortRoleName() const;
    void setSortRoleName(const QByteArray &role);

    QByteArray filterRoleName() const;
    void setFilterRoleName(const QByteArray &role);

    void setSortColumn(int newSortColumn);
    void setSortOrder(Qt::SortOrder newSortOrder);

    QString filterString() const;
    void setFilterString(const QString &filter);

    FilterSyntax filterSyntax() const;
    void setFilterSyntax(FilterSyntax syntax);

    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged(int newCount);
    void sortOrderChanged(int newSortOrder);
    void sortColumnChanged(int newSortColumn);

    void sortRoleNameChanged(QByteArray newSortRoleName);
    void filterRoleNameChanged(QByteArray newFilterRoleName);

    void filterSyntaxChanged(QmlSortFilterProxyModel::FilterSyntax newFilterSyntax);

private:
    int roleKey(const QByteArray &role) const;

    FilterSyntax m_filterSyntax = FixedString;
};

class QmlClipboard : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Clipboard)
    QML_SINGLETON

public:
    Q_ENUMS(QClipboard::Mode)

    QmlClipboard() = default;

    Q_INVOKABLE void clear(QClipboard::Mode mode = QClipboard::Clipboard);
    Q_INVOKABLE QString text(QClipboard::Mode mode = QClipboard::Clipboard) const;
    Q_INVOKABLE void setText(const QString &text, QClipboard::Mode mode = QClipboard::Clipboard);
};

class QmlUtility : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Utility)
    QML_SINGLETON

public:
    QmlUtility() = default;

    Q_INVOKABLE bool fuzzyCompare(double d1, double d2) { return Utility::fuzzyCompare(d1, d2); }

    Q_INVOKABLE int naturalCompare(const QString &s1, const QString &s2) const { return Utility::naturalCompare(s1, s2); }

    Q_INVOKABLE QColor gradientColor(const QColor &c1, const QColor &c2, float f = 0.5) { return Utility::gradientColor(c1, c2, f); }
    Q_INVOKABLE QColor textColor(const QColor &backgroundColor) { return Utility::textColor(backgroundColor); }
    Q_INVOKABLE QColor contrastColor(const QColor &c, float f) { return Utility::contrastColor(c, f); }

    Q_INVOKABLE QString weightToString(double gramm, QLocale::MeasurementSystem ms, bool optimize = false, bool show_unit = false) { return Utility::weightToString(gramm, ms, optimize, show_unit); }
    Q_INVOKABLE double stringToWeight(const QString &s, QLocale::MeasurementSystem ms) { return Utility::stringToWeight(s, ms); }

    Q_INVOKABLE double roundTo(double f, int decimals) { return Utility::roundTo(f, decimals); }
};

class QmlThenable : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Thenable)
    QML_UNCREATABLE("")

public:
    QmlThenable(QJSEngine *engine, QObject *parent = nullptr);
    ~QmlThenable() override;

    Q_INVOKABLE void then(const QJSValue &function);

    void callThen(const QVariantList &arguments);

private:
    void callThenInternal(const QVariantList &arguments);

    bool m_thenable = false;
    QJSValue m_function;
    QPointer<QJSEngine> m_engine;
};

class QmlBrickStore : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BrickStore)
    QML_SINGLETON
    Q_PROPERTY(QString defaultCurrencyCode READ defaultCurrencyCode NOTIFY defaultCurrencyCodeChanged)
    Q_PROPERTY(QString versionNumber READ versionNumber CONSTANT)
    Q_PROPERTY(QString buildNumber READ buildNumber CONSTANT)
    Q_PROPERTY(RecentFiles *recentFiles READ recentFiles CONSTANT)
    Q_PROPERTY(DocumentList *documents READ documents CONSTANT)
    Q_PROPERTY(Document *activeDocument READ activeDocument NOTIFY activeDocumentChanged)
    Q_PROPERTY(ColumnLayoutsModel *columnLayouts READ columnLayouts CONSTANT)
    Q_PROPERTY(QVariantMap about READ about CONSTANT)

public:
    QmlBrickStore();

    DocumentList *documents() const;
    Config *config() const;
    QString versionNumber() const;
    QString buildNumber() const;
    RecentFiles *recentFiles() const;
    ColumnLayoutsModel *columnLayouts() const;
    QVariantMap about() const;
    QString defaultCurrencyCode() const;

    Q_INVOKABLE QString symbolForCurrencyCode(const QString &currencyCode) const;
    Q_INVOKABLE QString toCurrencyString(double value, const QString &symbol = { }, int precision = 3) const;
    Q_INVOKABLE QString toWeightString(double value, bool showUnit = false) const;

    Q_INVOKABLE QStringList nameFiltersForBrickLinkXML(bool includeAll = false) const;
    Q_INVOKABLE QStringList nameFiltersForBrickStoreXML(bool includeAll = false) const;
    Q_INVOKABLE QStringList nameFiltersForLDraw(bool includeAll = false) const;

    Q_INVOKABLE Document *importBrickLinkStore(BrickLink::Store *store);
    Q_INVOKABLE Document *importBrickLinkOrder(BrickLink::Order *order);
    Q_INVOKABLE Document *importBrickLinkCart(BrickLink::Cart *cart);

    Q_INVOKABLE Document *importPartInventory(BrickLink::QmlItem item,
                                              BrickLink::QmlColor color, int multiply,
                                              BrickLink::Condition condition,
                                              BrickLink::Status extraParts,
                                              bool includeInstructions, bool includeAlternates,
                                              bool includeCounterParts);

    Q_INVOKABLE void updateDatabase();

    Document *activeDocument() const;

    Q_INVOKABLE QmlThenable *checkBrickLinkLogin();

    Q_INVOKABLE void updateIconTheme(bool darkTheme);

signals:
    void defaultCurrencyCodeChanged(const QString &defaultCurrencyCode);
    void showSettings(const QString &page);
    void activeDocumentChanged(Document *doc);

private:
    ColumnLayoutsModel *m_columnLayouts;
};

class QmlAnnouncements
{
    Q_GADGET
    QML_FOREIGN(Announcements)
    QML_NAMED_ELEMENT(Announcements)
    QML_SINGLETON

public:
     static Announcements *create(QQmlEngine *, QJSEngine *)
     {
         auto a = Application::inst()->announcements();
         QQmlEngine::setObjectOwnership(a, QQmlEngine::CppOwnership);
         return a;
     }
};

class QmlCurrency
{
    Q_GADGET
    QML_FOREIGN(Currency)
    QML_NAMED_ELEMENT(Currency)
    QML_SINGLETON

public:
     static Currency *create(QQmlEngine *, QJSEngine *)
     {
         auto c = Currency::inst();
         QQmlEngine::setObjectOwnership(c, QQmlEngine::CppOwnership);
         return c;
     }
};


class QmlSystemInfo
{
    Q_GADGET
    QML_FOREIGN(SystemInfo)
    QML_NAMED_ELEMENT(SystemInfo)
    QML_SINGLETON

public:
     static SystemInfo *create(QQmlEngine *, QJSEngine *)
     {
         auto si = SystemInfo::inst();
         QQmlEngine::setObjectOwnership(si, QQmlEngine::CppOwnership);
         return si;
     }
};

class QmlConfig
{
    Q_GADGET
    QML_FOREIGN(Config)
    QML_NAMED_ELEMENT(Config)
    QML_SINGLETON

public:
     static Config *create(QQmlEngine *, QJSEngine *)
     {
         auto c = Config::inst();
         QQmlEngine::setObjectOwnership(c, QQmlEngine::CppOwnership);
         return c;
     }
};


class QmlOnlineState
{
    Q_GADGET
    QML_FOREIGN(OnlineState)
    QML_NAMED_ELEMENT(OnlineState)
    QML_SINGLETON

public:
     static OnlineState *create(QQmlEngine *, QJSEngine *)
     {
         auto os = OnlineState::inst();
         QQmlEngine::setObjectOwnership(os, QQmlEngine::CppOwnership);
         return os;
     }
};

class QmlDocument
{
    Q_GADGET
    QML_FOREIGN(Document)
    QML_NAMED_ELEMENT(Document)
    QML_UNCREATABLE("")
};

class QmlDocumentList
{
    Q_GADGET
    QML_FOREIGN(DocumentList)
    QML_NAMED_ELEMENT(DocumentList)
    QML_UNCREATABLE("")
};

class QmlDocumentModel
{
    Q_GADGET
    QML_FOREIGN(DocumentModel)
    QML_NAMED_ELEMENT(DocumentModel)
    QML_UNCREATABLE("")
};

class QmlLots
{
    Q_GADGET
    QML_FOREIGN(QmlDocumentLots)
    QML_NAMED_ELEMENT(Lots)
    QML_UNCREATABLE("")
};


Q_DECLARE_METATYPE(QmlBrickStore *)
