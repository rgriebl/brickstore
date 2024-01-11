// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <functional>

#include <QWidget>
#include <QHash>
#include <QTimer>

#include "common/actionmanager.h"
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

    double rowHeightFactor() const;
    void setRowHeightFactor(double factor);

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
    double               m_rowHeightFactor = 1.;

    ActionManager::ActionTable m_actionTable;
};
