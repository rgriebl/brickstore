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

#include <vector>

#include <QObject>
#include <QUndoCommand>
#include <QMultiHash>
#include <QModelIndex>
#include <QPointer>

#include "bricklink/global.h"
#include "bricklink/order.h"
#include "common/documentmodel.h"
#include "qcoro/qcoro.h"

QT_FORWARD_DECLARE_CLASS(QTimer)
QT_FORWARD_DECLARE_CLASS(QItemSelectionModel)

class DocumentModel;
class Document;
class ColumnChangeWatcher;
class QmlLots;


enum {
    CID_Column = 0x0001000,
    CID_ColumnLayout,
};

class ColumnCmd : public QUndoCommand
{
public:
    enum class Type { Move, Resize, Hide };

    ColumnCmd(Document *document, Type type, int logical, int oldValue, int newValue);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    Document *m_document;
    Type m_type;
    int m_logical;
    int m_oldValue;
    int m_newValue;
};

struct ColumnData
{
    int m_size;
    int m_visualIndex;
    bool m_hidden;
};

class ColumnLayoutCmd : public QUndoCommand
{
public:
    ColumnLayoutCmd(Document *document, const QVector<ColumnData> &columnData);
    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void redo() override;
    void undo() override;

private:
    Document *m_document;
    QVector<ColumnData> m_columnData;
};


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////


class Document : public QObject
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(model(), QString currencyCode READ currencyCode NOTIFY currencyCodeChanged)
    Q_PRIVATE_PROPERTY(model(), int lotCount READ lotCount NOTIFY lotCountChanged)
    Q_PRIVATE_PROPERTY(model(), bool modified READ isModified NOTIFY modificationChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QImage thumbnail READ thumbnail WRITE setThumbnail NOTIFY thumbnailChanged)
    Q_PROPERTY(BrickLink::Order *order READ order CONSTANT)
    Q_PROPERTY(DocumentModel *model READ model CONSTANT)
    Q_PROPERTY(bool blockingOperationActive READ isBlockingOperationActive NOTIFY blockingOperationActiveChanged)
    Q_PROPERTY(bool blockingOperationCancelable READ isBlockingOperationCancelable NOTIFY blockingOperationCancelableChanged)
    Q_PROPERTY(QString blockingOperationTitle READ blockingOperationTitle WRITE setBlockingOperationTitle NOTIFY blockingOperationTitleChanged)
    Q_PROPERTY(LotList selectedLots READ selectedLots NOTIFY selectedLotsChanged)
    Q_PROPERTY(bool restoredFromAutosave READ isRestoredFromAutosave CONSTANT)
    Q_PROPERTY(QItemSelectionModel *selectionModel READ selectionModel CONSTANT)
    Q_PROPERTY(QObject *lots READ lots NOTIFY lotsChanged) // QML lots API adapter
    Q_ENUMS(DocumentModel::Field)

