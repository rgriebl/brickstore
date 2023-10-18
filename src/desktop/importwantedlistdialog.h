// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QDialog>
#include <QDateTime>

#include "ui_importwantedlistdialog.h"

class Transfer;
class TransferJob;
class WantedListModel;


class ImportWantedListDialog : public QDialog, private Ui::ImportWantedListDialog
{
    Q_OBJECT
public:
    ImportWantedListDialog(QWidget *parent = nullptr);
    ~ImportWantedListDialog() override;

    void updateWantedLists();

protected:
    void changeEvent(QEvent *e) override;
    void showEvent(QShowEvent *) override;
    void closeEvent(QCloseEvent *) override;
    void keyPressEvent(QKeyEvent *e) override;
    void languageChange();

protected slots:
    void checkSelected();
    void updateStatusLabel();

    void importWantedLists(const QModelIndexList &rows);
    void showWantedListsOnBrickLink();

private:
    QPushButton *w_import;
    QPushButton *w_showOnBrickLink;
    QDateTime m_lastUpdated;
    QString m_updateMessage;
};
