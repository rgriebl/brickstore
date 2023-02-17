// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <QWidget>
#include <QIcon>

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
    void cancelAll();
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

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
    std::unique_ptr<QGradient> m_fill;
};
