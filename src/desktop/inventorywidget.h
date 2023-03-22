// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QFrame>

#include <QCoro/QCoroTask>

#include "bricklink/lot.h"
#include "bricklink/model.h"

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

    void setActivateAction(QAction *action);

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