public:
    Document(QObject *parent = nullptr);
    Document(DocumentModel *model, QObject *parent = nullptr);
    Document(DocumentModel *model, BrickLink::Order *order, QObject *parent = nullptr);
    Document(DocumentModel *model, const QByteArray &columnsState, QObject *parent = nullptr);
    Document(DocumentModel *model, const QByteArray &columnsState, bool restoredFromAutosave, QObject *parent = nullptr);
    ~Document() override;

    Q_INVOKABLE void ref();
    Q_INVOKABLE void deref();
    int refCount() const;

    void setActive(bool active);

    QCoro::Task<bool> requestClose();

    QString filePath() const;
    QString fileName() const;
    void setFilePath(const QString &str);
    QString title() const;
    void setTitle(const QString &title);
    QString filePathOrTitle() const;

    QImage thumbnail() const;
    void setThumbnail(const QImage &image);

    void updateSelection();

    BrickLink::Order *order() const;

    DocumentModel *model() const { return m_model; }
    QItemSelectionModel *selectionModel() const { return m_selectionModel; }
    QModelIndex currentIndex() const;

    const LotList &selectedLots() const  { return m_selectedLots; }

    bool isBlockingOperationActive() const;
    Q_INVOKABLE void startBlockingOperation(const QString &title, std::function<void()> cancelCallback = { });
    Q_INVOKABLE void endBlockingOperation();

    bool isBlockingOperationCancelable() const;
    void setBlockingOperationCancelCallback(std::function<void()> cancelCallback);
    Q_INVOKABLE void cancelBlockingOperation();

    QString blockingOperationTitle() const;
    void setBlockingOperationTitle(const QString &title);

    bool isRestoredFromAutosave() const;

    Q_INVOKABLE void gotoNextErrorOrDifference(bool difference = false);

    Q_INVOKABLE void setStatus(BrickLink::Status status);
    Q_INVOKABLE void setCondition(BrickLink::Condition condition);
    Q_INVOKABLE void setSubCondition(BrickLink::SubCondition subCondition);
    Q_INVOKABLE void setRetain(bool retain);
    Q_INVOKABLE void setStockroom(BrickLink::Stockroom stockroom);

    Q_INVOKABLE void copyFields(const BrickLink::LotList &srcLots, DocumentModel::MergeMode defaultMergeMode,
                                const QHash<DocumentModel::Field, DocumentModel::MergeMode> &fieldMergeModes);
    Q_INVOKABLE void subtractItems(const BrickLink::LotList &subLots);
    Q_INVOKABLE void resetDifferenceMode();

    Q_INVOKABLE void cut();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void duplicate();
    Q_INVOKABLE void remove();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectNone();

    Q_INVOKABLE void toggleStatus();
    Q_INVOKABLE void toggleRetain();
    Q_INVOKABLE void setCost(double cost);

    enum class SpreadCost { ByPrice, ByWeight };

    Q_INVOKABLE void spreadCost(double spreadAmount, Document::SpreadCost how);
    Q_INVOKABLE void roundCost();
    Q_INVOKABLE void setFilterFromSelection();
    Q_INVOKABLE void setPrice(double price);
    Q_INVOKABLE void setPriceToGuide(BrickLink::Time time, BrickLink::Price price, bool forceUpdate);
    Q_INVOKABLE void priceAdjust(bool isFixed, double value, bool applyToTiers);
    Q_INVOKABLE void roundPrice();
    Q_INVOKABLE void costAdjust(bool isFixed, double value);
    Q_INVOKABLE void divideQuantity(int divisor);
    Q_INVOKABLE void multiplyQuantity(int factor);
    Q_INVOKABLE void setSale(int sale);
    Q_INVOKABLE void setBulkQuantity(int qty);
    Q_INVOKABLE void setQuantity(int quantity);
    Q_INVOKABLE void setRemarks(const QString &remarks);
    Q_INVOKABLE void addRemarks(const QString &addRemarks);
    Q_INVOKABLE void removeRemarks(const QString &remRemarks);
    Q_INVOKABLE void setComments(const QString &comments);
    Q_INVOKABLE void addComments(const QString &addComments);
    Q_INVOKABLE void removeComments(const QString &remComments);
    Q_INVOKABLE void setReserved(const QString &reserved);
    Q_INVOKABLE void setMarkerText(const QString &text);
    Q_INVOKABLE void setMarkerColor(const QColor &color);
    Q_INVOKABLE void clearMarker();

    Q_INVOKABLE QCoro::Task<> exportBrickLinkXMLToFile();
    Q_INVOKABLE QCoro::Task<> exportBrickLinkXMLToClipboard();
    Q_INVOKABLE QCoro::Task<> exportBrickLinkUpdateXMLToClipboard();
    Q_INVOKABLE QCoro::Task<> exportBrickLinkInventoryRequestToClipboard();
    Q_INVOKABLE QCoro::Task<> exportBrickLinkWantedListToClipboard();

    Q_INVOKABLE void moveColumn(int logical, int oldVisual, int newVisual);
    Q_INVOKABLE void resizeColumn(int logical, int oldSize, int newSize);
    Q_INVOKABLE void hideColumn(int logical, bool oldHidden, int newHidden);
    Q_INVOKABLE void resizeColumnsToDefault(bool simpleMode = false);

    QByteArray saveColumnsState() const;

    QVector<ColumnData> columnLayout() const;

    enum class ColumnLayoutCommand {
        BrickStoreDefault,
        BrickStoreSimpleDefault,
        AutoResize,
        UserDefault,
        User,
    };
    Q_ENUM(ColumnLayoutCommand)

    static std::vector<ColumnLayoutCommand> columnLayoutCommands();
    static QString columnLayoutCommandName(ColumnLayoutCommand clc);
    static QString columnLayoutCommandId(ColumnLayoutCommand clc);
    static ColumnLayoutCommand columnLayoutCommandFromId(const QString &id);

    Q_INVOKABLE QCoro::Task<> saveCurrentColumnLayout();
    Q_INVOKABLE void setColumnLayoutFromId(const QString &layoutId);

    static QCoro::Task<Document *> load(const QString &fileName = { });
    static Document *loadFromFile(const QString &fileName);
    void saveToFile(const QString &fileName);
    QCoro::Task<bool> save(bool saveAs);

    static void setQmlLotsFactory(std::function<QObject *(Document *)> factory);
    QObject *lots();

