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
