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

#include <QLayout>
#include <QRect>
#include <QVector>

// based on Qt's flowlayout example

class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent, int margin = -1, int spacing = -1);
    explicit FlowLayout(int margin = -1, int spacing = -1);
    ~FlowLayout() override;

    int spacing() const; //Qt6 override;

    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    void addItem(QLayoutItem *item) override;

    bool hasHeightForWidth() const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;

    void setGeometry(const QRect &rect) override;

private:
    QVector<QLayoutItem *> m_items;
    int m_space;
};