public:
    static Document *fromPartInventory(const BrickLink::Item *preselect = nullptr,
                                       const BrickLink::Color *color = nullptr, int multiply = 1,
                                       BrickLink::Condition condition = BrickLink::Condition::New,
                                       BrickLink::Status extraParts = BrickLink::Status::Extra,
                                       bool includeInstructions = false);

    static int restorableAutosaves();
    enum class AutosaveAction { Restore, Delete };
    static int processAutosaves(AutosaveAction action);

signals:
    void closeAllViews();

    void filePathChanged(const QString &filePath);
    void fileNameChanged(const QString &fileName);
    void titleChanged(const QString &title);
    void thumbnailChanged(const QImage &image);
    void selectedLotsChanged(const BrickLink::LotList &);
    void ensureVisible(const QModelIndex &idx, bool centerItem = false);

    void requestActivation();
    void resizeColumnsToContents();

    void columnPositionChanged(int logical, int newVisual);
    void columnSizeChanged(int logical, int newSize);
    void columnHiddenChanged(int logical, bool newHidden);
    void columnLayoutChanged();

    void blockingOperationActiveChanged(bool blocked);
    void blockingOperationCancelableChanged(bool cancelable);
    void blockingOperationTitleChanged(const QString &title);
    void blockingOperationProgress(int done, int total);

    // relayed from Document for the QML API
    void currencyCodeChanged(const QString &ccode);
    void lotCountChanged(int lotCount);
    void modificationChanged(bool modified);

    void lotsChanged();

private:
    QString actionText() const;
    void applyTo(const LotList &lots, std::function<bool(const Lot &, Lot &)> callback);
    void priceGuideUpdated(BrickLink::PriceGuide *pg);
    void cancelPriceGuideUpdates();
    enum ExportCheckMode {
        ExportToFile = 0,
        ExportToClipboard = 1,
        ExportUpdate = 2
    };

    QCoro::Task<BrickLink::LotList> exportCheck(int exportCheckMode) const;
    void updateItemFlagsMask();
    void documentDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    static std::tuple<QVector<ColumnData>, QVector<QPair<int, Qt::SortOrder>>> parseColumnsState(const QByteArray &cl);
    QVector<ColumnData> defaultColumnLayout(bool simpleMode = false);

    void moveColumnDirect(int logical, int newVisual);
    void resizeColumnDirect(int logical, int newSize);
    void hideColumnDirect(int logical, bool newHidden);
    void setColumnLayoutDirect(QVector<ColumnData> &columnData);

    void autosave() const;
    void deleteAutosave();

private:
    QBasicAtomicInt      m_ref = 0;
    DocumentModel *      m_model;
    QItemSelectionModel *m_selectionModel;
    LotList              m_selectedLots;
    QTimer *             m_delayedSelectionUpdate = nullptr;

    bool                 m_hasBeenActive = false;
    QObject *            m_actionConnectionContext = nullptr;

    std::vector<std::pair<const char *, std::function<void()>>> m_actionTable;

    struct SetToPriceGuideData
    {
        std::vector<std::pair<Lot *, Lot>> changes;
        QMultiHash<BrickLink::PriceGuide *, Lot *>    priceGuides;
        int              failCount = 0;
        int              doneCount = 0;
        int              totalCount = 0;
        BrickLink::Time  time;
        BrickLink::Price price;
        double           currencyRate;
        bool             canceled = false;
    };
    QScopedPointer<SetToPriceGuideData> m_setToPG;

    QVector<ColumnData>   m_columnData;

    QString               m_filePath;
    QString               m_title;
    QImage                m_thumbnail;

    QPointer<BrickLink::Order> m_order;

    bool                  m_blocked = false;
    QString               m_blockTitle;
    std::function<void()> m_blockCancelCallback;

    QUuid                 m_uuid;  // for autosave
    QTimer                m_autosaveTimer;
    mutable bool          m_autosaveClean = true;
    bool                  m_restoredFromAutosave = false;

    static std::function<QObject *(Document *)> s_qmlLotsFactory;
    QPointer<QObject>     m_qmlLots;

    friend class AutosaveJob;
    friend void ColumnCmd::redo();
    friend void ColumnLayoutCmd::redo();
};
