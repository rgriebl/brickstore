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

#include <QToolButton>

QT_FORWARD_DECLARE_CLASS(QVariantAnimation)


class ExpandButton : public QToolButton
{
public:
    ExpandButton(QWidget *parent = nullptr);

    void setExpandingWidget(QWidget *widget);
    void setExpanded(bool expanded);
    void setResizeTopLevelOnExpand(bool resize);

protected:
    void changeEvent(QEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void fontChange();
    void updateExpandHeight();

    QVariantAnimation *m_animation = nullptr;
    QWidget *m_expand = nullptr;
    int m_expandHeight = 0;
    bool m_resizeTopLevel = false;
    int m_topLevelHeightNonExpanded = 0;
};
