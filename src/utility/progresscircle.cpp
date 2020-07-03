/* Copyright (C) 2004-2020 Robert Griebl.All rights reserved.
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

#include <climits>

#include "progresscircle.h"


ProgressCircle::ProgressCircle(QWidget *parent)
    : QWidget(parent)
    , m_min(0)
    , m_max(100)
    , m_value(-1)
    , m_online(false)
    , m_tt_normal(QLatin1String("%p%"))
{
    QSizePolicy sp;
    sp.setHorizontalPolicy(QSizePolicy::Preferred);
    sp.setVerticalPolicy(QSizePolicy::Preferred);
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
    repaint();
}

void ProgressCircle::reset()
{
    if (m_min == INT_MIN)
        m_value = INT_MIN;
    else
        m_value = m_min - 1;
    setToolTip(toolTip());
    repaint();
}

void ProgressCircle::resizeEvent(QResizeEvent *)
{
    m_fill.reset();
}

void ProgressCircle::paintEvent(QPaintEvent *)
{
    qreal s = qMin(width(), height());
    qreal dx = (width() - s) / 2;
    qreal dy = (height() - s) / 2;
    QRectF r(dx, dy, s, s);
    r.adjust(1, 1, -1, -1);

    QColor outColor = palette().color(m_online ? QPalette::Active : QPalette::Disabled, QPalette::Text);
    QColor inColor = palette().color(m_online ? QPalette::Active : QPalette::Disabled, QPalette::Highlight);
    inColor.setAlphaF(inColor.alphaF() * qreal(0.8));

    int fromAngle = 0, sweepAngle = 0;
    bool inactive = false;

    if (m_min == 0 && m_max == 0) {
        fromAngle = (90 - (m_value - m_min)) * 16;
        sweepAngle = 22 * 16;
    } else if ((m_value == (m_min - 1)) || (m_value == INT_MIN && m_min == INT_MIN)) {
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
            m_fill->setColorAt(0, inColor);
            m_fill->setColorAt(0.25, inColor.lighter());
            m_fill->setColorAt(0.5, inColor);
            m_fill->setColorAt(0.75, inColor.darker());
            m_fill->setColorAt(1, inColor);
        }
        p.setBrush(*m_fill);
        p.drawPie(r, fromAngle, -sweepAngle);
    }

    p.setBrush(Qt::NoBrush);
    outColor.setAlphaF(outColor.alphaF() * qreal(inactive ? 0.5 : 0.8));
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
        (m_value == INT_MIN && m_min == INT_MIN))
        return m_tt_nothing;

    qint64 totalSteps = qint64(m_max) - m_min;

    QString result = m_tt_normal;
    result.replace(QLatin1String("%m"), QString::number(totalSteps));
    result.replace(QLatin1String("%v"), QString::number(m_value));

    // If max and min are equal and we get this far, it means that the
    // progress bar has one step and that we are on that step. Return
    // 100% here in order to avoid division by zero further down.
    if (totalSteps == 0) {
        result.replace(QLatin1String("%p"), QString::number(100));
        return result;
    }

    int progress = (m_value - m_min) * 100 / totalSteps;
    result.replace(QLatin1String("%p"), QString::number(progress));
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
        repaint();
        setToolTip(toolTip());
    }
}

#include "moc_progresscircle.cpp"
