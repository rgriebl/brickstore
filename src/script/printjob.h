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

#include <QObject>
#include <QFont>
#include <QColor>
#include <QVariant>
#include <QSizeF>

QT_FORWARD_DECLARE_CLASS(QPaintDevice)
QT_FORWARD_DECLARE_CLASS(QPainter)


class QmlPrintJob;

class QmlPrintPage : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int number READ pageNumber)
    Q_PROPERTY(QFont font READ font WRITE setFont)
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(QColor backgroundColor READ bgColor WRITE setBgColor)
    Q_PROPERTY(int lineStyle READ lineStyle  WRITE setLineStyle)
    Q_PROPERTY(double lineWidth READ lineWidth  WRITE setLineWidth)

public:
    enum LineStyle {
        NoLine,
        SolidLine,
        DashLine,
        DotLine,
        DashDotLine,
        DashDotDotLine
    };
    Q_ENUM(LineStyle)

    enum AlignmentFlag {
        AlignLeft     = Qt::AlignLeft,
        AlignHCenter  = Qt::AlignHCenter,
        AlignRight    = Qt::AlignRight,
        AlignTop      = Qt::AlignTop,
        AlignVCenter  = Qt::AlignVCenter,
        AlignBottom   = Qt::AlignBottom,
        AlignCenter   = Qt::AlignCenter,
        TextDontClip  = Qt::TextDontClip,
        TextWordWrap  = Qt::TextWordWrap,
        TextWrapAnywhere = Qt::TextWrapAnywhere
    };
    Q_DECLARE_FLAGS(Alignment, AlignmentFlag)
    Q_FLAG(Alignment)

    Q_INVOKABLE QSizeF textSize(const QString &text);

    Q_INVOKABLE void drawText(double left, double top, double width, double height,
                              QmlPrintPage::Alignment align, const QString &text);
    Q_INVOKABLE void drawLine(double x1, double y1, double x2, double y2);
    Q_INVOKABLE void drawRect(double left, double top, double width, double height);
    Q_INVOKABLE void drawEllipse(double left, double top, double width, double height);
    Q_INVOKABLE void drawImage(double left, double top, double width, double height,
                               const QImage &image);

public:
    int pageNumber() const;
    QFont font() const;
    QColor color() const;
    QColor bgColor() const;
    int lineStyle() const;
    double lineWidth() const;

    void setFont(const QFont &f);
    void setColor(const QColor &c);
    void setBgColor(const QColor &c);
    void setLineStyle(int linestyle);
    void setLineWidth(double linewidth);

public:
    QmlPrintPage(const QmlPrintJob *job);

    void dump();
    void print(QPainter *p, double scale [2]) const;

private:
    struct Cmd {
        enum {
            Attributes,
            Text,
            Line,
            Rect,
            Ellipse,
            Image
        } m_cmd;
    };

    struct AttrCmd : public Cmd {
        QFont   m_font;
        QColor  m_color;
        QColor  m_bgcolor;
        int     m_linestyle;
        double  m_linewidth;
    };

    struct DrawCmd : public Cmd {
        double    m_x, m_y, m_w, m_h;
        QVariant  m_p1;
        QVariant  m_p2;
    };

    void attr_cmd();

private:
    QVector<Cmd *> m_cmds;
    const QmlPrintJob *m_job;
    AttrCmd m_attr;
};


class QmlPrintJob : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
//  Q_PROPERTY(int paperFormat READ paperFormat)
    Q_PROPERTY(QSizeF paperSize READ paperSize CONSTANT)

public:
    Q_INVOKABLE QmlPrintPage *addPage();
    Q_INVOKABLE QmlPrintPage *getPage(int i) const;
    Q_INVOKABLE void abort();

    int pageCount() const;
    QSizeF paperSize() const;
    bool isAborted() const;

signals:
    void pageCountChanged(int pages);

public:
    QmlPrintJob(QPaintDevice *pd);
    ~QmlPrintJob() override;

    QPaintDevice *paintDevice() const;

    bool print(int from, int to);
    void dump();

private:
    QVector<QmlPrintPage *> m_pages;
    QPaintDevice *m_pd;
    bool m_aborted = false;
};

Q_DECLARE_METATYPE(QmlPrintPage *)
Q_DECLARE_METATYPE(QmlPrintPage::Alignment)
Q_DECLARE_METATYPE(QmlPrintPage::LineStyle)
Q_DECLARE_METATYPE(QmlPrintJob *)
