/* Copyright (C) 2004-2005 Robert Griebl. All rights reserved.
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
#ifndef __PROGRESSCIRCLE_H__
#define __PROGRESSCIRCLE_H__

#include <QWidget>
#include <QIcon>

class QGradient;

class ProgressCircle : public QWidget {
    Q_OBJECT
public:
    ProgressCircle(QWidget *parent = 0);

    QSize sizeHint() const;
    QSize minimumSizeHint() const;
    int heightForWidth(int w) const;

    int maximum() const;
    int minimum() const;
    int value() const;
    bool isOnline() const;

    void setToolTipTemplates(const QString &offline, const QString &nothing, const QString &normal);

    void setIcon(const QIcon &icon);
    QIcon icon() const;

public slots:
    void reset();
    void setMaximum(int maximum);
    void setMinimum(int minimum);
    void setRange(int minimum, int maximum);
    void setValue(int value);
    void setOnlineState(bool isOnline);

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent *e);
    void resizeEvent(QResizeEvent *e);

private:
    QString toolTip() const;

    int m_min;
    int m_max;
    int m_value;
    bool m_online;
    QString m_tt_offline;
    QString m_tt_nothing;
    QString m_tt_normal;
    QIcon m_icon;
    QGradient *m_fill;
};

#endif
