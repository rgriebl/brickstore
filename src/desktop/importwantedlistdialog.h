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

#include <QDialog>
#include <QDateTime>

#include "bricklink/global.h"
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
    void keyPressEvent(QKeyEvent *e) override;
    virtual void changeEvent(QEvent *e) override;
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
    QVector<int> m_wantedListsToOpen;
    QString m_updateMessage;
};
