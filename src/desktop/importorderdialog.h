// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include <QDateTime>
#include <QSet>

#include "ui_importorderdialog.h"


class ImportOrderDialog : public QDialog, private Ui::ImportOrderDialog
{
    Q_OBJECT
public:
    ImportOrderDialog(QWidget *parent = nullptr);
    ~ImportOrderDialog() override;

    void updateOrders();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    virtual void changeEvent(QEvent *e) override;
    void languageChange();

protected slots:
    void checkSelected();
    void updateStatusLabel();

    void importOrders(const QModelIndexList &rows, bool combined);

private:
    QPushButton *w_import;
    QPushButton *w_importCombined;
    QMenu *m_contextMenu;
    QAction *m_showOnBrickLink;
    QAction *m_orderInformation;
    QSet<QString> m_selectedCurrencyCodes;
    QString m_updateMessage;
};
