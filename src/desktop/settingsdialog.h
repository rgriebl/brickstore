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

#include "ui_settingsdialog.h"

QT_FORWARD_DECLARE_CLASS(QHttp)
QT_FORWARD_DECLARE_CLASS(QBuffer)
QT_FORWARD_DECLARE_CLASS(QSortFilterProxyModel)
class ActionModel;
class ToolBarModel;
class ShortcutModel;


class SettingsDialog : public QDialog, private Ui::SettingsDialog
{
    Q_OBJECT
public:
    SettingsDialog(const QString &goto_page, QWidget *parent = nullptr);

public slots:
    void accept() override;

protected:
    void resizeEvent(QResizeEvent *e) override;

protected slots:
    void selectDocDir(int index);
    void resetUpdateIntervals();
    void currenciesUpdated();
    void currentCurrencyChanged(const QString &);

protected:
    void load();
    void save();

    void checkLDrawDir();

private:
    QString m_preferedCurrency;
    ActionModel *m_tb_model;
    ToolBarModel *m_tb_actions;
    QSortFilterProxyModel *m_tb_proxymodel;
    ShortcutModel *m_sc_model;
    QSortFilterProxyModel *m_sc_proxymodel;
};
