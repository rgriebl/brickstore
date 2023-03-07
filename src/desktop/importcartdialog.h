// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include <QDateTime>

#include "ui_importcartdialog.h"

class Transfer;
class TransferJob;
class CartModel;


class ImportCartDialog : public QDialog, private Ui::ImportCartDialog
{
    Q_OBJECT
public:
    ImportCartDialog(QWidget *parent = nullptr);
    ~ImportCartDialog() override;

    void updateCarts();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void changeEvent(QEvent *e) override;
    void languageChange();

protected slots:
    void checkSelected();
    void updateStatusLabel();

    void importCarts(const QModelIndexList &rows);
    void showCartsOnBrickLink();

private:
    QPushButton *w_import;
    QPushButton *w_showOnBrickLink;
    QDateTime m_lastUpdated;
    QString m_updateMessage;
};
