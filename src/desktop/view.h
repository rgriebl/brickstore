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

#include <functional>

#include <QWidget>
#include <QHash>
#include <QTimer>

#include "bricklink/global.h"
#include "bricklink/lot.h"
#include "common/config.h"
#include "common/documentmodel.h"
#include "utility/currency.h"

QT_FORWARD_DECLARE_CLASS(QToolButton)
QT_FORWARD_DECLARE_CLASS(QProgressBar)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QPrinter)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QFrame)
QT_FORWARD_DECLARE_CLASS(QSizeGrip)
QT_FORWARD_DECLARE_CLASS(QHeaderView)

class MainWindow;
class Document;
class TableView;
class HeaderView;
class StatusBar;
class ColumnChangeWatcher;
class PrintingScriptAction;


class View : public QWidget
{
    Q_OBJECT

public:
    View(Document *document, QWidget *parent = nullptr);
    ~View() override;

    Document *document() const { return m_document; }
    DocumentModel *model() const { return m_model; }

    enum class Consolidate {
        Not = -1,
        IntoTopSorted = 0,
        IntoBottomSorted = 1,
        IntoLowestIndex = 2,
        IntoHighestIndex = 3,
        IntoExisting = 4,
        IntoNew = 5
    };

    enum class AddLotMode {
        AddAsNew,
        ConsolidateWithExisting,
        ConsolidateInteractive,
    };

    const LotList &selectedLots() const;

    QCoro::Task<> addLots(LotList &&lots, AddLotMode addLotMode = AddLotMode::AddAsNew);

    QCoro::Task<> consolidateLots(const LotList &lots);

    void printScriptAction(PrintingScriptAction *printingAction);

    void setActive(bool active);

    const QHeaderView *headerView() const;

signals:
    void currentColumnOrderChanged(const QVector<int> &newOrder);

protected:
    void resizeEvent(QResizeEvent *e) override;
    void changeEvent(QEvent *e) override;

    void print(bool aspdf);

private slots:
    void ensureLatestVisible();
    void updateCaption();
    void updateMinimumSectionSize();

    void contextMenu(const QPoint &pos);

private:
    void languageChange();
    void repositionBlockOverlay();
    QCoro::Task<> partOutItems();
    int consolidateLotsHelper(const LotList &lots, Consolidate conMode) const;

    friend class ColumnCmd;

    bool printPages(QPrinter *prt, const LotList &lots, const QList<uint> &pages, double scaleFactor,
                    uint *maxPageCount, double *maxWidth);

private:
    Document *           m_document;  // ref counted
    DocumentModel *      m_model;     // owned by m_document
    TableView *          m_table;
    HeaderView *         m_header;
    ColumnChangeWatcher *m_ccw = nullptr;
    QMenu *              m_contextMenu = nullptr;

    QSizeGrip *          m_sizeGrip;
    QFrame *             m_blockOverlay;
    QProgressBar *       m_blockProgress;
    QLabel *             m_blockTitle;
    QToolButton *        m_blockCancel;

    int                  m_latest_row;
    QTimer *             m_latest_timer;

    QObject *            m_actionConnectionContext = nullptr;

    std::vector<std::pair<const char *, std::function<void() > > > m_actionTable;
};

Q_DECLARE_METATYPE(View::Consolidate)
