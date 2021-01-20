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

#include <QWidget>
#include <QIcon>
#include <QScopedPointer>

QT_FORWARD_DECLARE_CLASS(QGradient)


class ProgressCircle : public QWidget
{
    Q_OBJECT
public:
    ProgressCircle(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    int heightForWidth(int w) const override;

    int maximum() const;
    int minimum() const;
    int value() const;
    bool isOnline() const;

    void setToolTipTemplates(const QString &offline, const QString &nothing, const QString &normal);

    void setIcon(const QIcon &icon);
    QIcon icon() const;

    void setColor(const QColor &color);
    QColor color() const;

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
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

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
    QColor m_color;
    QScopedPointer<QGradient> m_fill;
};
