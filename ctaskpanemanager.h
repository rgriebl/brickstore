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
#ifndef __CTASKPANEMANAGER_H__
#define __CTASKPANEMANAGER_H__

#include <QObject>
#include <QIcon>
#include <QList>
#include <QFrame>

class CTaskPane;
class CTaskPaneManagerPrivate;
class QMainWindow;
class QWidget;
class QAction;
class QMenu;


class CTaskPaneManager : public QObject {
    Q_OBJECT

public:
    CTaskPaneManager(QMainWindow *parent);
    ~CTaskPaneManager();

    enum Mode {
        Classic,
        Modern
    };

    Mode mode() const;
    void setMode(Mode m);

    void addItem(QWidget *w, const QIcon &is, const QString &txt = QString());
    void removeItem(QWidget *w, bool delete_widget = true);

    QString itemText(QWidget *w) const;
    void setItemText(QWidget *w, const QString &txt);

    bool isItemVisible(QWidget *w) const;
    void setItemVisible(QWidget *w, bool  visible);


    QAction *createItemVisibilityAction(QObject *parent = 0, const char *name = 0) const;
    QMenu *createItemVisibilityMenu() const;

private slots:
    void itemMenuAboutToShow();
    void itemMenuTriggered(QAction *);
    void dockVisibilityChanged(bool b);

private:
    void kill();
    void create();

private:
    CTaskPaneManagerPrivate *d;
};

#endif

