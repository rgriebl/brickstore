/* Copyright (C) 2004-2005 Robert Griebl.All rights reserved.
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

#include <QProgressBar>
#include <QToolButton>
#include <QToolTip>

#include "cmultiprogressbar.h"


CMultiProgressBar::CMultiProgressBar(QWidget *parent)
    : QWidget(parent)
{
    m_autoid = -1;

    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);

    m_stop = new QToolButton(this);
    m_stop->setAutoRaise(true);
    m_stop->setAutoRepeat(false);
    m_stop->hide();

    connect(m_stop, SIGNAL(clicked()), this, SIGNAL(stop()));
    recalc();

    languageChange();
}

void CMultiProgressBar::languageChange()
{
    m_stop->setToolTip(tr("Cancel all active transfers"));
}

CMultiProgressBar::~CMultiProgressBar()
{
    m_items.clear();
}

void CMultiProgressBar::setStopIcon(const QIcon &ico)
{
    m_stop_ico = ico;
    m_stop->setHidden(ico.isNull());
}

void CMultiProgressBar::resizeEvent(QResizeEvent *)
{
    int h = height();
    int w = width();
    bool too_small = (w < h);
    
    m_stop->setHidden(too_small);
    
    if (too_small) {
        m_progress->setGeometry(0, 0, w, h);
    }
    else {
        m_progress->setGeometry(0, 0, w - h, h);
        m_stop->setGeometry(w - h, 0, h, h);
    }
}


void CMultiProgressBar::recalc()
{
    int ta = 0, pa = 0;
    QStringList sl;
    QString str;

    foreach (const ItemData &id, m_items) {
        ta += id.m_total;
        pa += id.m_progress;

        if (id.m_total > 0) {
            sl << QString("<tr><td><b>%1:</b></td><td align=\"right\">%2%</td><td>(%3/%4)</td></tr>").
            arg(id.m_label).arg(int(100.f * float(id.m_progress) / float(id.m_total))).
            arg(id.m_progress).arg(id.m_total);
        }
    }
    if (ta > 0) {
        m_progress->setMaximum(ta);
        m_progress->setValue(pa);
        str = "<table>" + sl.join("") + "</table>";

        if (!m_progress->isVisible())
            m_progress->show();

        m_progress->setToolTip(str);
    }
    else {
        m_progress->hide();
        m_progress->reset();
    }

    m_stop->setEnabled(ta > 0);
    emit statusChange(ta > 0);
}

CMultiProgressBar::ItemData::ItemData(const QString &label)
{
    m_label = label;
    m_progress = 0;
    m_total = 0;
}

int CMultiProgressBar::addItem(const QString &label, int id)
{
    if (id < 0)
        id = --m_autoid;

    m_items.insert(id, ItemData(label));

    recalc();
    return id;
}

void CMultiProgressBar::removeItem(int id)
{
    QMap<int, ItemData>::iterator it = m_items.find(id);
    
    if (it != m_items.end()) {
        m_items.erase(it);
        recalc();
    }
}

QString CMultiProgressBar::itemLabel(int id) const
{
    QMap<int, ItemData>::const_iterator it = m_items.find(id);
    
    return (it != m_items.end()) ? it->m_label : QString();
}

int CMultiProgressBar::itemProgress(int id) const
{
    QMap<int, ItemData>::const_iterator it = m_items.find(id);
    
    return (it != m_items.end()) ? it->m_progress : -1;
}

int CMultiProgressBar::itemTotalSteps(int id) const
{
    QMap<int, ItemData>::const_iterator it = m_items.find(id);
    
    return (it != m_items.end()) ? it->m_total : -1;
}

void CMultiProgressBar::setItemProgress(int id, int progress)
{
    QMap<int, ItemData>::iterator it = m_items.find(id);
    
    if (it != m_items.end()) {
        it->m_progress = progress;
        recalc();
    }
}

void CMultiProgressBar::setItemProgress(int id, int progress, int total)
{
    QMap<int, ItemData>::iterator it = m_items.find(id);
    
    if (it != m_items.end()) {
        it->m_progress = progress;
        it->m_total = total;
        recalc();
    }
}

void CMultiProgressBar::setItemTotalSteps(int id, int total)
{
    QMap<int, ItemData>::iterator it = m_items.find(id);
    
    if (it != m_items.end()) {
        it->m_total = total;
        recalc();
    }
}


void CMultiProgressBar::setItemLabel(int id, const QString &label)
{
    QMap<int, ItemData>::iterator it = m_items.find(id);
    
    if (it != m_items.end()) {
        it->m_label = label;
        recalc();
    }
}

