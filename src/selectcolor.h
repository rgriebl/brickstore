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

#include <QDialog>

#include "bricklinkfwd.h"

QT_FORWARD_DECLARE_CLASS(QTreeView)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QToolButton)


class SelectColor : public QWidget
{
    Q_OBJECT
public:
    SelectColor(QWidget *parent = nullptr);

    void setWidthToContents(bool b);

    void setCurrentColor(const BrickLink::Color *color);
    void setCurrentColorAndItem(const BrickLink::Color *color, const BrickLink::Item *item);
    const BrickLink::Color *currentColor() const;

    bool colorLock() const;
    void unlockColor();

    QByteArray saveState() const;
    bool restoreState(const QByteArray &ba);
    static QByteArray defaultState();

signals:
    void colorSelected(const BrickLink::Color *color, bool confirmed);
    void colorLockChanged(const BrickLink::Color *color);

protected slots:
    void colorChanged();
    void colorConfirmed();
    void updateColorFilter(int filter);
    void languageChange();

protected:
    void changeEvent(QEvent *) override;
    void showEvent(QShowEvent *) override;

    void populateFilter(const BrickLink::Color *color);

protected:
    QComboBox *w_filter;
    QTreeView *w_colors;
    QToolButton *w_lock;
    BrickLink::ColorModel *m_colorModel;
    const BrickLink::Item *m_item = nullptr;
};
