/* Copyright (C) 2004-2022 Robert Griebl.All rights reserved.
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

#include <QToolTip>
#include <QPainter>
#include <QConicalGradient>
#include <QMenu>
#include <QContextMenuEvent>
#include <QAction>

#include <limits>

#include "utility/utility.h"
#include "progresscircle.h"


ProgressCircle::ProgressCircle(QWidget *parent)
    : QWidget(parent)
    , m_min(0)
    , m_max(100)
    , m_value(-1)
    , m_online(false)
    , m_tt_normal("%p%"_l1)
{
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    setToolTip(toolTip());
}

int ProgressCircle::heightForWidth(int w) const
{
    return w;
}

QSize ProgressCircle::minimumSizeHint() const
{
    return { 22, 22 };
}

QSize ProgressCircle::sizeHint() const
{
    return { 32, 32 };
}

int ProgressCircle::minimum() const
{
    return m_min;
}

int ProgressCircle::maximum() const
{
    return m_max;
}

int ProgressCircle::value() const
{
    return m_value;
}

void ProgressCircle::setMinimum(int mini)
{
    setRange(mini, qMax(mini, m_max));
}

void ProgressCircle::setMaximum(int maxi)
{
    setRange(qMin(maxi, m_min), maxi);
}

void ProgressCircle::setRange(int mini, int maxi)
{
    m_min = mini;
    m_max = qMax(mini, maxi);
    if (m_value < (m_min - 1) || m_value > m_max)
        reset();
}

void ProgressCircle::setValue(int value)
{
    if ((value == m_value) || ((value > m_max || value < m_min) && (m_max || m_min)))
        return;
    m_value = value;
    emit valueChanged(value);
    setToolTip(toolTip());
    update();
}

void ProgressCircle::reset()
{
    if (m_min == std::numeric_limits<int>::min())
        m_value = std::numeric_limits<int>::min();
    else
        m_value = m_min - 1;
    setToolTip(toolTip());
    update();
}

void ProgressCircle::resizeEvent(QResizeEvent *)
{
    m_fill.reset();
}

void ProgressCircle::mousePressEvent(QMouseEvent *e)
{
    QMenu *m = new QMenu(this);
    connect(m, &QMenu::aboutToHide, m, &QObject::deleteLater);
    auto *a = m->addAction(tr("Cancel all active downloads"));
    a->setEnabled(m_online && ((m_value >= m_min) && (m_value < m_max)));
    connect(a, &QAction::triggered, this, &ProgressCircle::cancelAll);
    m->popup(e->globalPosition().toPoint());
}

void ProgressCircle::paintEvent(QPaintEvent *)
{
    qreal s = qMin(width(), height());
    qreal dx = (width() - s) / 2;
    qreal dy = (height() - s) / 2;
    QRectF r(dx, dy, s, s);
    r.adjust(1, 1, -1, -1);

    QColor outColor = palette().color(m_online ? QPalette::Active
                                               : QPalette::Disabled, QPalette::Text);
    int fromAngle = 0, sweepAngle = 0;
    bool inactive = false;

    if (m_min == 0 && m_max == 0) {
        fromAngle = (90 - (m_value - m_min)) * 16;
        sweepAngle = 22 * 16;
    } else if ((m_value == (m_min - 1)) || (m_value == std::numeric_limits<int>::min()
                                            && m_min == std::numeric_limits<int>::min())) {
        inactive = true;
    } else if (m_max - m_min) {
        fromAngle = 90 * 16;
        sweepAngle = (360 * 16 * (m_value - m_min)) / (m_max - m_min);
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    if (sweepAngle) {
        if (!m_fill) {
            m_fill.reset(new QConicalGradient(r.center(), qreal(45)));
            m_fill->setColorAt(0, m_color);
            m_fill->setColorAt(0.25, m_color.lighter());
            m_fill->setColorAt(0.5, m_color);
            m_fill->setColorAt(0.75, m_color.darker());
            m_fill->setColorAt(1, m_color);
        }
        p.setBrush(*m_fill);
        p.drawPie(r, fromAngle, -sweepAngle);
    }

    p.setBrush(Qt::NoBrush);
    outColor.setAlphaF(outColor.alphaF() * (inactive ? .5f : .8f));
    p.setPen(outColor);

    if (m_icon.isNull() || !inactive) {
        p.drawEllipse(r.center(), s / 2 - 2, s / 2 - 2);
    }
    if (!m_icon.isNull()) {
        int d = inactive ? 0 : int(s / 4 - 1);
        m_icon.paint(&p, r.adjusted(d, d, -d, -d).toRect(), Qt::AlignCenter,
                     m_online ? QIcon::Normal : QIcon::Disabled, QIcon::On);
    }
}

void ProgressCircle::setIcon(const QIcon &icon)
{
    m_icon = icon;
    update();
}

QIcon ProgressCircle::icon() const
{
    return m_icon;
}

void ProgressCircle::setColor(const QColor &color)
{
    m_color = color;
    update();
}

QColor ProgressCircle::color() const
{
    return m_color;
}

void ProgressCircle::setToolTipTemplates(const QString &offline, const QString &nothing, const QString &normal)
{
    m_tt_offline = offline;
    m_tt_nothing = nothing;
    m_tt_normal = normal;

    setToolTip(toolTip());
}

QString ProgressCircle::toolTip() const
{
    if (!m_online)
        return m_tt_offline;

    if ((m_max == 0 && m_min == 0) ||
        (m_value < m_min) ||
        (m_value == std::numeric_limits<int>::min() && m_min == std::numeric_limits<int>::min()))
        return m_tt_nothing;

    qint64 totalSteps = qint64(m_max) - m_min;

    QString result = m_tt_normal;
    result.replace("%m"_l1, QString::number(totalSteps));
    result.replace("%v"_l1, QString::number(m_value));

    // If max and min are equal and we get this far, it means that the
    // progress bar has one step and that we are on that step. Return
    // 100% here in order to avoid division by zero further down.
    if (totalSteps == 0) {
        result.replace("%p"_l1, QString::number(100));
        return result;
    }

    int progress = (m_value - m_min) * 100 / totalSteps;
    result.replace("%p"_l1, QString::number(progress));
    return result;
}

bool ProgressCircle::isOnline() const
{
    return m_online;
}

void ProgressCircle::setOnlineState(bool isOnline)
{
    if (m_online != isOnline) {
        m_online = isOnline;
        update();
        setToolTip(toolTip());
    }
}

#include "moc_progresscircle.cpp"
