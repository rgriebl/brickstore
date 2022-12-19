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

#include <QFrame>

#include "bricklink/lot.h"
#include "bricklink/model.h"
#include "qcoro/task.h"

QT_FORWARD_DECLARE_CLASS(QAction)
class InventoryWidgetPrivate;


class InventoryWidget : public QFrame
{
    Q_OBJECT
public:
    explicit InventoryWidget(bool showCanBuild, QWidget *parent);
    explicit InventoryWidget(QWidget *parent);
    ~InventoryWidget() override;

    using Mode = BrickLink::InventoryModel::Mode;

    Mode mode() const;
    void setMode(Mode newMode);

    void setItem(const BrickLink::Item *item, const BrickLink::Color *color = nullptr, int quantity = 1);
    void setItems(const BrickLink::LotList &lots);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    BrickLink::InventoryModel::SimpleLot selected() const;

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);

signals:
    void customActionTriggered();

protected:
    void languageChange();
    void actionEvent(QActionEvent *e) override;
    void changeEvent(QEvent *e) override;

private slots:
    QCoro::Task<> partOut();

private:
    void updateModel(const QVector<BrickLink::InventoryModel::SimpleLot> &lots);

    std::unique_ptr<InventoryWidgetPrivate> d;
};
