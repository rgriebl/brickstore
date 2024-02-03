// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QStackedWidget>
#include <QPointer>
#include <QLabel>
#include <QTreeView>

#include "common/documentmodel.h"
#include "common/document.h"
#include "inventorywidget.h"
#include "picturewidget.h"
#include "priceguidewidget.h"

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QDockWidget)


class TaskPriceGuideWidget : public PriceGuideWidget
{
    Q_OBJECT

public:
    TaskPriceGuideWidget(QWidget *parent = nullptr);

protected slots:
    void documentUpdate(Document *document);
    void selectionUpdate();
    void currencyUpdate(const QString &ccode);
    virtual void topLevelChanged(bool);
    virtual void dockLocationChanged(Qt::DockWidgetArea);

    void setPrice(double p);

protected:
    bool event(QEvent *e) override;
    void fixParentDockWindow();

private:
    QPointer<Document> m_document;
    QDockWidget *m_dock;
    QTimer m_delayTimer;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class TaskInfoWidget : public QStackedWidget
{
    Q_OBJECT

public:
    TaskInfoWidget(QWidget *parent = nullptr);
    ~TaskInfoWidget() override;

protected slots:
    void documentUpdate(Document *document);
    void selectionUpdate();
    void statisticsUpdate();
    void currencyUpdate();

    void languageChange();
    void refresh();

protected:
    void changeEvent(QEvent *e) override;

private:
    void delayedSelectionUpdate();

    QLabel *m_text;
    PictureWidget *m_pic;
    QPointer<Document> m_document;
    QTimer m_delayTimer;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

class TaskInventoryWidget : public InventoryWidget
{
    Q_OBJECT

public:
    TaskInventoryWidget(QWidget *parent = nullptr);
    ~TaskInventoryWidget() override;

protected slots:
    void documentUpdate(Document *document);
    void selectionUpdate();

protected:
    void languageChange();
    void changeEvent(QEvent *e) override;

private:
    QPointer<Document> m_document;
    QTimer m_delayTimer;
    QAction *m_invGoToAction;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class TaskOpenDocumentsWidget : public QTreeView
{
    Q_OBJECT

public:
    TaskOpenDocumentsWidget(QWidget *parent = nullptr);

protected:
    void languageChange();
    void changeEvent(QEvent *e) override;

private:
    QMenu *m_contextMenu;
    QAction *m_closeDocument;
};


// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------


class TaskRecentDocumentsWidget : public QTreeView
{
    Q_OBJECT

public:
    TaskRecentDocumentsWidget(QWidget *parent = nullptr);
};
