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

#include <QTreeView>
#include <QScopedPointer>

#include "bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QAction)
class AppearsInWidgetPrivate;


class AppearsInWidget : public QTreeView
{
    Q_OBJECT
public:
    AppearsInWidget(QWidget *parent = nullptr);
    ~AppearsInWidget() override;

    void setItem(const BrickLink::Item *item, const BrickLink::Color *color = nullptr);
    void setItems(const BrickLink::InvItemList &list);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected slots:
    void showBLCatalogInfo();
    void showBLPriceGuideInfo();
    void showBLLotsForSale();
    void languageChange();

private slots:
    void showContextMenu(const QPoint &);
    void partOut();
    void resizeColumns();

protected:
    void changeEvent(QEvent *e) override;

private:
    const BrickLink::AppearsInItem *appearsIn() const;
    void triggerColumnResize();

private:
    QScopedPointer<AppearsInWidgetPrivate> d;
};
