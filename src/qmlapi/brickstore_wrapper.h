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

#include <QQmlParserStatus>
#include <QPointer>
#include <QIdentityProxyModel>
#include <QSortFilterProxyModel>
#include <QClipboard>

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
public:
    Q_ENUMS(QClipboard::Mode)

    Q_INVOKABLE void clear(QClipboard::Mode mode = QClipboard::Clipboard);
    Q_INVOKABLE QString text(QClipboard::Mode mode = QClipboard::Clipboard) const;
    Q_INVOKABLE void setText(const QString &text, QClipboard::Mode mode = QClipboard::Clipboard);
};

class QmlUtility : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE int naturalCompare(const QString &s1, const QString &s2) const { return Utility::naturalCompare(s1, s2); }

    Q_INVOKABLE QColor gradientColor(const QColor &c1, const QColor &c2, float f = 0.5) { return Utility::gradientColor(c1, c2, f); }
    Q_INVOKABLE QColor textColor(const QColor &backgroundColor) { return Utility::textColor(backgroundColor); }
    Q_INVOKABLE QColor contrastColor(const QColor &c, float f) { return Utility::contrastColor(c, f); }

    Q_INVOKABLE QString weightToString(double gramm, QLocale::MeasurementSystem ms, bool optimize = false, bool show_unit = false) { return Utility::weightToString(gramm, ms, optimize, show_unit); }
    Q_INVOKABLE double stringToWeight(const QString &s, QLocale::MeasurementSystem ms) { return Utility::stringToWeight(s, ms); }

    Q_INVOKABLE double roundTo(double f, int decimals) { return Utility::roundTo(f, decimals); }
};

class QmlBrickStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString defaultCurrencyCode READ defaultCurrencyCode NOTIFY defaultCurrencyCodeChanged)
    Q_PROPERTY(QString versionNumber READ versionNumber CONSTANT)
    Q_PROPERTY(QString buildNumber READ buildNumber CONSTANT)
    Q_PROPERTY(RecentFiles *recentFiles READ recentFiles CONSTANT)
    Q_PROPERTY(DocumentList *documents READ documents CONSTANT)
    Q_PROPERTY(Document *activeDocument READ activeDocument NOTIFY activeDocumentChanged)
    Q_PROPERTY(ColumnLayoutsModel *columnLayouts READ columnLayouts CONSTANT)
    Q_PROPERTY(QVariantMap about READ about CONSTANT)

public:
    static void registerTypes();

    QmlBrickStore();

    static QmlBrickStore *inst();

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
                                              BrickLink::QmlBrickLink::Condition condition,
                                              BrickLink::QmlBrickLink::Status extraParts,
                                              bool includeInstructions, bool includeAlternates,
                                              bool includeCounterParts);

    Q_INVOKABLE void updateDatabase();

    Document *activeDocument() const;

    Q_INVOKABLE LDraw::RenderController *createRenderController(QObject *parent);

    Q_INVOKABLE bool checkBrickLinkLogin();

signals:
    void defaultCurrencyCodeChanged(const QString &defaultCurrencyCode);
    void showSettings(const QString &page);
    void activeDocumentChanged(Document *doc);

private:
    static QmlBrickStore *s_inst;
    ColumnLayoutsModel *m_columnLayouts;
};

Q_DECLARE_METATYPE(QmlBrickStore *)
