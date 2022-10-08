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
#include "common/actionmanager.h"
#include "common/config.h"
#include "common/currency.h"
#include "common/documentmodel.h"
#include "common/eventfilter.h"

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

    const LotList &selectedLots() const;

    void printScriptAction(PrintingScriptAction *printingAction);

    void setActive(bool active);

    const QHeaderView *headerView() const;

    void setLatestRow(int row);

    double zoomFactor() const;
    void setZoomFactor(double zoom);

signals:
    void currentColumnOrderChanged(const QVector<int> &newOrder);

protected:
    void resizeEvent(QResizeEvent *e) override;
    void changeEvent(QEvent *e) override;
    EventFilter::Result zoomFilter(QObject *o, QEvent *e);

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
    double               m_zoom = 1.;

    ActionManager::ActionTable m_actionTable;
};
