// Copyright (C) 2004-2024 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

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
