// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <vector>
#include <functional>

#include <QObject>
#include <QUndoCommand>
#include <QMultiHash>
#include <QModelIndex>
#include <QPointer>

#include <QCoro/QCoroTask>

#include "bricklink/global.h"
#include "bricklink/lot.h"
#include "bricklink/order.h"
#include "common/actionmanager.h"
#include "common/documentmodel.h"

QT_FORWARD_DECLARE_CLASS(QTimer)
QT_FORWARD_DECLARE_CLASS(QItemSelectionModel)

class DocumentModel;
class Document;
class ColumnChangeWatcher;
class QmlDocumentLots;


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

public:
    Document(QObject *parent = nullptr);
    Document(DocumentModel *model, QObject *parent = nullptr);
    Document(DocumentModel *model, const QByteArray &columnsState, QObject *parent = nullptr);
    Document(DocumentModel *model, const QByteArray &columnsState, bool restoredFromAutosave, QObject *parent = nullptr);
    ~Document() override;

    void setActive(bool active);

    QCoro::Task<bool> requestClose();

    QString filePath() const;
    QString fileName() const;
    void setFilePath(const QString &str);
    QString title() const;
    void setTitle(const QString &title);
    QString filePathOrTitle() const;
    QString fileNameOrTitle() const;

    QImage thumbnail() const;
    void setThumbnail(const QImage &image);
    void setThumbnail(const QString &iconName);

    void updateSelection();

    BrickLink::Order *order() const;
    void setOrder(BrickLink::Order *order);

    DocumentModel *model() const { return m_model; }
    QItemSelectionModel *selectionModel() const { return m_selectionModel; }
    QModelIndex currentIndex() const;

    const LotList &selectedLots() const  { return m_selectedLots; }

    bool isBlockingOperationActive() const;
    void startBlockingOperation(const QString &title, const std::function<void ()> &cancelCallback = { });
    void endBlockingOperation();

    bool isBlockingOperationCancelable() const;
    void setBlockingOperationCancelCallback(const std::function<void ()> &cancelCallback);
    void cancelBlockingOperation();

    QString blockingOperationTitle() const;
    void setBlockingOperationTitle(const QString &title);

    bool isRestoredFromAutosave() const;

    void gotoNextErrorOrDifference(bool difference = false);

    void setStatus(BrickLink::Status status);
    void setCondition(BrickLink::Condition condition);
    void setSubCondition(BrickLink::SubCondition subCondition);
    void setRetain(bool retain);
    void setStockroom(BrickLink::Stockroom stockroom);

    void copyFields(const BrickLink::LotList &srcLots, const DocumentModel::FieldMergeModes &fieldMergeModes);
    void subtractItems(const BrickLink::LotList &subLots);
    void resetDifferenceMode();

    void cut();
    void copy();
    void duplicate();
    void remove();
    void selectAll();
    void selectNone();
    void selectLots(const BrickLink::LotList &lots, const BrickLink::Lot *current = nullptr,
                    int currentColumn = 0);

    void toggleStatus();
    void toggleRetain();
    void setCost(double cost);

    enum class SpreadCost { ByPrice, ByWeight };

    void spreadCost(double spreadAmount, Document::SpreadCost how);
    void roundCost();
    void setFilterFromSelection();
    void setPrice(double price);
    void setPriceToGuide(BrickLink::Time time, BrickLink::Price price, bool forceUpdate);
    void priceAdjust(bool isFixed, double value, bool applyToTiers);
    void setRelativeTierPrices(const std::array<double, 3> &percentagesOff);
    void roundPrice();
    void costAdjust(bool isFixed, double value);
    void divideQuantity(int divisor);
    void multiplyQuantity(int factor);
    void setSale(int sale);
    void setBulkQuantity(int qty);
    void setQuantity(int quantity);
    void setRemarks(const QString &remarks);
    void addRemarks(const QString &addRemarks);
    void removeRemarks(const QString &remRemarks);
    void setComments(const QString &comments);
    void addComments(const QString &addComments);
    void removeComments(const QString &remComments);
    void setReserved(const QString &reserved);
    void setMarkerText(const QString &text);
    void setMarkerColor(const QColor &color);
    void clearMarker();
    void copyLotId() const;
    void clearLotId();
    void setColor(const BrickLink::Color *color);

    QCoro::Task<> exportBrickLinkXMLToFile();
    QCoro::Task<> exportBrickLinkXMLToClipboard();
    QCoro::Task<> exportBrickLinkUpdateXMLToClipboard();
    QCoro::Task<> exportBrickLinkInventoryRequestToClipboard();
    QCoro::Task<> exportBrickLinkWantedListToClipboard();

    void moveColumn(int logical, int oldVisual, int newVisual);
    void resizeColumn(int logical, int oldSize, int newSize);
    void hideColumn(int logical, bool oldHidden, int newHidden);
    void resizeColumnsToDefault(bool simpleMode = false);

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

    QCoro::Task<> saveCurrentColumnLayout();
    void setColumnLayoutFromId(const QString &layoutId);

    static QCoro::Task<Document *> load(QString fileName = { });
    static Document *loadFromFile(const QString &fileName);
    void saveToFile(const QString &fileName);
    QCoro::Task<bool> save(bool saveAs);

public:
    static Document *fromPartInventory(const BrickLink::Item *preselect = nullptr,
                                       const BrickLink::Color *color = nullptr, int multiply = 1,
                                       BrickLink::Condition condition = BrickLink::Condition::New,
                                       BrickLink::Status extraParts = BrickLink::Status::Extra,
                                       BrickLink::PartOutTraits partOutTraits = { },
                                       BrickLink::Status status = BrickLink::Status::Include);

    static int restorableAutosaves();
    enum class AutosaveAction { Restore, Delete };
    static int processAutosaves(AutosaveAction action);

signals:
    void closeAllViewsForDocument();

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

    void orderChanged(BrickLink::Order *order);

private:
    void applyTo(const LotList &lots,
                 const char *actionName, const std::function<DocumentModel::ApplyToResult (const Lot &, Lot &)> &callback);
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
    DocumentModel *      m_model;
    QItemSelectionModel *m_selectionModel;
    LotList              m_selectedLots;

    bool                 m_hasBeenActive = false;
    QObject *            m_actionConnectionContext = nullptr;

    ActionManager::ActionTable  m_actionTable;

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
    std::unique_ptr<SetToPriceGuideData> m_setToPG;

    QVector<ColumnData>   m_columnData;

    QString               m_filePath;
    QString               m_fileName; // on mobile, this may be different from m_filePath
    QString               m_title;
    QImage                m_thumbnail;

    BrickLink::Order *    m_order = nullptr;

    bool                  m_blocked = false;
    QString               m_blockTitle;
    std::function<void()> m_blockCancelCallback;

    QUuid                 m_uuid;  // for autosave
    QTimer                m_autosaveTimer;
    mutable bool          m_autosaveClean = true;
    bool                  m_restoredFromAutosave = false;

    friend class AutosaveJob;
    friend void ColumnCmd::redo();
    friend void ColumnLayoutCmd::redo();
};
