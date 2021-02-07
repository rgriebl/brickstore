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

#include <QLineEdit>
#include <QStringListModel>
#include <QIcon>

QT_FORWARD_DECLARE_CLASS(QAction)
class HistoryViewDelegate;


class HistoryLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    HistoryLineEdit(int maximumHistorySize, QWidget *parent = nullptr);

    bool isInFavoritesMode() const;
    void setToFavoritesMode(bool favoritesMode);

    QString instructionToolTip() const;

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);

protected:
    void keyPressEvent(QKeyEvent *ke) override;

private:
    void appendToModel();
    void showPopup();

    QStringListModel m_filterModel;
    QIcon m_deleteIcon;
    int m_maximumHistorySize;
    bool m_favoritesMode = false;
    QMetaObject::Connection m_connection;

    friend class HistoryViewDelegate;
    friend class HistoryView;
};
