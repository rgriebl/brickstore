/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __CWINDOW_H__
#define __CWINDOW_H__

#include <QWidget>
#include <QRegExp>
#include <QHash>
#include <qdom.h>

//#include "citemview.h"
#include "bricklink.h"
#include "cdocument.h"
#include "cconfig.h"
#include "cmoney.h"

class QToolButton;
class QComboBox;
//class QListViewItem;
//class CItemView;
class QTableView;
class CFrameWork;
class CUndoStack;
class QLabel;


class CWindow : public QWidget, public IDocumentView {
    Q_OBJECT

public:
    CWindow(CDocument *doc, QWidget *parent = 0);
    ~CWindow();

    CDocument *document() { return m_doc; }

    enum Filter {
        All        = -1,

        Prices     = -2,
        Texts      = -3,
        Quantities = -4,

        FilterCountSpecial = 4
    };

    enum MergeFlags {
        MergeAction_None         = 0x00000000,
        MergeAction_Force        = 0x00000001,
        MergeAction_Ask          = 0x00000002,
        MergeAction_Mask         = 0x0000ffff,

        MergeKeep_Old            = 0x00000000,
        MergeKeep_New            = 0x00010000,
        MergeKeep_Mask           = 0xffff0000
    };

    CDocument::ItemList sortedItems(bool selection_only = false) const;

    uint setItems(const BrickLink::InvItemList &items, int multiply = 1);
    uint addItems(const BrickLink::InvItemList &items, int multiply = 1, uint mergeflags = MergeAction_None, bool dont_change_sorting = false);
    void deleteItems(const BrickLink::InvItemList &items);

    void mergeItems(const CDocument::ItemList &items, int globalmergeflags = MergeAction_Ask);

    void subtractItems(const BrickLink::InvItemList &items);
    void copyRemarks(const BrickLink::InvItemList &items);

// bool hasFilter ( ) const;
// void setFilter ( const QRegExp &exp, Field f );
// void setFilterExpression ( const QRegExp &exp );
// void setFilterField ( Field f );
// QRegExp filterExpression ( ) const;
// Field filterField ( ) const;

// InvItemList &selectedItems ( );
// void setSelectedItems ( const InvItemList &items );

    virtual QDomElement createGuiStateXML(QDomDocument doc);
    virtual bool parseGuiStateXML(QDomElement root);

    bool isDifferenceMode() const;

public slots:
    void setDifferenceMode(bool);

    void fileSave();
    void fileSaveAs();
    void fileExportBrickLinkXML();
    void fileExportBrickLinkXMLClipboard();
    void fileExportBrickLinkUpdateClipboard();
    void fileExportBrickLinkInvReqClipboard();
    void fileExportBrickLinkWantedListClipboard();
    void fileExportBrikTrakInventory();

    void filePrint();

    void editCut();
    void editCopy();
    void editPaste();
    void editDelete();

    void editSubtractItems();
    void editCopyRemarks();
    void editMergeItems();
    void editPartOutItems();

    void editResetDifferences();

    void selectAll();
    void selectNone();

    void editStatusInclude();
    void editStatusExclude();
    void editStatusExtra();
    void editStatusToggle();
    void editConditionNew();
    void editConditionUsed();
    void editConditionToggle();
    void editColor();
    void editQtyMultiply();
    void editQtyDivide();
    void editPrice();
    void editPriceRound();
    void editPriceToPG();
    void editPriceIncDec();
    void editBulk();
    void editSale();
    void editComment();
    void editRemark();
    void editRetainYes();
    void editRetainNo();
    void editRetainToggle();
    void editStockroomYes();
    void editStockroomNo();
    void editStockroomToggle();
    void editReserved();
    void addRemark();
    void removeRemark();
    void addComment();
    void removeComment();

    void addItem(BrickLink::InvItem *, uint);

    void setPrice(money_t);

    void showBLCatalog();
    void showBLPriceGuide();
    void showBLLotsForSale();
    void showBLMyInventory();

    void saveDefaultColumnLayout();

signals:
    void selectionChanged(const BrickLink::InvItemList &);
    void statisticsChanged();

protected:
    void closeEvent(QCloseEvent *e);

protected slots:
    void languageChange();

private slots:
    void applyFilter();
    void updateCaption();

    void contextMenu(const QPoint &);
    void priceGuideUpdated(BrickLink::PriceGuide *);
    void updateErrorMask();

    void itemsAddedToDocument(const CDocument::ItemList &);
    void itemsRemovedFromDocument(const CDocument::ItemList &);
    void itemsChangedInDocument(const CDocument::ItemList &, bool);

private:
    CDocument::ItemList exportCheck() const;

private:
    CDocument * m_doc;
    QRegExp        m_filter_expression;
    int            m_filter_field;

// bool m_ignore_selection_update;

// QHash<int, CItemViewItem *>  m_lvitems;

    QToolButton *  w_filter_clear;
    QComboBox *    w_filter_expression;
    QComboBox *    w_filter_field;
    QTableView *   w_list;
    QLabel *       w_filter_label;
    QLabel *       w_filter_field_label;

    uint                           m_settopg_failcnt;
    QMultiHash<BrickLink::PriceGuide *, CDocument::Item *> *m_settopg_list;
    BrickLink::Time    m_settopg_time;
    BrickLink::Price   m_settopg_price;
};

#endif
