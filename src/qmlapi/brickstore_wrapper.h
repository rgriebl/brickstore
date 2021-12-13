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
#pragma once

#include <QQmlParserStatus>
#include <QPointer>
#include <QIdentityProxyModel>

#include "common/document.h"
#include "common/documentlist.h"
#include "bricklink_wrapper.h"

class RecentFiles;
class Config;
class QmlColor;
class QmlCategory;
class QmlItemType;
class QmlItem;
class QmlLot;
class QmlPriceGuide;
class QmlPicture;



class QmlDocumentProxyModel : public QAbstractProxyModel
{
    Q_OBJECT
    Q_PROPERTY(Document *document READ document WRITE setDocument NOTIFY documentChanged REQUIRED);
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

signals:
    void forceLayout();
    void documentChanged(Document *doc);

private:
    void update();
    void emitForceLayout();

    QVector<int> l2v;
    QVector<int> v2l;

    QPointer<Document> m_doc;
    QObject *m_connectionContext = nullptr;
    QTimer *m_forceLayoutDelay = nullptr;
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


class QmlBrickStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString defaultCurrencyCode READ defaultCurrencyCode NOTIFY defaultCurrencyCodeChanged)
    Q_PROPERTY(Config * config READ config CONSTANT)
    Q_PROPERTY(QString versionNumber READ versionNumber CONSTANT)
    Q_PROPERTY(QString buildNumber READ buildNumber CONSTANT)
    Q_PROPERTY(RecentFiles *recentFiles READ recentFiles CONSTANT)
    Q_PROPERTY(DocumentList *documents READ documents CONSTANT)
    Q_PROPERTY(ColumnLayoutsModel *columnLayouts READ columnLayouts CONSTANT)
    Q_PROPERTY(QVariantMap about READ about CONSTANT)
    Q_PROPERTY(bool databaseValid READ isDatabaseValid NOTIFY databaseValidChanged)
    Q_PROPERTY(QDateTime lastDatabaseUpdate READ lastDatabaseUpdate NOTIFY lastDatabaseUpdateChanged)

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

    bool isDatabaseValid() const;
    QDateTime lastDatabaseUpdate() const;
    Q_INVOKABLE bool updateDatabase();

signals:
    void defaultCurrencyCodeChanged(const QString &defaultCurrencyCode);
    void showSettings(const QString &page);
    void databaseValidChanged(bool valid);
    void lastDatabaseUpdateChanged(const QDateTime &lastUpdate);


private:
    static QmlBrickStore *s_inst;
    ColumnLayoutsModel *m_columnLayouts;
};

Q_DECLARE_METATYPE(QmlBrickStore *)
