// Copyright (C) 2004-2026 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#include <QVariantAnimation>
#include <QGuiApplication>
#include <QScreen>
#include <QEvent>
#include <QLayout>

#include "expandbutton.h"


ExpandButton::ExpandButton(QWidget *parent)
    : QToolButton(parent)
    , m_animation(new QVariantAnimation(this))
{
    setCheckable(true);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setArrowType(Qt::RightArrow);
    setStyleSheet(u"background:none; border:none"_qs);
    connect(this, &QToolButton::toggled, [this](bool b) {
        setExpanded(b);
    });

    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    m_animation->setStartValue(0);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        int h = value.toInt();

        QWidget *tlw = m_resizeTopLevel ? window() : nullptr;

        if (tlw && (m_animation->direction() == QAbstractAnimation::Forward)) {
            tlw->resize(QSize { tlw->size().width(), m_topLevelHeightNonExpanded + h });
            int ydelta = tlw->frameGeometry().bottom() - tlw->screen()->availableGeometry().bottom();
            if (ydelta > 0)
                tlw->move(tlw->x(), tlw->y() - ydelta);
        }

        if (m_expand)
            m_expand->setMaximumHeight(h);

        if (tlw && (m_animation->direction() == QAbstractAnimation::Backward))
            tlw->resize(QSize { tlw->size().width(), m_topLevelHeightNonExpanded + h });
    });

    fontChange();
}

void ExpandButton::setExpandingWidget(QWidget *widget)
{
    m_expand = widget;

    m_animation->stop();

    updateExpandHeight();

    if (!isChecked())
        widget->setMaximumHeight(0);
}

void ExpandButton::setExpanded(bool expanded)
{
    if (!m_expand || !m_animation)
        return;

    setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);

    bool isRunning = (m_animation->state() == QAbstractAnimation::Running);
    if (isRunning)
        m_animation->pause();
    else if (window()) {
        m_topLevelHeightNonExpanded = window()->size().height();
        if (!expanded)
            m_topLevelHeightNonExpanded -= m_expandHeight;
    }
    m_animation->setDirection(expanded ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
    m_animation->start();
}

void ExpandButton::setResizeTopLevelOnExpand(bool resize)
{
    m_resizeTopLevel = resize;
}

void ExpandButton::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange)
        fontChange();
    QToolButton::changeEvent(e);
}

void ExpandButton::resizeEvent(QResizeEvent *e)
{
    QToolButton::resizeEvent(e);
    updateExpandHeight();
}

void ExpandButton::updateExpandHeight()
{
    if (m_expand) {
        if (m_expand->hasHeightForWidth())
            m_expandHeight = m_expand->heightForWidth(width());
        else
            m_expandHeight = m_expand->sizeHint().height();
    } else {
        m_expandHeight = 0;
    }
    m_animation->setEndValue(m_expandHeight);
}

void ExpandButton::fontChange()
{
    setFont(parentWidget() ? parentWidget()->font() : qApp->font());
}
